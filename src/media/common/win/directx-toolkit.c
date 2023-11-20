#include "directx-toolkit.h"

bool directx_device_create(directx_device_t* p_device) {
	HRESULT hr = S_OK;
	IDXGIFactory5* p_factory5 = NULL; /* using five version to control tearing */
	IDXGIAdapter* p_adapter = NULL;

	memset(p_device, 0, sizeof(directx_device_t));
	/**
	 * using CreateDXGIFactory1 not CreateDXGIFactory for IDXGIOutput1_DuplicateOutput success.
	 * https://docs.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgioutput1-duplicateoutput
	 */
	if (FAILED(hr = CreateDXGIFactory1(&IID_IDXGIFactory5, &p_factory5)))
	{
		cdk_loge("Failed to CreateDXGIFactory1: 0x%x.\n", hr);
		goto fail;
	}
	IDXGIFactory5_EnumAdapters(p_factory5, 0, &p_adapter);

	DXGI_ADAPTER_DESC desc;
	IDXGIAdapter_GetDesc(p_adapter, &desc);

	if (desc.VendorId == VENDOR_ID_NVIDIA
		|| desc.VendorId == VENDOR_ID_AMD
		|| desc.VendorId == VENDOR_ID_MICROSOFT
		|| desc.VendorId == VENDOR_ID_INTEL) {

		p_device->vendor = desc.VendorId;
	}
	else {
		cdk_logd("not supported dxgi adapter.\n");
		goto fail;
	}
	/**
	 * when adapter not null, driver type must be D3D_DRIVER_TYPE_UNKNOWN.
	 * https://docs.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-d3d11createdevice
	 */
	hr = D3D11CreateDevice(p_adapter,
		D3D_DRIVER_TYPE_UNKNOWN,
		NULL,
		0,
		NULL,
		0,
		D3D11_SDK_VERSION,
		&p_device->p_dev,
		NULL,
		&p_device->p_ctx);

	if (FAILED(hr)) {
		cdk_loge("Failed to D3D11CreateDevice: 0x%x.\n", hr);
		goto fail;
	}
	SAFE_RELEASE(p_factory5);
	SAFE_RELEASE(p_adapter);
	return true;
fail:
	SAFE_RELEASE(p_factory5);
	SAFE_RELEASE(p_adapter);
	return false;
}

void directx_device_destroy(directx_device_t* p_device) {
	SAFE_RELEASE(p_device->p_dev);
	SAFE_RELEASE(p_device->p_ctx);
}

bool directx_texture2d_create(directx_device_t* p_device, int width, int height, DXGI_FORMAT fmt, ID3D11Texture2D** pp_texture2d) {
	D3D11_TEXTURE2D_DESC desc;
	memset(&desc, 0, sizeof(D3D11_TEXTURE2D_DESC));

	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = fmt;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT; /* use gpu */
	desc.BindFlags = D3D11_BIND_RENDER_TARGET;

	if (FAILED(ID3D11Device_CreateTexture2D(p_device->p_dev, &desc, NULL, pp_texture2d))) {
		cdk_loge("Faliled to ID3D11Device_CreateTexture2D\n");
		return false;
	}
	return true;
}

void directx_texture2d_destroy(ID3D11Texture2D* p_texture2d) {
	SAFE_RELEASE(p_texture2d);
}

bool directx_video_processor_create(directx_device_t* p_device, int in_width, int in_height, int out_width, int out_height, directx_video_processor_t* p_processor) {
	HRESULT hr = S_OK;
	D3D11_VIDEO_PROCESSOR_CONTENT_DESC content_desc;

	content_desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
	content_desc.InputFrameRate.Numerator = 1;
	content_desc.InputFrameRate.Denominator = 1;
	content_desc.InputHeight = in_height;
	content_desc.InputWidth = in_width;
	content_desc.OutputFrameRate.Numerator = 1;
	content_desc.OutputFrameRate.Denominator = 1;
	content_desc.OutputHeight = out_height;
	content_desc.OutputWidth = out_width;
	content_desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

	if (FAILED(hr = ID3D11Device_QueryInterface(p_device->p_dev, &IID_ID3D11VideoDevice, &p_processor->p_dev))) {
		cdk_loge("failed to ID3D11Device_QueryInterface: 0x%x.\n", hr);
		return false;
	}
	if (FAILED(hr = ID3D11DeviceContext_QueryInterface(p_device->p_ctx, &IID_ID3D11VideoContext, &p_processor->p_ctx))) {
		cdk_loge("failed to ID3D11DeviceContext_QueryInterface: 0x%x.\n", hr);
		return false;
	}
	if (FAILED(hr = ID3D11VideoDevice_CreateVideoProcessorEnumerator(p_processor->p_dev, &content_desc, &p_processor->p_vpe))) {
		cdk_loge("failed to ID3D11VideoDevice_CreateVideoProcessorEnumerator: 0x%x.\n", hr);
		return false;
	}
	if (FAILED(hr = ID3D11VideoDevice_CreateVideoProcessor(p_processor->p_dev, p_processor->p_vpe, 0, &p_processor->p_vp))) {
		cdk_loge("failed to ID3D11VideoDevice_CreateVideoProcessor: 0x%x.\n", hr);
		return false;
	}
	return true;
}

