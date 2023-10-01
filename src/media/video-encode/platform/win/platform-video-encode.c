#define COBJMACROS
#include "media/video-encode/video-encode-types.h"
#include "media/video-capture/video-capture-types.h"

#define SAFE_RELEASE(u) \
    do { if ((u) != NULL) (u)->lpVtbl->Release(u); (u) = NULL; } while(0)

once_flag once = ONCE_FLAG_INIT;

static bool _d3d11_create_device(ID3D11Device** device, ID3D11DeviceContext** context) {
	HRESULT          hr;
	IDXGIFactory5* p_dxgi_factory5; /* using five version to control tearing */
	IDXGIAdapter* p_dxgi_adapter;

	p_dxgi_factory5 = NULL;
	p_dxgi_adapter = NULL;

	SAFE_RELEASE(*device);
	SAFE_RELEASE(*context);

	/**
	 * using CreateDXGIFactory1 not CreateDXGIFactory for IDXGIOutput1_DuplicateOutput success.
	 * https://docs.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgioutput1-duplicateoutput
	 */
	if (FAILED(hr = CreateDXGIFactory1(&IID_IDXGIFactory5, &p_dxgi_factory5)))
	{
		printf("create dxgi factory failed: 0x%x.\n", hr);
		return false;
	}
	IDXGIFactory5_EnumAdapters(p_dxgi_factory5, 0, &p_dxgi_adapter);

	/**
	 * when adapter not null, driver type must be D3D_DRIVER_TYPE_UNKNOWN.
	 * https://docs.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-d3d11createdevice
	 */
	hr = D3D11CreateDevice(p_dxgi_adapter,
		D3D_DRIVER_TYPE_UNKNOWN,
		NULL,
		0,
		NULL,
		0,
		D3D11_SDK_VERSION,
		device,
		NULL,
		context);

	if (FAILED(hr)) {
		printf("d3d11 create device and context failed: 0x%x.\n", hr);

		SAFE_RELEASE(p_dxgi_factory5);
		SAFE_RELEASE(p_dxgi_adapter);
		return false;
	}

	SAFE_RELEASE(p_dxgi_factory5);
	SAFE_RELEASE(p_dxgi_adapter);
	return true;
}

static void _d3d11_create_video_device(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11VideoDevice** video_device, ID3D11VideoContext** video_context) {
	HRESULT hr;
	if (FAILED(hr = ID3D11Device_QueryInterface(device, &IID_ID3D11VideoDevice, video_device)))
	{
		printf("query video device failed: 0x%x.\n", hr);
		return false;
	}
	if (FAILED(hr = ID3D11DeviceContext_QueryInterface(context, &IID_ID3D11VideoContext, video_context)))
	{
		printf("query video device context failed: 0x%x.\n", hr);
		return false;
	}
}

static void _bgra_to_nv12(ID3D11Texture2D* bgra, ID3D11Texture2D* nv12){

}

video_encode_frame_t* platform_video_encode(video_capture_frame_t* frame) {
	ID3D11Device* device;
	ID3D11DeviceContext* context;
	ID3D11VideoDevice* video_device;
	ID3D11VideoContext* video_context;

	if (!atomic_flag_test_and_set(once)) {
		_d3d11_create_device(&device, &context);
		_d3d11_create_video_device(device, context, &video_device, &video_context);
	}
	ID3D11Texture2D* texture = NULL;
	D3D11_TEXTURE2D_DESC desc;
	memset(&desc, 0, sizeof(D3D11_TEXTURE2D_DESC));
	desc.Width = frame->width;
	desc.Height = frame->height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_NV12;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET;
	desc.CPUAccessFlags = 0;

	if (FAILED(ID3D11Device_CreateTexture2D(device, &desc, NULL, &texture))) {
		printf("faliled to create d3d11texture\n");
		return NULL;
	}
	_bgra_to_nv12(frame->frame, texture);
	return NULL;
}