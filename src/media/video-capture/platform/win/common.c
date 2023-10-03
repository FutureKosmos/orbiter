#include "common.h"

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