void directx_video_processor_destroy(directx_video_processor_t* p_processor) {
	SAFE_RELEASE(p_processor->p_ctx);
	SAFE_RELEASE(p_processor->p_dev);
	SAFE_RELEASE(p_processor->p_vp);
	SAFE_RELEASE(p_processor->p_vpe);
}

bool directx_bgra_to_nv12(directx_video_processor_t* p_processor, ID3D11Texture2D* p_in, ID3D11Texture2D* p_out) {
	HRESULT hr = S_OK;
	ID3D11VideoProcessorInputView* p_iv = NULL;
	ID3D11VideoProcessorOutputView* p_ov = NULL;
	D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC ivd;
	D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC ovd;
	D3D11_VIDEO_PROCESSOR_STREAM stream;

	memset(&ivd, 0, sizeof(D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC));
	memset(&ovd, 0, sizeof(D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC));
	memset(&stream, 0, sizeof(D3D11_VIDEO_PROCESSOR_STREAM));

	ivd.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
	ovd.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;

	if (FAILED(hr = ID3D11VideoDevice_CreateVideoProcessorInputView(p_processor->p_dev, (ID3D11Resource*)p_in, p_processor->p_vpe, &ivd, &p_iv))) {
		cdk_loge("Failed to ID3D11VideoDevice_CreateVideoProcessorInputView: 0x%x.\n", hr);
		goto fail;
	}
	if (FAILED(hr = ID3D11VideoDevice_CreateVideoProcessorOutputView(p_processor->p_dev, (ID3D11Resource*)p_out, p_processor->p_vpe, &ovd, &p_ov))) {
		cdk_loge("Failed to ID3D11VideoDevice_CreateVideoProcessorOutputView: 0x%x.\n", hr);
		goto fail;
	}
	stream.Enable = true;
	stream.pInputSurface = p_iv;
	if (FAILED(hr = ID3D11VideoContext_VideoProcessorBlt(p_processor->p_ctx, p_processor->p_vp, p_ov, 0, 1, &stream))) {
		cdk_loge("Failed to ID3D11VideoContext_VideoProcessorBlt: 0x%x.\n", hr);
		goto fail;
	}
	SAFE_RELEASE(p_iv);
	SAFE_RELEASE(p_ov);
	return true;
fail:
	SAFE_RELEASE(p_iv);
	SAFE_RELEASE(p_ov);
	return false;
}

void directx_texture2d_dump(directx_device_t* p_device, const char* filename, ID3D11Texture2D* p_texture2d, DXGI_FORMAT fmt) {
	static FILE* file;
	D3D11_TEXTURE2D_DESC desc;
	ID3D11Texture2D* p_tex2d;
	IDXGISurface* p_surface;
	DXGI_MAPPED_RECT rect;

	if (!file) {
		file = fopen(filename, "ab+");
	}
	ID3D11Texture2D_GetDesc(p_texture2d, &desc);
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.MiscFlags = 0;

	ID3D11Device_CreateTexture2D(p_device->p_dev, &desc, NULL, &p_tex2d);
	ID3D11DeviceContext_CopyResource(p_device->p_ctx, (ID3D11Resource*)p_tex2d, (ID3D11Resource*)p_texture2d);
	ID3D11Texture2D_QueryInterface(p_tex2d, &IID_IDXGISurface, &p_surface);
	ID3D11Texture2D_Release(p_tex2d);
	IDXGISurface_Map(p_surface, &rect, DXGI_MAP_READ);

	if (fmt == DXGI_FORMAT_NV12) {
		for (int i = 0; i < (int)desc.Height; i++) {
			fwrite(rect.pBits + i * rect.Pitch, rect.Pitch, 1, file);
		}
		for (int i = 0; i < (int)desc.Height / 2; i++) {
			fwrite(rect.pBits + rect.Pitch * desc.Height + i * rect.Pitch, rect.Pitch, 1, file);
		}
	}
	if (fmt == DXGI_FORMAT_B8G8R8A8_UNORM) {
		for (int i = 0; i < (int)desc.Height; i++) {
			fwrite(rect.pBits + i * rect.Pitch, rect.Pitch, 1, file);
		}
	}
	IDXGISurface_Unmap(p_surface);
	IDXGISurface_Release(p_surface);
	fclose(file);
	file = NULL;
}

