#define COBJMACROS
#include "media/video-encode/video-encode-types.h"
#include "media/video-capture/video-capture-types.h"

#define SAFE_RELEASE(u) \
    do { if ((u) != NULL) (u)->lpVtbl->Release(u); (u) = NULL; } while(0)

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

video_encode_frame_t* platform_video_encode(video_capture_frame_t* frame) {

	return NULL;
}