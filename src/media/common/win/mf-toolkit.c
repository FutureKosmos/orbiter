#include "mf-toolkit.h"

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

static int _mf_wait_events(mf_video_encoder_t* p_encoder) {
	while (!(p_encoder->async_need_input || p_encoder->async_have_output)) {
		HRESULT hr = S_OK;
		IMFMediaEvent* p_event = NULL;
		MediaEventType type;

		hr = IMFMediaEventGenerator_GetEvent(p_encoder->p_gen, 0, &p_event);
		if (FAILED(hr)) {
			cdk_loge("Failed to IMFMediaEventGenerator_GetEvent: 0x%x.\n", hr);
			return -1;
		}
		IMFMediaEvent_GetType(p_event, &type);

		switch (type) {
		case METransformNeedInput:
			p_encoder->async_need_input = true;
			break;
		case METransformHaveOutput:
			p_encoder->async_have_output = true;
			break;
		default:;
		}
		IMFMediaEvent_Release(p_event);
	}
	return 0;
}

static int _mf_hw_video_enqueue_sample(mf_video_encoder_t* p_encoder, IMFSample* p_sample) {
	int ret = 0;
	HRESULT hr = S_OK;
	
	if ((ret = _mf_wait_events(p_encoder)) < 0) {
		return ret;
	}
	if (!p_encoder->async_need_input) {
		return -EAGAIN;
	}
	if (FAILED(hr = IMFTransform_ProcessInput(p_encoder->p_trans, p_encoder->in_stm_id, p_sample, 0))) {
		if (hr == MF_E_NOTACCEPTING) {
			return -EAGAIN;
		}
		cdk_loge("Failed to IMFTransform_ProcessInput: 0x%x.\n", hr);
		return -1;
	}
	p_encoder->async_need_input = false;
	return 0;
}

static int _mf_hw_video_dequeue_sample(mf_video_encoder_t* p_encoder, IMFSample** pp_sample) {
	HRESULT hr = S_OK;
	MFT_OUTPUT_DATA_BUFFER outbuf = {
		.dwStreamID = p_encoder->out_stm_id,
		.dwStatus = 0,
		.pEvents = NULL,
		.pSample = NULL
	};
	int ret;
	while (true) {
		*pp_sample = NULL;
		if (ret = _mf_wait_events(p_encoder) < 0) {
			return ret;
		}
		if (!p_encoder->async_have_output) {
			ret = 0;
			break;
		}
		DWORD status;
		hr = IMFTransform_ProcessOutput(p_encoder->p_trans, 0, 1, &outbuf, &status);
		if (outbuf.pEvents) {
			IMFCollection_Release(outbuf.pEvents);
		}
		if (!FAILED(hr)) {
			*pp_sample = outbuf.pSample;
			ret = 0;
			break;
		}
		if (outbuf.pSample) {
			IMFSample_Release(outbuf.pSample);
		}
		if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
			ret = 0;
		}
		else if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
			cdk_logi("MF_E_TRANSFORM_STREAM_CHANGE: 0x%x.\n", hr);

			IMFMediaType* type;
			IMFTransform_GetOutputAvailableType(p_encoder->p_trans, p_encoder->out_stm_id, 0, &type);
			IMFTransform_SetOutputType(p_encoder->p_trans, p_encoder->out_stm_id, type, 0);
			
			p_encoder->async_have_output = false;
			continue;
		}
		else {
			ret = -1;
		}
		break;
	}
	p_encoder->async_have_output = false;

	if (!*pp_sample) {
		ret = -1;
	}
	return ret;
}

/**
 * below functions and macros are missing when using MFT in C.
 */
static HRESULT MFSetAttributeSize(IMFAttributes* p_attr, REFGUID guid, UINT32 width, UINT32 height) {
	UINT64 t = (((UINT64)width) << 32) | height;
	return IMFAttributes_SetUINT64(p_attr, guid, t);
}

static HRESULT MFGetAttributeSize(IMFAttributes* pattr, REFGUID guid, UINT32* pw, UINT32* ph) {
	UINT64 t;
	HRESULT hr = IMFAttributes_GetUINT64(pattr, guid, &t);
	if (!FAILED(hr)) {
		*pw = t >> 32;
		*ph = (UINT32)t;
	}
	return hr;
}
#define MFSetAttributeRatio MFSetAttributeSize
#define MFGetAttributeRatio MFGetAttributeSize
#define VARIANT_VALUE(type, contents) &(VARIANT){ .vt = (type), contents }
#define VAL_VT_UI4(v) VARIANT_VALUE(VT_UI4, .ulVal = (v))
#define VAL_VT_BOOL(v) VARIANT_VALUE(VT_BOOL, .boolVal = (v))

