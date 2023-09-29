_Pragma("once")

#if defined(_WIN32)
#include <dxgi1_5.h>
#include <d3d11.h>

typedef struct video_capture_frame_s {
	DXGI_OUTDUPL_FRAME_INFO frame_info;
	ID3D11Texture2D*        frame;
}video_capture_frame_t;
#endif

typedef enum video_capture_type_e {
	DXGI_CAPTURE_TYPE
}video_capture_type_t;