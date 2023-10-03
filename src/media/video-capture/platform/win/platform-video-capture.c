#include "dxgi-toolkit.h"
#include "d3d11-toolkit.h"
#include "common/synchronized-queue.h"

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
	for (int i = 0; i < duplicator_.height / 2; i++) {
		fwrite(dxgi_mapped_rect.pBits + dxgi_mapped_rect.Pitch * duplicator_.height + i * dxgi_mapped_rect.Pitch, dxgi_mapped_rect.Pitch, 1, bgra);
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

void platform_video_capture(synchronized_queue_t* p_nalus) {
	dxgi_frame_t frame;
	ID3D11Texture2D* p_tex = NULL;
	D3D11_TEXTURE2D_DESC desc;

	d3d11_device_create();
	dxgi_duplicator_create();
	d3d11_video_device_create();
	d3d11_texture2d_create(&p_tex, duplicator_.width, duplicator_.height);
	ID3D11Texture2D_GetDesc(p_tex, &desc);

	while (true) {
		dxgi_status_t status = dxgi_capture_frame(&frame);
		if (status == DXGI_STATUS_TIMEOUT || status == DXGI_STATUS_NO_UPDATE || status == DXGI_STATUS_FAILURE) {
			continue;
		}
		if (status == DXGI_STATUS_RECREATE_DUP) {
			cdk_loge("Re-create Duplicator.\n");

			dxgi_duplicator_destroy();
			dxgi_duplicator_create();
			continue;
		}
		if (desc.Width != duplicator_.width || desc.Height != duplicator_.height) {
			d3d11_texture2d_destroy(p_tex);
			d3d11_texture2d_create(&p_tex, duplicator_.width, duplicator_.height);
		}
		d3d11_video_processor_create(duplicator_.width, duplicator_.height, desc.Width, desc.Height);
		d3d11_bgra_to_nv12(frame.p_texture2d, p_tex);
		dump_nv12(p_tex);
	}
	dxgi_duplicator_destroy();
	d3d11_device_destroy();
	d3d11_video_device_destroy();
	d3d11_video_processor_destroy();
	d3d11_texture2d_destroy(p_tex);
}