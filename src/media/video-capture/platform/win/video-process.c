#include "video-process.h"


typedef struct d3d11_video_processor_s {
	ID3D11VideoProcessor* p_processor;
	ID3D11VideoProcessorEnumerator* p_enumerator;
}d3d11_video_processor_t;

d3d11_video_processor_t d3d11_vp_;

typedef struct d3d11_video_device_s {
	ID3D11VideoDevice* p_device;
	ID3D11VideoContext* p_context;
}d3d11_video_device_t;

d3d11_video_device_t d3d11_vd_;

static void _d3d11_video_processor_create(D3D11_TEXTURE2D_DESC* p_idesc, D3D11_TEXTURE2D_DESC* p_odesc) {
	HRESULT hr = S_OK;
	D3D11_VIDEO_PROCESSOR_CONTENT_DESC content_desc;

	content_desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
	content_desc.InputFrameRate.Numerator = 1;
	content_desc.InputFrameRate.Denominator = 1;
	content_desc.InputHeight = p_idesc->Height;
	content_desc.InputWidth = p_idesc->Width;
	content_desc.OutputFrameRate.Numerator = 1;
	content_desc.OutputFrameRate.Denominator = 1;
	content_desc.OutputHeight = p_odesc->Height;
	content_desc.OutputWidth = p_odesc->Width;
	content_desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

	if (FAILED(hr = ID3D11VideoDevice_CreateVideoProcessorEnumerator(d3d11_vd_.p_device, &content_desc, &d3d11_vp_.p_enumerator))) {
		cdk_loge("Failed to ID3D11VideoDevice_CreateVideoProcessorEnumerator: 0x%x.\n", hr);
		return;
	}
	if (FAILED(hr = ID3D11VideoDevice_CreateVideoProcessor(d3d11_vd_.p_device, d3d11_vp_.p_enumerator, 0, &d3d11_vp_.p_processor))) {
		cdk_loge("Failed to ID3D11VideoDevice_CreateVideoProcessor: 0x%x.\n", hr);
		return;
	}
}

static void _d3d11_video_processor_destroy(void) {
	SAFE_RELEASE(d3d11_vp_.p_enumerator);
	SAFE_RELEASE(d3d11_vp_.p_processor);
}

void d3d11_video_device_create(void) {
	HRESULT hr = S_OK;
	if (!d3d11_vd_.p_device || !d3d11_vd_.p_context) {
		if (FAILED(hr = ID3D11Device_QueryInterface(d3d11_.p_device, &IID_ID3D11VideoDevice, &d3d11_vd_.p_device))) {
			cdk_loge("Failed to ID3D11Device_QueryInterface: 0x%x.\n", hr);
			return;
		}
		if (FAILED(hr = ID3D11DeviceContext_QueryInterface(d3d11_.p_context, &IID_ID3D11VideoContext, &d3d11_vd_.p_context))) {
			cdk_loge("Failed to ID3D11DeviceContext_QueryInterface: 0x%x.\n", hr);
			return;
		}
	}
}

void d3d11_video_device_destroy(void) {
	SAFE_RELEASE(d3d11_vd_.p_device);
	SAFE_RELEASE(d3d11_vd_.p_context);
	_d3d11_video_processor_destroy();
}

void d3d11_bgra_to_nv12(ID3D11Texture2D* p_in, ID3D11Texture2D* p_out) {
	HRESULT hr = S_OK;
	ID3D11VideoProcessorInputView* p_iv = NULL;
	ID3D11VideoProcessorOutputView* p_ov = NULL;
	D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC ivd;
	D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC ovd;
	D3D11_VIDEO_PROCESSOR_STREAM stream;
	D3D11_TEXTURE2D_DESC idesc;
	D3D11_TEXTURE2D_DESC odesc;
	static D3D11_TEXTURE2D_DESC last;

	memset(&ivd, 0, sizeof(D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC));
	memset(&ovd, 0, sizeof(D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC));
	memset(&stream, 0, sizeof(D3D11_VIDEO_PROCESSOR_STREAM));
	memset(&idesc, 0, sizeof(D3D11_TEXTURE2D_DESC));
	memset(&odesc, 0, sizeof(D3D11_TEXTURE2D_DESC));
	memset(&last, 0, sizeof(D3D11_TEXTURE2D_DESC));

	ID3D11Texture2D_GetDesc(p_in, &idesc);
	ID3D11Texture2D_GetDesc(p_out, &odesc);

	if (last.Width != idesc.Width || last.Height != idesc.Height) {
		_d3d11_video_processor_destroy();
		_d3d11_video_processor_create(&idesc, &odesc);
	}
	last = idesc;

	ivd.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
	ovd.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;

	if (FAILED(hr = ID3D11VideoDevice_CreateVideoProcessorInputView(d3d11_vd_.p_device, (ID3D11Resource*)p_in, d3d11_vp_.p_enumerator, &ivd, &p_iv))) {
		cdk_loge("Failed to ID3D11VideoDevice_CreateVideoProcessorInputView: 0x%x.\n", hr);
		return;
	}
	if (FAILED(hr = ID3D11VideoDevice_CreateVideoProcessorOutputView(d3d11_vd_.p_device, (ID3D11Resource*)p_out, d3d11_vp_.p_enumerator, &ovd, &p_ov))) {
		cdk_loge("Failed to ID3D11VideoDevice_CreateVideoProcessorOutputView: 0x%x.\n", hr);

		SAFE_RELEASE(p_iv);
		return;
	}
	stream.Enable = true;
	stream.pInputSurface = p_iv;
	if (FAILED(hr = ID3D11VideoContext_VideoProcessorBlt(d3d11_vd_.p_context, d3d11_vp_.p_processor, p_ov, 0, 1, &stream))) {
		cdk_loge("Failed to ID3D11VideoContext_VideoProcessorBlt: 0x%x.\n", hr);

		SAFE_RELEASE(p_iv);
		SAFE_RELEASE(p_ov);
		return;
	}
	SAFE_RELEASE(p_iv);
	SAFE_RELEASE(p_ov);
}