#define COBJMACROS
#include "dxgi-video-capture.h"

bool dxgi_create_duplicator(dxgi_device_t* device, dxgi_dup_t* duplication) {
	IDXGIDevice*   p_dxgi_device;
	IDXGIAdapter*  p_dxgi_adapter;
	IDXGIOutput*   p_dxgi_output;
	IDXGIOutput1*  p_dxgi_output1;
	HRESULT        hr;

	SAFE_RELEASE(duplication->dup);

	if (duplication->width != 0 || duplication->height != 0) {
		duplication->width = 0;
		duplication->height = 0;
	}
	if (FAILED(hr = ID3D11Device_QueryInterface(device->dev, &IID_IDXGIDevice, &p_dxgi_device)))
	{
		printf("query dxgi device failed: 0x%x.\n", hr);
		return false;
	}
	if (FAILED(hr = IDXGIDevice_GetAdapter(p_dxgi_device, &p_dxgi_adapter)))
	{
		printf("get dxgi adapter failed: 0x%x.\n", hr);
		
		SAFE_RELEASE(p_dxgi_device);
		return false;
	}
	if (FAILED(hr = IDXGIAdapter_EnumOutputs(p_dxgi_adapter, 0, &p_dxgi_output)))
	{
		printf("get dxgi output failed: 0x%x.\n", hr);
	
		SAFE_RELEASE(p_dxgi_device);
		SAFE_RELEASE(p_dxgi_adapter);
		return false;
	}

	if (FAILED(hr = IDXGIOutput_QueryInterface(p_dxgi_output, &IID_IDXGIOutput1, &p_dxgi_output1)))
	{
		printf("get dxgi output1 failed: 0x%x.\n", hr);

		SAFE_RELEASE(p_dxgi_device);
		SAFE_RELEASE(p_dxgi_adapter);
		SAFE_RELEASE(p_dxgi_output);
		return false;
	}
	if (FAILED(hr = IDXGIOutput1_DuplicateOutput(p_dxgi_output1, (IUnknown*)p_dxgi_device, &duplication->dup)))
	{
		printf("get dxgi dup failed: 0x%x.\n", hr);
		SAFE_RELEASE(p_dxgi_device);
		SAFE_RELEASE(p_dxgi_adapter);
		SAFE_RELEASE(p_dxgi_output);
		SAFE_RELEASE(p_dxgi_output1);
		
		return false;
	}
	DXGI_OUTDUPL_DESC dup_desc;
	memset(&dup_desc, 0, sizeof(dup_desc));
	IDXGIOutputDuplication_GetDesc(duplication->dup, &dup_desc);

	duplication->height = dup_desc.ModeDesc.Height;
	duplication->width  = dup_desc.ModeDesc.Width;

	SAFE_RELEASE(p_dxgi_device);
	SAFE_RELEASE(p_dxgi_adapter);
	SAFE_RELEASE(p_dxgi_output);
	SAFE_RELEASE(p_dxgi_output1);
	return true;
}

bool dxgi_create_device(dxgi_device_t* device) {
	HRESULT          hr;
	IDXGIFactory5*   p_dxgi_factory5; /* using five version to control tearing */
	IDXGIAdapter*    p_dxgi_adapter;

	p_dxgi_factory5 = NULL;
	p_dxgi_adapter  = NULL;

	SAFE_RELEASE(device->dev);
	SAFE_RELEASE(device->ctx);
	if (device->vid != 0) {
		device->vid = 0;
	}
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

	DXGI_ADAPTER_DESC dxgi_desc;
	IDXGIAdapter_GetDesc(p_dxgi_adapter, &dxgi_desc);

	if (dxgi_desc.VendorId == VENDOR_ID_NVIDIA
	 || dxgi_desc.VendorId == VENDOR_ID_AMD
	 || dxgi_desc.VendorId == VENDOR_ID_MICROSOFT
	 || dxgi_desc.VendorId == VENDOR_ID_INTEL) {

		device->vid = dxgi_desc.VendorId;
	}
	else {
		printf("not supported dxgi adapter.\n");
	
		SAFE_RELEASE(p_dxgi_factory5);
		SAFE_RELEASE(p_dxgi_adapter);

		return false;
	}
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
		&device->dev,
		NULL,
		&device->ctx);

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

int dxgi_capture_frame(dxgi_dup_t* duplication, dxgi_frame_t* p_dxgi_frame) {

	HRESULT           hr;
	IDXGIResource*    p_dxgi_res;

	p_dxgi_res = NULL;

	IDXGIOutputDuplication_ReleaseFrame(duplication->dup);

	hr = IDXGIOutputDuplication_AcquireNextFrame(duplication->dup,
		500,
		&p_dxgi_frame->info,
		&p_dxgi_res);
	if (FAILED(hr)) {

		if (hr == DXGI_ERROR_WAIT_TIMEOUT)
		{
			printf("wait frame timeout.\n");
			return DXGI_STATUS_TIMEOUT;
		}
		if (hr == DXGI_ERROR_ACCESS_LOST || hr == DXGI_ERROR_INVALID_CALL)
		{
			printf("get frame failed, maybe resolution changed or desktop switched. please re-create desktop-duplicator.\n");
			return DXGI_STATUS_RECREATE_DUP;
		}
	}

	/* no desktop image update, only cursor move. */
	if (p_dxgi_frame->info.AccumulatedFrames == 0 || p_dxgi_frame->info.LastPresentTime.QuadPart == 0)
	{
		printf("no desktop image update, only cursor move.\n");

		SAFE_RELEASE(p_dxgi_res);
		return DXGI_STATUS_NO_UPDATE;
	}
	SAFE_RELEASE(p_dxgi_frame->frame);

	if (FAILED(IDXGIResource_QueryInterface(p_dxgi_res, &IID_ID3D11Texture2D, &p_dxgi_frame->frame)))
	{
		printf("get d3d11 texture2D failed.\n");
		SAFE_RELEASE(p_dxgi_res);
		return DXGI_STATUS_FAILURE;
	}
	SAFE_RELEASE(p_dxgi_res);
	return DXGI_STATUS_SUCCESS;
}
