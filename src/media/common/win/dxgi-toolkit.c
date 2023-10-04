#include "dxgi-toolkit.h"

void dxgi_duplicator_create(void) {
	HRESULT hr = S_OK;
	IDXGIDevice* p_device = NULL;
	IDXGIAdapter* p_adapter = NULL;
	IDXGIOutput* p_output = NULL;
	IDXGIOutput1* p_output1 = NULL;

	if (FAILED(hr = ID3D11Device_QueryInterface(d3d11_.p_device, &IID_IDXGIDevice, &p_device))) {
		cdk_loge("Failed to ID3D11Device_QueryInterface: 0x%x.\n", hr);
		return;
	}
	if (FAILED(hr = IDXGIDevice_GetAdapter(p_device, &p_adapter))) {
		cdk_loge("Failed to IDXGIDevice_GetAdapter: 0x%x.\n", hr);

		SAFE_RELEASE(p_device);
		return;
	}
	if (FAILED(hr = IDXGIAdapter_EnumOutputs(p_adapter, 0, &p_output))) {
		cdk_loge("Failed to IDXGIAdapter_EnumOutputs: 0x%x.\n", hr);
	
		SAFE_RELEASE(p_device);
		SAFE_RELEASE(p_adapter);
		return;
	}
	if (FAILED(hr = IDXGIOutput_QueryInterface(p_output, &IID_IDXGIOutput1, &p_output1))) {
		cdk_loge("Failed to IDXGIOutput_QueryInterface: 0x%x.\n", hr);

		SAFE_RELEASE(p_device);
		SAFE_RELEASE(p_adapter);
		SAFE_RELEASE(p_output);
		return;
	}
	if (FAILED(hr = IDXGIOutput1_DuplicateOutput(p_output1, (IUnknown*)p_device, &duplicator_.p_duplication))) {
		cdk_loge("Failed to IDXGIOutput1_DuplicateOutput: 0x%x.\n", hr);

		SAFE_RELEASE(p_device);
		SAFE_RELEASE(p_adapter);
		SAFE_RELEASE(p_output);
		SAFE_RELEASE(p_output1);
		return;
	}
	DXGI_OUTDUPL_DESC desc;
	memset(&desc, 0, sizeof(desc));
	IDXGIOutputDuplication_GetDesc(duplicator_.p_duplication, &desc);

	duplicator_.height = desc.ModeDesc.Height;
	duplicator_.width  = desc.ModeDesc.Width;

	SAFE_RELEASE(p_device);
	SAFE_RELEASE(p_adapter);
	SAFE_RELEASE(p_output);
	SAFE_RELEASE(p_output1);
	return;
}

void dxgi_duplicator_destroy(void) {
	SAFE_RELEASE(duplicator_.p_duplication);
}

int dxgi_capture_frame(dxgi_frame_t* p_frame) {
	HRESULT hr = S_OK;
	IDXGIResource* p_resource = NULL;

	memset(p_frame, 0, sizeof(dxgi_frame_t));
	IDXGIOutputDuplication_ReleaseFrame(duplicator_.p_duplication);

	hr = IDXGIOutputDuplication_AcquireNextFrame(duplicator_.p_duplication,
		500,
		&p_frame->frameinfo,
		&p_resource);
	if (FAILED(hr)) {
		if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
			return DXGI_STATUS_TIMEOUT;
		}
		if (hr == DXGI_ERROR_ACCESS_LOST || hr == DXGI_ERROR_INVALID_CALL) {
			cdk_logd("Maybe resolution changed Or Desktop-switched.\n");
			return DXGI_STATUS_RECREATE_DUP;
		}
	}
	/* no desktop image update, only cursor move. */
	if (p_frame->frameinfo.AccumulatedFrames == 0 || p_frame->frameinfo.LastPresentTime.QuadPart == 0) {
		cdk_logd("No Desktop Image update, Only cursor move.\n");

		SAFE_RELEASE(p_resource);
		return DXGI_STATUS_NO_UPDATE;
	}
	SAFE_RELEASE(p_frame->p_texture2d);

	if (FAILED(hr = IDXGIResource_QueryInterface(p_resource, &IID_ID3D11Texture2D, &p_frame->p_texture2d))) {
		cdk_loge("Failed to IDXGIResource_QueryInterface: 0x%x.\n", hr);

		SAFE_RELEASE(p_resource);
		return DXGI_STATUS_FAILURE;
	}
	SAFE_RELEASE(p_resource);
	return DXGI_STATUS_SUCCESS;
}
