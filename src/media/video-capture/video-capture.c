#include "media/video-capture/video-capture-types.h"
#include "platform/platform-video-capture.h"

void video_capture(video_capture_frame_t* frame, video_capture_type_t type) {
	platform_video_capture(frame, type);
}


//void dump_bgra(dxgi_frame_t* frame, d3d11_device_t* d3d_dev, dxgi_dup_t* dup) {
//	static FILE* bgra;
//	D3D11_TEXTURE2D_DESC d3d11_texture_desc;
//	ID3D11Texture2D* p_frame;
//	IDXGISurface* p_dxgi_surface;
//	DXGI_MAPPED_RECT     dxgi_mapped_rect;
//
//	if (!bgra) {
//		bgra = fopen("dump.bgra", "ab+");
//	}
//
//	ID3D11Texture2D_GetDesc(frame->frame, &d3d11_texture_desc);
//	d3d11_texture_desc.MipLevels = 1;
//	d3d11_texture_desc.ArraySize = 1;
//	d3d11_texture_desc.SampleDesc.Count = 1;
//	d3d11_texture_desc.Usage = D3D11_USAGE_STAGING;
//	d3d11_texture_desc.BindFlags = 0;
//	d3d11_texture_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
//	d3d11_texture_desc.MiscFlags = 0;
//
//	ID3D11Device_CreateTexture2D(d3d_dev->dev, &d3d11_texture_desc, NULL, &p_frame);
//	ID3D11DeviceContext_CopyResource(d3d_dev->ctx, (ID3D11Resource*)p_frame, (ID3D11Resource*)frame->frame);
//
//	ID3D11Texture2D_QueryInterface(p_frame, &IID_IDXGISurface, &p_dxgi_surface);
//	if (p_frame) {
//		ID3D11Texture2D_Release(p_frame);
//		p_frame = NULL;
//	}
//	IDXGISurface_Map(p_dxgi_surface, &dxgi_mapped_rect, DXGI_MAP_READ);
//
//	for (int i = 0; i < (int)dup->height; i++) {
//		fwrite(dxgi_mapped_rect.pBits + i * dxgi_mapped_rect.Pitch, dxgi_mapped_rect.Pitch, 1, bgra);
//	}
//	IDXGISurface_Unmap(p_dxgi_surface);
//
//	if (p_dxgi_surface) {
//		IDXGISurface_Release(p_dxgi_surface);
//		p_dxgi_surface = NULL;
//	}
//}