bool directx_duplicator_create(directx_device_t* p_device, directx_duplicator_t* p_duplicator) {
	HRESULT hr = S_OK;
	IDXGIDevice* p_dd = NULL;
	IDXGIAdapter* p_da = NULL;
	IDXGIOutput* p_do = NULL;
	IDXGIOutput1* p_do1 = NULL;

	if (FAILED(hr = ID3D11Device_QueryInterface(p_device->p_dev, &IID_IDXGIDevice, &p_dd))) {
		cdk_loge("Failed to ID3D11Device_QueryInterface: 0x%x.\n", hr);
		goto fail;
	}
	if (FAILED(hr = IDXGIDevice_GetAdapter(p_dd, &p_da))) {
		cdk_loge("Failed to IDXGIDevice_GetAdapter: 0x%x.\n", hr);
		goto fail;
	}
	if (FAILED(hr = IDXGIAdapter_EnumOutputs(p_da, 0, &p_do))) {
		cdk_loge("Failed to IDXGIAdapter_EnumOutputs: 0x%x.\n", hr);
		goto fail;
	}
	if (FAILED(hr = IDXGIOutput_QueryInterface(p_do, &IID_IDXGIOutput1, &p_do1))) {
		cdk_loge("Failed to IDXGIOutput_QueryInterface: 0x%x.\n", hr);
		goto fail;
	}
	if (FAILED(hr = IDXGIOutput1_DuplicateOutput(p_do1, (IUnknown*)p_device->p_dev, &p_duplicator->p_dup))) {
		cdk_loge("Failed to IDXGIOutput1_DuplicateOutput: 0x%x.\n", hr);
		goto fail;
	}
	DXGI_OUTDUPL_DESC desc;
	memset(&desc, 0, sizeof(desc));
	IDXGIOutputDuplication_GetDesc(p_duplicator->p_dup, &desc);

	p_duplicator->h = desc.ModeDesc.Height;
	p_duplicator->w = desc.ModeDesc.Width;

	SAFE_RELEASE(p_dd);
	SAFE_RELEASE(p_da);
	SAFE_RELEASE(p_do);
	SAFE_RELEASE(p_do1);
	return true;
fail:
	SAFE_RELEASE(p_dd);
	SAFE_RELEASE(p_da);
	SAFE_RELEASE(p_do);
	SAFE_RELEASE(p_do1);
	return false;
}

void directx_duplicator_destroy(directx_duplicator_t* p_duplicator) {
	SAFE_RELEASE(p_duplicator->p_dup);
}

HRESULT directx_duplicator_capture(directx_duplicator_t* p_duplicator, directx_duplicator_frame_t* p_frame) {
	HRESULT hr = S_OK;
	IDXGIResource* p_res = NULL;

	memset(p_frame, 0, sizeof(directx_duplicator_frame_t));
	IDXGIOutputDuplication_ReleaseFrame(p_duplicator->p_dup);

	hr = IDXGIOutputDuplication_AcquireNextFrame(p_duplicator->p_dup,
		500,
		&p_frame->fi,
		&p_res);
	if (FAILED(hr)) {
		if (hr == DXGI_ERROR_ACCESS_LOST || hr == DXGI_ERROR_INVALID_CALL) {
			cdk_logd("Maybe resolution changed Or Desktop-switched.\n");
		}
		return hr;
	}
	/* no desktop image update, only cursor move. */
	if (p_frame->fi.AccumulatedFrames == 0 || p_frame->fi.LastPresentTime.QuadPart == 0) {
		SAFE_RELEASE(p_res);
		return DXGI_ERROR_WAIT_TIMEOUT;
	}
	SAFE_RELEASE(p_frame->p_tex2d);

	if (FAILED(hr = IDXGIResource_QueryInterface(p_res, &IID_ID3D11Texture2D, &p_frame->p_tex2d))) {
		cdk_loge("Failed to IDXGIResource_QueryInterface: 0x%x.\n", hr);

		SAFE_RELEASE(p_res);
		return hr;
	}
	SAFE_RELEASE(p_res);
	return hr;
}