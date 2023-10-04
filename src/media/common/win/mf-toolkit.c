#include "mf-toolkit.h"
#include <mfapi.h>

void mf_hw_video_encode(ID3D11Texture2D* p_indata, MFT_OUTPUT_DATA_BUFFER* p_outdata, const char* type) {
	HRESULT hr = S_OK;
	IMFActivate** pp_activate = NULL;
	uint32_t cnt = 0;
	MFT_REGISTER_TYPE_INFO info = {0};

	/*if (!strncmp(type, "h265", strlen("h265"))) {
		info.guidMajorType = MFMediaType_Video;
		info.guidSubtype = MFVideoFormat_H265;
	}
	if (FAILED(hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER, NULL, &info, &pp_activate, &cnt))) {
		cdk_loge("Failed to MFTEnumEx: 0x%x.\n", hr);
		return;
	}*/
}