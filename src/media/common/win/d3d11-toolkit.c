#include "d3d11-toolkit.h"
#include "dxgi-toolkit.h"

typedef struct d3d11_video_device_s {
	ID3D11VideoDevice* p_device;
	ID3D11VideoContext* p_context;
}d3d11_video_device_t;

static d3d11_video_device_t d3d11_vd_;

static void _d3d11_video_device_create(void) {
	HRESULT hr = S_OK;
	if (FAILED(hr = ID3D11Device_QueryInterface(d3d11_.p_device, &IID_ID3D11VideoDevice, &d3d11_vd_.p_device))) {
		cdk_loge("Failed to ID3D11Device_QueryInterface: 0x%x.\n", hr);
		return;
	}
	if (FAILED(hr = ID3D11DeviceContext_QueryInterface(d3d11_.p_context, &IID_ID3D11VideoContext, &d3d11_vd_.p_context))) {
		cdk_loge("Failed to ID3D11DeviceContext_QueryInterface: 0x%x.\n", hr);
		return;
	}
}

static void _d3d11_video_device_destroy(void) {
	SAFE_RELEASE(d3d11_vd_.p_device);
	SAFE_RELEASE(d3d11_vd_.p_context);
}

void d3d11_device_create(void) {
	HRESULT hr = S_OK;
	IDXGIFactory5* p_factory5 = NULL; /* using five version to control tearing */
	IDXGIAdapter* p_adapter = NULL;

	memset(&d3d11_, 0, sizeof(d3d11_device_t));
	/**
	 * using CreateDXGIFactory1 not CreateDXGIFactory for IDXGIOutput1_DuplicateOutput success.
	 * https://docs.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgioutput1-duplicateoutput
	 */
	if (FAILED(hr = CreateDXGIFactory1(&IID_IDXGIFactory5, &p_factory5)))
	{
		cdk_loge("Failed to CreateDXGIFactory1: 0x%x.\n", hr);
		return;
	}
	IDXGIFactory5_EnumAdapters(p_factory5, 0, &p_adapter);

	DXGI_ADAPTER_DESC desc;
	IDXGIAdapter_GetDesc(p_adapter, &desc);

	if (desc.VendorId == VENDOR_ID_NVIDIA
		|| desc.VendorId == VENDOR_ID_AMD
		|| desc.VendorId == VENDOR_ID_MICROSOFT
		|| desc.VendorId == VENDOR_ID_INTEL) {

		d3d11_.vendor = desc.VendorId;
	}
	else {
		cdk_logd("Not supported dxgi adapter.\n");

		SAFE_RELEASE(p_factory5);
		SAFE_RELEASE(p_adapter);
		return;
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
		&d3d11_.p_device,
		NULL,
		&d3d11_.p_context);

	if (FAILED(hr)) {
		cdk_loge("Failed to D3D11CreateDevice: 0x%x.\n", hr);

		SAFE_RELEASE(p_factory5);
		SAFE_RELEASE(p_adapter);
		return;
	}
	SAFE_RELEASE(p_factory5);
	SAFE_RELEASE(p_adapter);
	return;
}

void d3d11_device_destroy(void) {
	SAFE_RELEASE(d3d11_.p_device);
	SAFE_RELEASE(d3d11_.p_context);
}

void d3d11_texture2d_create(ID3D11Texture2D** pp_texture2d, int width, int height) {
	D3D11_TEXTURE2D_DESC desc;
	memset(&desc, 0, sizeof(D3D11_TEXTURE2D_DESC));

	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_NV12;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET;

	if (FAILED(ID3D11Device_CreateTexture2D(d3d11_.p_device, &desc, NULL, pp_texture2d))) {
		cdk_loge("Faliled to ID3D11Device_CreateTexture2D\n");
		return;
	}
}

void d3d11_texture2d_destroy(ID3D11Texture2D* p_texture2d) {
	SAFE_RELEASE(p_texture2d);
}

void d3d11_video_processor_create(int in_width, int in_height, int out_width, int out_heigh) {
	HRESULT hr = S_OK;
	D3D11_VIDEO_PROCESSOR_CONTENT_DESC content_desc;

	content_desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
	content_desc.InputFrameRate.Numerator = 1;
	content_desc.InputFrameRate.Denominator = 1;
	content_desc.InputHeight = in_height;
	content_desc.InputWidth = in_width;
	content_desc.OutputFrameRate.Numerator = 1;
	content_desc.OutputFrameRate.Denominator = 1;
	content_desc.OutputHeight = out_heigh;
	content_desc.OutputWidth = out_width;
	content_desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

	_d3d11_video_device_create();

	if (FAILED(hr = ID3D11VideoDevice_CreateVideoProcessorEnumerator(d3d11_vd_.p_device, &content_desc, &d3d11_vp_.p_enumerator))) {
		cdk_loge("Failed to ID3D11VideoDevice_CreateVideoProcessorEnumerator: 0x%x.\n", hr);
		return;
	}
	if (FAILED(hr = ID3D11VideoDevice_CreateVideoProcessor(d3d11_vd_.p_device, d3d11_vp_.p_enumerator, 0, &d3d11_vp_.p_processor))) {
		cdk_loge("Failed to ID3D11VideoDevice_CreateVideoProcessor: 0x%x.\n", hr);
		return;
	}
}

void d3d11_video_processor_destroy(void) {
	_d3d11_video_device_destroy();
	SAFE_RELEASE(d3d11_vp_.p_enumerator);
	SAFE_RELEASE(d3d11_vp_.p_processor);
}

void d3d11_bgra_to_nv12(ID3D11Texture2D* p_in, ID3D11Texture2D* p_out) {
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

void d3d11_dump_rawdata(const char* filename, ID3D11Texture2D* p_texture2d, DXGI_FORMAT fmt) {
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

	ID3D11Device_CreateTexture2D(d3d11_.p_device, &desc, NULL, &p_tex2d);
	ID3D11DeviceContext_CopyResource(d3d11_.p_context, (ID3D11Resource*)p_tex2d, (ID3D11Resource*)p_texture2d);
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