#include "dxgi-toolkit.h"
#include "d3d11-toolkit.h"
#include "common/synchronized-queue.h"

void platform_video_capture(synchronized_queue_t* p_nalus) {
	dxgi_frame_t frame;
	ID3D11Texture2D* p_tex2d = NULL;
	D3D11_TEXTURE2D_DESC desc;

	d3d11_device_create();
	dxgi_duplicator_create();
	d3d11_texture2d_create(&p_tex2d, duplicator_.width, duplicator_.height);
	ID3D11Texture2D_GetDesc(p_tex2d, &desc);
	d3d11_video_processor_create(duplicator_.width, duplicator_.height, desc.Width, desc.Height);

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
			d3d11_texture2d_destroy(p_tex2d);
			d3d11_texture2d_create(&p_tex2d, duplicator_.width, duplicator_.height);
			ID3D11Texture2D_GetDesc(p_tex2d, &desc);
		}
		d3d11_bgra_to_nv12(frame.p_texture2d, p_tex2d);
	}
	dxgi_duplicator_destroy();
	d3d11_device_destroy();
	d3d11_video_processor_destroy();
	d3d11_texture2d_destroy(p_tex2d);
}