bool mf_hw_video_encoder_create(directx_device_t* p_device, int bitrate, int framerate, int width, int height, mf_video_encoder_t* p_encoder) {
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

	_mf_create();
	if (FAILED(hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER, &in_type, &out_type, &pp_activate, &num_activate))) {
		cdk_loge("Failed to MFTEnumEx: 0x%x.\n", hr);
		goto fail;
	}
	if (!num_activate) {
		cdk_loge("No available encoder.\n");
		goto fail;
	}
	for (uint32_t i = 0; i < num_activate; i++) {
		if (FAILED(hr = IMFActivate_ActivateObject(pp_activate[i], &IID_IMFTransform, &p_encoder->p_trans))) {
			cdk_loge("Failed to IMFActivate_ActivateObject: 0x%x.\n", hr);
			goto fail;
		}
		if (p_encoder->p_trans) {
			p_activate = pp_activate[i];
			IMFActivate_AddRef(p_activate);
			break;
		}
	}
	for (uint32_t i = 0; i < num_activate; i++) {
		IMFActivate_Release(pp_activate[i]);
	}
	CoTaskMemFree(pp_activate);

	if (FAILED(hr = IMFTransform_GetAttributes(p_encoder->p_trans, &p_attrs))) {
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
	if (FAILED(hr = IMFTransform_QueryInterface(p_encoder->p_trans, &IID_IMFMediaEventGenerator, &p_encoder->p_gen))) {
		cdk_loge("Failed to query IMFMediaEventGenerator: 0x%x.\n", hr);
		goto fail;
	}
	if (FAILED(hr = IMFTransform_QueryInterface(p_encoder->p_trans, &IID_ICodecAPI, &p_encoder->p_codec_api))) {
		cdk_loge("Failed to query ICodecAPI: 0x%x.\n", hr);
		goto fail;
	}
	if (FAILED(hr = IMFTransform_GetStreamIDs(p_encoder->p_trans, 1, &in_stm, 1, &out_stm))) {
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
	p_encoder->in_stm_id = in_stm;
	p_encoder->out_stm_id = out_stm;

	if (FAILED(hr = MFCreateMediaType(&p_out_type))) {
		cdk_loge("Failed to MFCreateMediaType: 0x%x.\n", hr);
		goto fail;
	}
	IMFMediaType_SetGUID(p_out_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
	IMFMediaType_SetGUID(p_out_type, &MF_MT_SUBTYPE, &MFVideoFormat_HEVC);
	IMFMediaType_SetUINT32(p_out_type, &MF_MT_AVG_BITRATE, bitrate);
	IMFAttributes_SetUINT32(p_out_type, &MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Main);
	MFSetAttributeSize((IMFAttributes*)p_out_type, &MF_MT_FRAME_SIZE, width, height);
	MFSetAttributeRatio((IMFAttributes*)p_out_type, &MF_MT_FRAME_RATE, framerate, 1);
	IMFMediaType_SetUINT32(p_out_type, &MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	IMFMediaType_SetUINT32(p_out_type, &MF_MT_ALL_SAMPLES_INDEPENDENT, true);
	IMFAttributes_SetUINT32(p_attrs, &MF_LOW_LATENCY, true);
	
	if (FAILED(hr = ICodecAPI_SetValue(p_encoder->p_codec_api, &CODECAPI_AVEncCommonMeanBitRate, VAL_VT_UI4(bitrate)))) {
		cdk_loge("Failed to set CODECAPI_AVEncCommonMeanBitRate: 0x%x.\n", hr);
		goto fail;
	}
	if (FAILED(hr = ICodecAPI_SetValue(p_encoder->p_codec_api, &CODECAPI_AVEncCommonMaxBitRate, VAL_VT_UI4(bitrate)))) {
		cdk_loge("Failed to set CODECAPI_AVEncCommonMaxBitRate: 0x%x.\n", hr);
		goto fail;
	}
	if (FAILED(hr = ICodecAPI_SetValue(p_encoder->p_codec_api, &CODECAPI_AVEncCommonRateControlMode, VAL_VT_UI4(eAVEncCommonRateControlMode_CBR)))) {
		cdk_loge("Failed to set CODECAPI_AVEncCommonRateControlMode: 0x%x.\n", hr);
		goto fail;
	}
	if (FAILED(hr = ICodecAPI_SetValue(p_encoder->p_codec_api, &CODECAPI_AVEncMPVGOPSize, VAL_VT_UI4(0xffffffff)))) {
		cdk_loge("Failed to set CODECAPI_AVEncMPVGOPSize: 0x%x.\n", hr);
		goto fail;
	}
	if (FAILED(hr = ICodecAPI_SetValue(p_encoder->p_codec_api, &CODECAPI_AVEncVideoForceKeyFrame, VAL_VT_UI4(1)))) {
		cdk_loge("Failed to set CODECAPI_AVEncVideoForceKeyFrame: 0x%x.\n", hr);
		goto fail;
	}
	/**
	 * Based on the current situation, it appears that the Intel HEVC hardware encoder, 
	 * if not configured with this setting, should include B-frames in the encoded video stream. 
	 * This can lead to intermittent stuttering during playback, where the video may lag or freeze alternately.
	 */
	if (p_device->vendor == VENDOR_ID_INTEL) {
		if (FAILED(hr = ICodecAPI_SetValue(p_encoder->p_codec_api, &CODECAPI_AVEncMPVDefaultBPictureCount, VAL_VT_UI4(0)))) {
			cdk_loge("Failed to set CODECAPI_AVEncMPVDefaultBPictureCount: 0x%x.\n", hr);
			goto fail;
		}
	}
	
	if (FAILED(hr = IMFTransform_SetOutputType(p_encoder->p_trans, out_stm, p_out_type, 0))) {
		cdk_loge("Failed to IMFTransform_SetOutputType: 0x%x.\n", hr);
		goto fail;
	}
	if (FAILED(hr = MFCreateMediaType(&p_in_type))) {
		cdk_loge("Failed to MFCreateMediaType: 0x%x.\n", hr);
		goto fail;
	}
	for (DWORD i = 0;; i++)
	{
		if (FAILED(hr = IMFTransform_GetInputAvailableType(p_encoder->p_trans, in_stm, i, &p_in_type))) {
			cdk_loge("Failed to IMFTransform_GetInputAvailableType: 0x%x.\n", hr);
			goto fail;
		}
		IMFMediaType_SetGUID(p_in_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
		IMFMediaType_SetGUID(p_in_type, &MF_MT_SUBTYPE, &MFVideoFormat_NV12);

		if (FAILED(hr = IMFTransform_SetInputType(p_encoder->p_trans, in_stm, p_in_type, 0))) {
			cdk_loge("Failed to IMFTransform_SetInputType: 0x%x.\n", hr);
			goto fail;
		}
		break;
	}
	UINT token;
	if (FAILED(hr = MFCreateDXGIDeviceManager(&token, &p_encoder->p_manager))) {
		cdk_loge("Failed to MFCreateDXGIDeviceManager: 0x%x.\n", hr);
		goto fail;
	}
	if (FAILED(hr = IMFDXGIDeviceManager_ResetDevice(p_encoder->p_manager, (IUnknown*)p_device->p_dev, token))) {
		cdk_loge("Failed to IMFDXGIDeviceManager_ResetDevice: 0x%x.\n", hr);
		goto fail;
	}
	if (FAILED(hr = IMFTransform_ProcessMessage(p_encoder->p_trans, MFT_MESSAGE_SET_D3D_MANAGER, (ULONG_PTR)p_encoder->p_manager))) {
		cdk_loge("Failed to set d3d manager: 0x%x.\n", hr);
		goto fail;
	}
	if (FAILED(hr = IMFTransform_ProcessMessage(p_encoder->p_trans, MFT_MESSAGE_COMMAND_FLUSH, 0))) {
		cdk_loge("Failed to flush encoder: 0x%x.\n", hr);
		goto fail;
	}
	if (FAILED(hr = IMFTransform_ProcessMessage(p_encoder->p_trans, MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0))) {
		cdk_loge("Failed to start streaming: 0x%x.\n", hr);
		goto fail;
	}
	if (FAILED(hr = IMFTransform_ProcessMessage(p_encoder->p_trans, MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0))) {
		cdk_loge("Failed to start stream: 0x%x.\n", hr);
		goto fail;
	}
	SAFE_RELEASE(p_in_type);
	SAFE_RELEASE(p_out_type);
	SAFE_RELEASE(p_attrs);
	return true;
fail:
	for (uint32_t i = 0; i < num_activate; i++) {
		SAFE_RELEASE(pp_activate[i]);
	}
	CoTaskMemFree(pp_activate);

	SAFE_RELEASE(p_in_type);
	SAFE_RELEASE(p_out_type);
	SAFE_RELEASE(p_attrs);
	SAFE_RELEASE(p_activate);

	SAFE_RELEASE(p_encoder->p_manager);
	SAFE_RELEASE(p_encoder->p_gen);
	SAFE_RELEASE(p_encoder->p_codec_api);
	SAFE_RELEASE(p_encoder->p_trans);
	return false;
}

void mf_hw_video_encoder_reconfigure(int bitrate, int framerate, int width, int height) {

}

void mf_hw_video_encoder_destroy(mf_video_encoder_t* p_encoder) {
	IMFTransform_ProcessMessage(p_encoder->p_trans, MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
	IMFTransform_ProcessMessage(p_encoder->p_trans, MFT_MESSAGE_NOTIFY_END_STREAMING, 0);
	IMFTransform_ProcessMessage(p_encoder->p_trans, MFT_MESSAGE_COMMAND_FLUSH, 0);
	IMFTransform_ProcessMessage(p_encoder->p_trans, MFT_MESSAGE_SET_D3D_MANAGER, (ULONG_PTR)0);

	SAFE_RELEASE(p_encoder->p_manager);
	SAFE_RELEASE(p_encoder->p_gen);
	SAFE_RELEASE(p_encoder->p_codec_api);
	SAFE_RELEASE(p_encoder->p_trans);
	_mf_destroy();
}

void mf_hw_video_encode(mf_video_encoder_t* p_encoder, ID3D11Texture2D* p_indata, video_frame_t* p_outdata) {
	HRESULT hr = S_OK;
	IMFMediaBuffer* p_inbuf = NULL;
	IMFMediaBuffer* p_outbuf = NULL;
	IMFSample* p_sample = NULL;
	BYTE* p_data = NULL;
	DWORD len = 0;

	if (FAILED(hr = MFCreateDXGISurfaceBuffer(&IID_ID3D11Texture2D, (IUnknown*)p_indata, 0, FALSE, &p_inbuf))) {
		cdk_loge("Failed to MFCreateDXGISurfaceBuffer: 0x%x.\n", hr);
		return;
	}
	if (FAILED(hr = MFCreateSample(&p_sample))) {
		cdk_loge("Failed to MFCreateSample: 0x%x.\n", hr);

		SAFE_RELEASE(p_inbuf);
		return;
	}
	hr = IMFSample_AddBuffer(p_sample, p_inbuf);
	SAFE_RELEASE(p_inbuf);

	hr = IMFSample_SetSampleTime(p_sample, (LONGLONG)_mf_generate_timestamp());
	
	int ret = _mf_hw_video_enqueue_sample(p_encoder, p_sample);
	if (p_sample) {
		IMFSample_Release(p_sample);
	}
	if (ret < 0 && ret != -EAGAIN) {
		return;
	}
	ret = _mf_hw_video_dequeue_sample(p_encoder, &p_sample);
	if (ret < 0) {
		return;
	}
	IMFSample_GetTotalLength(p_sample, (DWORD*)&len);
	IMFSample_ConvertToContiguousBuffer(p_sample, &p_outbuf);

	if (FAILED(hr = IMFMediaBuffer_Lock(p_outbuf, &p_data, NULL, NULL))) {
		cdk_loge("Failed to IMFMediaBuffer_Lock: 0x%x.\n", hr);

		SAFE_RELEASE(p_outbuf);
		IMFSample_Release(p_sample);
		return;
	}
	p_outdata->bitstream = malloc(len);
	if (!p_outdata->bitstream) {
		SAFE_RELEASE(p_outbuf);
		IMFSample_Release(p_sample);
		return;
	}
	memcpy(p_outdata->bitstream, p_data, len);
	p_outdata->bslen = len;

	IMFMediaBuffer_Unlock(p_outbuf);
	IMFMediaBuffer_Release(p_outbuf);

	LONGLONG pts;
	hr = IMFSample_GetSampleTime(p_sample, &pts);

	p_outdata->dts = pts;
	p_outdata->pts = pts;

	UINT32 t32 = 0;
	hr = IMFAttributes_GetUINT32(p_sample, &MFSampleExtension_CleanPoint, &t32);
	if (!FAILED(hr) && t32 != 0){
		p_outdata->keyframe = true;
	}
	UINT64 t = 0;
	hr = IMFAttributes_GetUINT64(p_sample, &MFSampleExtension_DecodeTimestamp, &t);
	if (!FAILED(hr)) {
		p_outdata->dts = t;
	}
	IMFSample_Release(p_sample);
}

void mf_dump_video(const char* filename, video_frame_t* frame) {
	static FILE* file;
	if (!file) {
		file = fopen(filename, "ab+");
	}
	fwrite(frame->bitstream, frame->bslen, 1, file);
	fclose(file);
	file = NULL;
}