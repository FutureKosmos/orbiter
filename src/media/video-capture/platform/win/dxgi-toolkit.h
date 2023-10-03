_Pragma("once")

#include "d3d11-toolkit.h"

typedef enum dxgi_status_e {
	DXGI_STATUS_RECREATE_DUP,
	DXGI_STATUS_TIMEOUT,
	DXGI_STATUS_NO_UPDATE,
	DXGI_STATUS_FAILURE,
	DXGI_STATUS_SUCCESS
}dxgi_status_t;

typedef struct dxgi_frame_s {
	DXGI_OUTDUPL_FRAME_INFO frameinfo;
	ID3D11Texture2D* p_texture2d;
}dxgi_frame_t;

typedef struct dxgi_duplicator_s {
	IDXGIOutputDuplication* p_duplication;
	int width;
	int height;
}dxgi_duplicator_t;

dxgi_duplicator_t duplicator_;

extern void dxgi_duplicator_create(void);
extern void dxgi_duplicator_destroy(void);
extern int dxgi_capture_frame(dxgi_frame_t* p_frame);