#include "mf-toolkit.h"
#include "common/string-utils.h"
#include <mfapi.h>
#include <mfobjects.h>
#include <icodecapi.h>
#include <mftransform.h>
#include <Mferror.h>

typedef struct mf_video_encoder_s {
	IMFMediaEventGenerator* p_gen;
	ICodecAPI* p_codec_api;
	IMFTransform* p_trans;
	int in_reqs;
	int out_reqs;
	DWORD in_stm_id;
	DWORD out_stm_id;
}mf_video_encoder_t;

static mf_video_encoder_t encoder_;
static atomic_flag inited_ = ATOMIC_FLAG_INIT;

static void _mf_create() {
	HRESULT hr = S_OK;
	if (FAILED(hr = MFStartup(MF_VERSION, MFSTARTUP_FULL))) {
		cdk_loge("Failed to MFStartup: 0x%x.\n", hr);
		return;
	}
}

static void _mf_destroy() {
	MFShutdown();
}

static uint64_t _mf_generate_timestamp() {
	uint64_t now = cdk_time_now();
	/** in 100-nanosecond units. */
	return now * 10000;
}

static HRESULT _mf_hw_video_drain_event(bool block) {
	HRESULT hr, status;
	IMFMediaEvent* p_event;
	MediaEventType type;

	hr = IMFMediaEventGenerator_GetEvent(encoder_.p_gen, block ? 0 : MF_EVENT_FLAG_NO_WAIT, &p_event);
	if (FAILED(hr)) {
		cdk_loge("Failed to IMFMediaEventGenerator_GetEvent: 0x%x.\n", hr);
		return -1;
	}
	IMFMediaEvent_GetType(p_event, &type);
	IMFMediaEvent_GetStatus(p_event, &status);

	if (hr != MF_E_NO_EVENTS_AVAILABLE && FAILED(hr)) {
		goto fail;
	}
	if (hr == MF_E_NO_EVENTS_AVAILABLE) {
		IMFMediaEvent_Release(p_event);
		return hr;
	}
	if (SUCCEEDED(status)) {
		if (type == METransformNeedInput) {
			encoder_.in_reqs++;
		}
		else if (type == METransformHaveOutput) {
			encoder_.out_reqs++;
		}
	}
	IMFMediaEvent_Release(p_event);
	return S_OK;

fail:
	IMFMediaEvent_Release(p_event);
	return hr;
}

static HRESULT _mf_hw_video_drain_events() {
	HRESULT hr;
	while ((hr = _mf_hw_video_drain_event(false)) == S_OK);
	if (hr == MF_E_NO_EVENTS_AVAILABLE) {
		hr = S_OK;
	}
	return hr;
}

static bool _mf_hw_video_enqueue_sample(IMFSample* p_sample) {
	



	if (!encoder_.async_need_input) {
		return -EAGAIN;
	}
	if (FAILED(hr = IMFTransform_ProcessInput(encoder_.p_trans, encoder_.in_stm_id, p_sample, 0))) {
		if (hr == MF_E_NOTACCEPTING) {
			return -EAGAIN;
		}
		cdk_loge("Failed to IMFTransform_ProcessInput: 0x%x.\n", hr);
		return -1;
	}
	encoder_.async_need_input = false;
	return 0;

fail:

}

static int _mf_hw_video_dequeue_sample(IMFSample** pp_sample) {
	HRESULT hr = S_OK;
	MFT_OUTPUT_DATA_BUFFER outbuf = {
		.dwStreamID = encoder_.out_stm_id,
		.dwStatus = 0,
		.pEvents = NULL,
		.pSample = *pp_sample
	};
	IMFMediaType* p_type = NULL;

	while (true) {
		if (_mf_hw_video_wait_events() < 0) {
			return -1;
		}
		if (!encoder_.async_have_output) {
			return -EAGAIN;
		}
		DWORD status;
		if (FAILED(hr = IMFTransform_ProcessOutput(encoder_.p_trans, 0, 1, &outbuf, &status))) {
			if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
				return -EAGAIN;
			}
			if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
				IMFTransform_GetOutputAvailableType(encoder_.p_trans, 0, 0, &p_type);
				IMFTransform_SetOutputType(encoder_.p_trans, 0, p_type, 0);

				encoder_.async_have_output = 0;
				continue;
			}
			cdk_loge("Failed to IMFTransform_ProcessOutput: 0x%x.\n", hr);
			return -1;
		}
		SAFE_RELEASE(outbuf.pEvents);
		
		break;
	}
	encoder_.async_have_output = false;
	return 0;
}

