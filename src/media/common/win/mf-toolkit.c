#include "mf-toolkit.h"
#include <mfapi.h>
#include <mfobjects.h>

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
	IMFTransform* p_trans = NULL;
	IMFAttributes* p_attrs = NULL;
	IMFActivate* p_activate = NULL;
	IMFMediaEventGenerator* p_gen = NULL;
	DWORD in_stm;
	DWORD out_stm;
	
	IMFMediaType* p_in_type = NULL;
	IMFMediaType* p_out_type = NULL; 

	MFT_REGISTER_TYPE_INFO in_type = { MFMediaType_Video, MFVideoFormat_NV12 };
	MFT_REGISTER_TYPE_INFO out_type = { MFMediaType_Video, MFVideoFormat_HEVC };

	wchar_t encoder_name[512] = { 0 };

	if (!atomic_flag_test_and_set(&inited_)) {
		_mf_create();
	}
	if (FAILED(hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER, &in_type, &out_type, &pp_activate, &num_activate))) {
		cdk_loge("Failed to MFTEnumEx: 0x%x.\n", hr);
		return;
	}
	if (!num_activate) {
		cdk_loge("No available encoder.\n");
		return;
	}
	for (int i = 0; i < num_activate; i++) {
		if (FAILED(hr = IMFActivate_ActivateObject(pp_activate[i], &IID_IMFTransform, &p_trans))) {
			cdk_loge("Failed to IMFActivate_ActivateObject: 0x%x.\n", hr);
			return;
		}
		if (p_trans) {
			p_activate = pp_activate[i];
			IMFActivate_AddRef(p_activate);
			break;
		}
	}
	for (int i = 0; i < num_activate; i++) {
		IMFActivate_Release(pp_activate[i]);
	}
	CoTaskMemFree(pp_activate);

	if (FAILED(hr = IMFTransform_GetAttributes(p_trans, &p_attrs))) {
		cdk_loge("Failed to IMFTransform_GetAttributes: 0x%x.\n", hr);
		return;
	}
	IMFActivate_GetString(p_activate, &MFT_FRIENDLY_NAME_Attribute, encoder_name, sizeof(encoder_name), NULL);

	cdk_logi("%s is available.\n", encoder_name);
	IMFActivate_Release(p_activate);

	//unlock
	uint32_t val;
	if (FAILED(hr = IMFAttributes_GetUINT32(p_attrs, &MF_TRANSFORM_ASYNC, &val))) {
		cdk_loge("Failed to get MF_TRANSFORM_ASYNC: 0x%x.\n", hr);

		IMFTransform_Release(p_trans);
		IMFAttributes_Release(p_attrs);
		return;
	}
	if (!val) {
		cdk_loge("hardware MFT is not async\n");

		IMFTransform_Release(p_trans);
		IMFAttributes_Release(p_attrs);
		return;
	}
	if (FAILED(hr = IMFAttributes_SetUINT32(p_attrs, &MF_TRANSFORM_ASYNC_UNLOCK, true))) {
		cdk_loge("Failed to MF_TRANSFORM_ASYNC_UNLOCK: 0x%x.\n", hr);

		IMFTransform_Release(p_trans);
		IMFAttributes_Release(p_attrs);
		return;
	}
	if (FAILED(hr = IMFTransform_QueryInterface(p_trans, &IID_IMFMediaEventGenerator, &p_gen))) {
		cdk_loge("Failed to query IMFMediaEventGenerator: 0x%x.\n", hr);

		IMFTransform_Release(p_trans);
		IMFAttributes_Release(p_attrs);
		return;
	}
	if (FAILED(hr = IMFTransform_GetStreamIDs(p_trans, 1, &in_stm, 1, &out_stm))) {
		if (hr == E_NOTIMPL)
		{
			in_stm = 0;
			out_stm = 0;
			hr = S_OK;
		}
		else {
			cdk_loge("Failed to IMFTransform_GetStreamIDs: 0x%x.\n", hr);
			IMFTransform_Release(p_trans);
			IMFAttributes_Release(p_attrs);
			IMFMediaEventGenerator_Release(p_gen);
			return;
		}
	}
	if (FAILED(hr = IMFAttributes_SetUINT32(p_attrs, &MF_LOW_LATENCY, true))) {
		cdk_loge("Failed to set MF_LOW_LATENCY: 0x%x.\n", hr);

		IMFTransform_Release(p_trans);
		IMFAttributes_Release(p_attrs);
		IMFMediaEventGenerator_Release(p_gen);
		return;
	}
	if (FAILED(hr = MFCreateMediaType(&p_out_type))) {
		cdk_loge("Failed to MFCreateMediaType: 0x%x.\n", hr);

		IMFTransform_Release(p_trans);
		IMFAttributes_Release(p_attrs);
		IMFMediaEventGenerator_Release(p_gen);
		return;
	}
	IMFMediaType_SetGUID(p_out_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
	IMFMediaType_SetGUID(p_out_type, &MF_MT_SUBTYPE, &MFVideoFormat_HEVC);
	IMFMediaType_SetUINT32(p_out_type, &MF_MT_AVG_BITRATE, bitrate);
	MFSetAttributeSize(p_out_type, &MF_MT_FRAME_SIZE, width, height);
	MFSetAttributeRatio(p_out_type, &MF_MT_FRAME_RATE, framerate, 1);
	IMFMediaType_SetUINT32(p_out_type, &MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	IMFMediaType_SetUINT32(p_out_type, &MF_MT_ALL_SAMPLES_INDEPENDENT, true);

	if (FAILED(hr = IMFTransform_SetOutputType(p_trans, out_stm, p_out_type, 0))) {
		cdk_loge("Failed to IMFTransform_SetOutputType: 0x%x.\n", hr);

		IMFTransform_Release(p_trans);
		IMFAttributes_Release(p_attrs);
		IMFMediaEventGenerator_Release(p_gen);
		IMFMediaType_Release(p_out_type);
		return;
	}
	if (FAILED(hr = MFCreateMediaType(&p_in_type))) {
		cdk_loge("Failed to MFCreateMediaType: 0x%x.\n", hr);

		IMFTransform_Release(p_trans);
		IMFAttributes_Release(p_attrs);
		IMFMediaEventGenerator_Release(p_gen);
		IMFMediaType_Release(p_out_type);
		return;
	}
	for (DWORD i = 0;; i++)
	{
		if (FAILED(hr = IMFTransform_GetInputAvailableType(p_trans, in_stm, i, &p_in_type))) {
			cdk_loge("Failed to IMFTransform_GetInputAvailableType: 0x%x.\n", hr);

			IMFTransform_Release(p_trans);
			IMFAttributes_Release(p_attrs);
			IMFMediaEventGenerator_Release(p_gen);
			IMFMediaType_Release(p_out_type);
			IMFMediaType_Release(p_in_type);
			return;
		}
		IMFMediaType_SetGUID(p_in_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
		IMFMediaType_SetGUID(p_in_type, &MF_MT_SUBTYPE, &MFVideoFormat_NV12);
		MFSetAttributeSize(p_in_type, &MF_MT_FRAME_SIZE, width, height);
		MFSetAttributeRatio(p_in_type, &MF_MT_FRAME_RATE, framerate, 1);

		if (FAILED(hr = IMFTransform_SetInputType(p_trans, in_stm, p_in_type, 0))) {
			cdk_loge("Failed to IMFTransform_SetInputType: 0x%x.\n", hr);

			IMFTransform_Release(p_trans);
			IMFAttributes_Release(p_attrs);
			IMFMediaEventGenerator_Release(p_gen);
			IMFMediaType_Release(p_out_type);
			IMFMediaType_Release(p_in_type);
			return;
		}
		break;
	}
	IMFTransform_Release(p_trans);
	IMFAttributes_Release(p_attrs);
}

void mf_hw_video_encoder_destroy() {
	_mf_destroy();
}

void mf_hw_video_encode(ID3D11Texture2D* p_indata, MFT_OUTPUT_DATA_BUFFER* p_outdata) {
	
}