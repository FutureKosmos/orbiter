#define COBJMACROS
#include "dxgi-video-capture.h"
#include "common/synchronized-queue.h"

void dump_bgra(dxgi_frame_t* frame, dxgi_device_t* dev, dxgi_dup_t* dup) {
	static FILE* bgra;
	D3D11_TEXTURE2D_DESC d3d11_texture_desc;
	ID3D11Texture2D* p_frame;
	IDXGISurface* p_dxgi_surface;
	DXGI_MAPPED_RECT dxgi_mapped_rect;

	if (!bgra) {
		bgra = fopen("dump.bgra", "ab+");
	}

	ID3D11Texture2D_GetDesc(frame->frame, &d3d11_texture_desc);
	d3d11_texture_desc.MipLevels = 1;
	d3d11_texture_desc.ArraySize = 1;
	d3d11_texture_desc.SampleDesc.Count = 1;
	d3d11_texture_desc.Usage = D3D11_USAGE_STAGING;
	d3d11_texture_desc.BindFlags = 0;
	d3d11_texture_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	d3d11_texture_desc.MiscFlags = 0;

	ID3D11Device_CreateTexture2D(dev->dev, &d3d11_texture_desc, NULL, &p_frame);
	ID3D11DeviceContext_CopyResource(dev->ctx, (ID3D11Resource*)p_frame, (ID3D11Resource*)frame->frame);

	ID3D11Texture2D_QueryInterface(p_frame, &IID_IDXGISurface, &p_dxgi_surface);
	if (p_frame) {
		ID3D11Texture2D_Release(p_frame);
		p_frame = NULL;
	}
	IDXGISurface_Map(p_dxgi_surface, &dxgi_mapped_rect, DXGI_MAP_READ);

	for (int i = 0; i < (int)dup->height; i++) {
		fwrite(dxgi_mapped_rect.pBits + i * dxgi_mapped_rect.Pitch, dxgi_mapped_rect.Pitch, 1, bgra);
	}
	IDXGISurface_Unmap(p_dxgi_surface);

	if (p_dxgi_surface) {
		IDXGISurface_Release(p_dxgi_surface);
		p_dxgi_surface = NULL;
	}
}

void platform_video_capture(synchronized_queue_t* queue) {
	dxgi_device_t dev;
	dxgi_dup_t dup;

	memset(&dev, 0, sizeof(dxgi_device_t));
	memset(&dup, 0, sizeof(dxgi_dup_t));

	dxgi_create_device(&dev);
	dxgi_create_duplicator(&dev, &dup);

	dxgi_status_t status;
	while (true) {
		dxgi_frame_t* frame = malloc(sizeof(dxgi_frame_t));
		if (!frame) {
			return;
		}
		memset(frame, 0, sizeof(dxgi_frame_t));

		frame->width = dup.width;
		frame->height = dup.height;

		status = dxgi_capture_frame(&dup, frame);
		if (status == DXGI_STATUS_TIMEOUT || status == DXGI_STATUS_NO_UPDATE || status == DXGI_STATUS_FAILURE) {
			continue;
		}
		if (status == DXGI_STATUS_RECREATE_DUP) {
			printf("need re-create dup.\n");
			dxgi_create_duplicator(&dev, &dup);
			continue;
		}
		synchronized_queue_enqueue(queue, &frame->node);
		dump_bgra(frame, &dev, &dup);
	}
}