/**
 * MFSetAttributeSize is missing when using MFT in C.
 */
static HRESULT MFSetAttributeSize(IMFAttributes* p_attr, REFGUID guid, UINT32 width, UINT32 height) {
	UINT64 t = (((UINT64)width) << 32) | height;
	return IMFAttributes_SetUINT64(p_attr, guid, t);
}

#define MFSetAttributeRatio MFSetAttributeSize

void mf_hw_video_encoder_create(int bitrate, int framerate, int width, int height) {
	HRESULT hr = S_OK;
	IMFActivate** pp_activate = NULL;
	uint32_t num_activate = 0;
	IMFAttributes* p_attrs = NULL;
	IMFActivate* p_activate = NULL;
	DWORD in_stm;
	DWORD out_stm;
	
	IMFMediaType* p_in_type = NULL;
	IMFMediaType* p_out_type = NULL; 

	MFT_REGISTER_TYPE_INFO in_type = { MFMediaType_Video, MFVideoFormat_NV12 };
	MFT_REGISTER_TYPE_INFO out_type = { MFMediaType_Video, MFVideoFormat_HEVC };

	char encoder_name[128] = { 0 };

	if (!atomic_flag_test_and_set(&inited_)) {
		_mf_create();
	}
	if (FAILED(hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER, &in_type, &out_type, &pp_activate, &num_activate))) {
		cdk_loge("Failed to MFTEnumEx: 0x%x.\n", hr);
		return;
	}
	if (!num_activate) {
		cdk_loge("No available encoder.\n");
		goto fail;
	}
	for (uint32_t i = 0; i < num_activate; i++) {
		if (FAILED(hr = IMFActivate_ActivateObject(pp_activate[i], &IID_IMFTransform, &encoder_.p_trans))) {
			cdk_loge("Failed to IMFActivate_ActivateObject: 0x%x.\n", hr);
			goto fail;
		}
		if (encoder_.p_trans) {
			p_activate = pp_activate[i];
			IMFActivate_AddRef(p_activate);
			break;
		}
	}
	for (uint32_t i = 0; i < num_activate; i++) {
		IMFActivate_Release(pp_activate[i]);
	}
	CoTaskMemFree(pp_activate);

	if (FAILED(hr = IMFTransform_GetAttributes(encoder_.p_trans, &p_attrs))) {
		cdk_loge("Failed to IMFTransform_GetAttributes: 0x%x.\n", hr);
		goto fail;
	}
	wchar_t tmp[128] = { 0 };
	IMFActivate_GetString(p_activate, &MFT_FRIENDLY_NAME_Attribute, tmp, sizeof(tmp), NULL);

	string_utils_wide2narrow(tmp, encoder_name, sizeof(encoder_name) - 1);
	cdk_logi("%s is available.\n", encoder_name);
	IMFActivate_Release(p_activate);

	uint32_t val;
	if (FAILED(hr = IMFAttributes_GetUINT32(p_attrs, &MF_TRANSFORM_ASYNC, &val))) {
		cdk_loge("Failed to get MF_TRANSFORM_ASYNC: 0x%x.\n", hr);
		goto fail;

	}
	if (!val) {
		cdk_loge("hardware MFT is not async\n");
		goto fail;

	}
	if (FAILED(hr = IMFAttributes_SetUINT32(p_attrs, &MF_TRANSFORM_ASYNC_UNLOCK, true))) {
		cdk_loge("Failed to MF_TRANSFORM_ASYNC_UNLOCK: 0x%x.\n", hr);
		goto fail;
	}
	if (FAILED(hr = IMFTransform_QueryInterface(encoder_.p_trans, &IID_IMFMediaEventGenerator, &encoder_.p_gen))) {
		cdk_loge("Failed to query IMFMediaEventGenerator: 0x%x.\n", hr);
		goto fail;
	}
	if (FAILED(hr = IMFTransform_QueryInterface(encoder_.p_trans, &IID_ICodecAPI, &encoder_.p_codec_api))) {
		cdk_loge("Failed to query ICodecAPI: 0x%x.\n", hr);
		goto fail;
	}
	if (FAILED(hr = IMFTransform_GetStreamIDs(encoder_.p_trans, 1, &in_stm, 1, &out_stm))) {
		if (hr == E_NOTIMPL)
		{
			in_stm = 0;
			out_stm = 0;
			hr = S_OK;
		}
		else {
			cdk_loge("Failed to IMFTransform_GetStreamIDs: 0x%x.\n", hr);
			goto fail;
		}
	}
	encoder_.in_stm_id = in_stm;
	encoder_.out_stm_id = out_stm;

	if (FAILED(hr = IMFAttributes_SetUINT32(p_attrs, &MF_LOW_LATENCY, true))) {
		cdk_loge("Failed to set MF_LOW_LATENCY: 0x%x.\n", hr);
		goto fail;
	}
	if (FAILED(hr = MFCreateMediaType(&p_out_type))) {
		cdk_loge("Failed to MFCreateMediaType: 0x%x.\n", hr);
		goto fail;
	}
	IMFMediaType_SetGUID(p_out_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
	IMFMediaType_SetGUID(p_out_type, &MF_MT_SUBTYPE, &MFVideoFormat_HEVC);
	IMFMediaType_SetUINT32(p_out_type, &MF_MT_AVG_BITRATE, bitrate);
	MFSetAttributeSize((IMFAttributes*)p_out_type, &MF_MT_FRAME_SIZE, width, height);
	MFSetAttributeRatio((IMFAttributes*)p_out_type, &MF_MT_FRAME_RATE, framerate, 1);
	IMFMediaType_SetUINT32(p_out_type, &MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	IMFMediaType_SetUINT32(p_out_type, &MF_MT_ALL_SAMPLES_INDEPENDENT, true);

	if (FAILED(hr = IMFTransform_SetOutputType(encoder_.p_trans, out_stm, p_out_type, 0))) {
		cdk_loge("Failed to IMFTransform_SetOutputType: 0x%x.\n", hr);
		goto fail;
	}
	if (FAILED(hr = MFCreateMediaType(&p_in_type))) {
		cdk_loge("Failed to MFCreateMediaType: 0x%x.\n", hr);
		goto fail;
	}
	for (DWORD i = 0;; i++)
	{
		if (FAILED(hr = IMFTransform_GetInputAvailableType(encoder_.p_trans, in_stm, i, &p_in_type))) {
			cdk_loge("Failed to IMFTransform_GetInputAvailableType: 0x%x.\n", hr);
			goto fail;
		}
		IMFMediaType_SetGUID(p_in_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
		IMFMediaType_SetGUID(p_in_type, &MF_MT_SUBTYPE, &MFVideoFormat_NV12);
		MFSetAttributeSize((IMFAttributes*)p_in_type, &MF_MT_FRAME_SIZE, width, height);
		//MFSetAttributeRatio((IMFAttributes*)p_in_type, &MF_MT_FRAME_RATE, framerate, 1);

		if (FAILED(hr = IMFTransform_SetInputType(encoder_.p_trans, in_stm, p_in_type, 0))) {
			cdk_loge("Failed to IMFTransform_SetInputType: 0x%x.\n", hr);
			goto fail;
		}
		break;
	}
	if (FAILED(hr = IMFTransform_ProcessMessage(encoder_.p_trans, MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0))) {
		cdk_loge("Failed to start streaming: 0x%x.\n", hr);
		goto fail;
	}
	if (FAILED(hr = IMFTransform_ProcessMessage(encoder_.p_trans, MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0))) {
		cdk_loge("Failed to start stream: 0x%x.\n", hr);
		goto fail;
	}
	SAFE_RELEASE(p_in_type);
	SAFE_RELEASE(p_out_type);
	SAFE_RELEASE(p_attrs);
	return;
fail:
	for (uint32_t i = 0; i < num_activate; i++) {
		SAFE_RELEASE(pp_activate[i]);
	}
	CoTaskMemFree(pp_activate);

	SAFE_RELEASE(p_in_type);
	SAFE_RELEASE(p_out_type);
	SAFE_RELEASE(p_attrs);
	SAFE_RELEASE(p_activate);

	SAFE_RELEASE(encoder_.p_gen);
	SAFE_RELEASE(encoder_.p_codec_api);
	SAFE_RELEASE(encoder_.p_trans);
}

void mf_hw_video_encoder_destroy() {
	IMFTransform_ProcessMessage(encoder_.p_trans, MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
	IMFTransform_ProcessMessage(encoder_.p_trans, MFT_MESSAGE_NOTIFY_END_STREAMING, 0);

	SAFE_RELEASE(encoder_.p_gen);
	SAFE_RELEASE(encoder_.p_codec_api);
	SAFE_RELEASE(encoder_.p_trans);
	_mf_destroy();
}

void mf_hw_video_encode(ID3D11Texture2D* p_indata, video_frame_t* p_outdata) {
	HRESULT hr = S_OK;
	IMFMediaBuffer* p_inbuf = NULL;
	IMFMediaBuffer* p_outbuf = NULL;
	IMFSample* p_in_sample = NULL;
	IMFSample* p_out_sample = NULL;
	uint64_t pts = 0;

	if (FAILED(hr = MFCreateDXGISurfaceBuffer(&IID_ID3D11Texture2D, (IUnknown*)p_indata, 0, FALSE, &p_inbuf))) {
		cdk_loge("Failed to MFCreateDXGISurfaceBuffer: 0x%x.\n", hr);
		return;
	}
	if (FAILED(hr = MFCreateSample(&p_in_sample))) {
		cdk_loge("Failed to MFCreateSample: 0x%x.\n", hr);
		return;
	}
	hr = IMFSample_AddBuffer(p_in_sample, p_inbuf);
	IMFMediaBuffer_Release(p_inbuf);

	pts = _mf_generate_timestamp();
	hr = IMFSample_SetSampleTime(p_in_sample, (LONGLONG)pts);

	HRESULT hr = S_OK;
	if (FAILED(hr = _mf_hw_video_wait_events())) {
		cdk_loge("Failed to _mf_hw_video_wait_events: 0x%x.\n", hr);
		return false;
	}
	while (encoder_.out_reqs > 0 && (hr = ProcessOutput()) == S_OK);

	if (hr != MF_E_TRANSFORM_NEED_MORE_INPUT && FAILED(hr)) {
		MF_LOG_COM(LOG_ERROR, "ProcessOutput()", hr);
		goto fail;
	}
	while (inputRequests == 0) {
		hr = DrainEvent(false);
		if (hr == MF_E_NO_EVENTS_AVAILABLE) {
			Sleep(1);
			continue;
		}
		if (FAILED(hr)) {
			MF_LOG_COM(LOG_ERROR, "DrainEvent()", hr);
			goto fail;
		}
		if (outputRequests > 0) {
			hr = ProcessOutput();
			if (hr != MF_E_TRANSFORM_NEED_MORE_INPUT &&
				FAILED(hr))
				goto fail;
		}
	}
	HRC(ProcessInput(sample));

	pendingRequests++;

	*status = SUCCESS;
	return true;





	while (true) {
		int ret = _mf_hw_video_enqueue_sample(p_in_sample);
		if (p_in_sample) {
			IMFSample_Release(p_in_sample);
		}
		if (ret < 0 && ret != -EAGAIN) {
			return;
		}
		ret = _mf_hw_video_dequeue_sample(&p_out_sample);
		if (ret < 0 && ret != -EAGAIN) {
			return;
		}
		if (ret == 0) {
			break;
		}
	}
	IMFSample_GetTotalLength(p_out_sample, (DWORD*)&p_outdata->bslen);
	IMFSample_ConvertToContiguousBuffer(p_out_sample, &p_outbuf);

	p_outdata->bitstream = malloc(p_outdata->bslen);
	if (!p_outdata->bitstream) {
		SAFE_RELEASE(p_outbuf);
		SAFE_RELEASE(p_in_sample);
		SAFE_RELEASE(p_out_sample);
		return;
	}
	if (FAILED(hr = IMFMediaBuffer_Lock(p_outbuf, &p_outdata->bitstream, NULL, NULL))) {
		cdk_loge("Failed to IMFMediaBuffer_Lock: 0x%x.\n", hr);

		free(p_outdata->bitstream);
		p_outdata->bitstream = NULL;
		SAFE_RELEASE(p_outbuf);
		SAFE_RELEASE(p_in_sample);
		SAFE_RELEASE(p_out_sample);
		return;
	}
	IMFMediaBuffer_Unlock(p_outbuf);
	IMFMediaBuffer_Release(p_outbuf);

	p_outdata->dts = pts;
	p_outdata->pts = pts;

	SAFE_RELEASE(p_in_sample);
	SAFE_RELEASE(p_out_sample);
}