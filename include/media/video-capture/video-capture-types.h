_Pragma("once")

#include "common/synchronized-queue.h"

#if defined(_WIN32)
#include <dxgi1_5.h>
#include <d3d11.h>

typedef struct video_capture_frame_s {
	DXGI_OUTDUPL_FRAME_INFO info;
	ID3D11Texture2D* frame;
	int width;
	int height;
	synchronized_queue_node_t node;
}video_capture_frame_t;
#endif