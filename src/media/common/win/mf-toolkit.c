#include "mf-toolkit.h"
#include <mfapi.h>

void mf_hw_video_encoder_create() {
	HRESULT hr = S_OK;
	IMFActivate** pp_activate = NULL;
	IMFActivate* p_activate = NULL;
	IMFTransform** trans = NULL;
	IMFAttributes* attrs = NULL;
	uint32_t cnt = 0;
	MFT_REGISTER_TYPE_INFO in_type = { MFMediaType_Video, MFVideoFormat_NV12 };
	MFT_REGISTER_TYPE_INFO out_type = { MFMediaType_Video, MFVideoFormat_HEVC };

	if (FAILED(hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER, &in_type, &out_type, &pp_activate, &cnt))) {
		cdk_loge("Failed to MFTEnumEx: 0x%x.\n", hr);
		return;
	}
	if (!cnt) {
		cdk_loge("No available encoder.\n");
		return;
	}
	p_activate = pp_activate[0];

	for (uint32_t i = 0; i < cnt; i++) {
		IMFActivate_Release(pp_activate[i]);
	}
	if (FAILED(hr = IMFActivate_ActivateObject(p_activate, &IID_IMFTransform, (void**)trans))) {
		cdk_loge("Failed to IMFActivate_ActivateObject: 0x%x.\n", hr);
		return;
	}
	if (FAILED(hr = IMFTransform_GetAttributes(*trans, &attrs))) {
		cdk_loge("Failed to IMFTransform_GetAttributes: 0x%x.\n", hr);
		return;
	}
}

void mf_hw_video_encoder_destroy() {

}

void mf_hw_video_encode(ID3D11Texture2D* p_indata, MFT_OUTPUT_DATA_BUFFER* p_outdata) {
	
}