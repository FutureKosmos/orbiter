#include "dxgi-capture.h"
#include "video-process.h"
#include "common/synchronized-queue.h"

bool running_;

void dump_nv12(ID3D11Texture2D* frame) {
	static FILE* bgra;
	D3D11_TEXTURE2D_DESC d3d11_texture_desc;
	ID3D11Texture2D* p_frame;
	IDXGISurface* p_dxgi_surface;
	DXGI_MAPPED_RECT dxgi_mapped_rect;

	if (!bgra) {
		bgra = fopen("dump.yuv", "ab+");
	}

	ID3D11Texture2D_GetDesc(frame, &d3d11_texture_desc);
	d3d11_texture_desc.MipLevels = 1;
	d3d11_texture_desc.ArraySize = 1;
	d3d11_texture_desc.SampleDesc.Count = 1;
	d3d11_texture_desc.Usage = D3D11_USAGE_STAGING;
	d3d11_texture_desc.BindFlags = 0;
	d3d11_texture_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	d3d11_texture_desc.MiscFlags = 0;

	ID3D11Device_CreateTexture2D(d3d11_.p_device, &d3d11_texture_desc, NULL, &p_frame);
	ID3D11DeviceContext_CopyResource(d3d11_.p_context, (ID3D11Resource*)p_frame, (ID3D11Resource*)frame);

	ID3D11Texture2D_QueryInterface(p_frame, &IID_IDXGISurface, &p_dxgi_surface);
	if (p_frame) {
		ID3D11Texture2D_Release(p_frame);
		p_frame = NULL;
	}
	IDXGISurface_Map(p_dxgi_surface, &dxgi_mapped_rect, DXGI_MAP_READ);

	for (int i = 0; i < duplicator_.height; i++) {
		fwrite(dxgi_mapped_rect.pBits + i * dxgi_mapped_rect.Pitch, dxgi_mapped_rect.Pitch, 1, bgra);
	}
	IDXGISurface_Unmap(p_dxgi_surface);

	if (p_dxgi_surface) {
		IDXGISurface_Release(p_dxgi_surface);
		p_dxgi_surface = NULL;
	}
}
void dump_bgra(dxgi_frame_t* frame) {
	static FILE* bgra;
	D3D11_TEXTURE2D_DESC d3d11_texture_desc;
	ID3D11Texture2D* p_frame;
	IDXGISurface* p_dxgi_surface;
	DXGI_MAPPED_RECT dxgi_mapped_rect;

	if (!bgra) {
		bgra = fopen("dump.bgra", "ab+");
	}

	ID3D11Texture2D_GetDesc(frame->p_texture2d, &d3d11_texture_desc);
	d3d11_texture_desc.MipLevels = 1;
	d3d11_texture_desc.ArraySize = 1;
	d3d11_texture_desc.SampleDesc.Count = 1;
	d3d11_texture_desc.Usage = D3D11_USAGE_STAGING;
	d3d11_texture_desc.BindFlags = 0;
	d3d11_texture_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	d3d11_texture_desc.MiscFlags = 0;

	ID3D11Device_CreateTexture2D(d3d11_.p_device, &d3d11_texture_desc, NULL, &p_frame);
	ID3D11DeviceContext_CopyResource(d3d11_.p_context, (ID3D11Resource*)p_frame, (ID3D11Resource*)frame->p_texture2d);

	ID3D11Texture2D_QueryInterface(p_frame, &IID_IDXGISurface, &p_dxgi_surface);
	if (p_frame) {
		ID3D11Texture2D_Release(p_frame);
		p_frame = NULL;
	}
	IDXGISurface_Map(p_dxgi_surface, &dxgi_mapped_rect, DXGI_MAP_READ);

	for (int i = 0; i < duplicator_.height; i++) {
		fwrite(dxgi_mapped_rect.pBits + i * dxgi_mapped_rect.Pitch, dxgi_mapped_rect.Pitch, 1, bgra);
	}
	IDXGISurface_Unmap(p_dxgi_surface);

	if (p_dxgi_surface) {
		IDXGISurface_Release(p_dxgi_surface);
		p_dxgi_surface = NULL;
	}
}

void platform_video_capture_start(synchronized_queue_t* p_nalus) {
	dxgi_frame_t frame;

	d3d11_device_create();
	dxgi_duplicator_create();
	d3d11_video_device_create();

	running_ = true;
	while (running_) {
		dxgi_status_t status = dxgi_capture_frame(&frame);
		if (status == DXGI_STATUS_TIMEOUT || status == DXGI_STATUS_NO_UPDATE || status == DXGI_STATUS_FAILURE) {
			continue;
		}
		if (status == DXGI_STATUS_RECREATE_DUP) {
			cdk_loge("Re-create duplicator.\n");

			dxgi_duplicator_create();
			continue;
		}
		//dump_bgra(frame);
		ID3D11Texture2D* p_texture;
		D3D11_TEXTURE2D_DESC out_desc;
		memset(&out_desc, 0, sizeof(D3D11_TEXTURE2D_DESC));

		out_desc.Width = duplicator_.width;
		out_desc.Height = duplicator_.height;
		out_desc.MipLevels = 1;
		out_desc.ArraySize = 1;
		out_desc.Format = DXGI_FORMAT_NV12;
		out_desc.SampleDesc.Count = 1;
		out_desc.Usage = D3D11_USAGE_DEFAULT;
		out_desc.BindFlags = D3D11_BIND_RENDER_TARGET;
		out_desc.CPUAccessFlags = 0;

		if (FAILED(ID3D11Device_CreateTexture2D(d3d11_.p_device, &out_desc, NULL, &p_texture))) {
			cdk_loge("Faliled to ID3D11Device_CreateTexture2D\n");
			return;
		}
		dump_bgra(&frame);
		d3d11_bgra_to_nv12(frame.p_texture2d, p_texture);
		dump_nv12(p_texture);
		SAFE_RELEASE(p_texture);
	}
	dxgi_duplicator_destroy();
	d3d11_device_destroy();
	d3d11_video_device_destroy();
}

void platform_video_capture_stop(void) {
	running_ = false;
}