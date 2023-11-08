_Pragma("once")

#include "cdk.h"

#define COBJMACROS
#include <dxgi1_5.h>
#include <d3d11.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

#define VENDOR_ID_NVIDIA        0x10de
#define VENDOR_ID_AMD           0x1002
#define VENDOR_ID_MICROSOFT     0x1414
#define VENDOR_ID_INTEL         0x8086

#define SAFE_RELEASE(u) \
    do { if ((u) != NULL) (u)->lpVtbl->Release(u); (u) = NULL; } while(0)

typedef struct directx_device_s {
	ID3D11Device* p_dev;
	ID3D11DeviceContext* p_ctx;
	int vendor;
}directx_device_t;

typedef struct directx_video_processor_s {
	ID3D11VideoDevice* p_dev;
	ID3D11VideoContext* p_ctx;
	ID3D11VideoProcessor* p_vp;
	ID3D11VideoProcessorEnumerator* p_vpe;
}directx_video_processor_t;

typedef struct directx_duplicator_frame_s {
	DXGI_OUTDUPL_FRAME_INFO fi;
	ID3D11Texture2D* p_tex2d;
}directx_duplicator_frame_t;

typedef struct directx_duplicator_s {
	IDXGIOutputDuplication* p_dup;
	int w;
	int h;
}directx_duplicator_t;


extern bool directx_device_create(directx_device_t* p_device);
extern void directx_device_destroy(directx_device_t* p_device);
extern bool directx_duplicator_create(directx_device_t* p_device, directx_duplicator_t* p_duplicator);
extern void directx_duplicator_destroy(directx_duplicator_t* p_duplicator);
extern HRESULT directx_duplicator_capture(directx_duplicator_t* p_duplicator, directx_duplicator_frame_t* p_frame);
extern bool directx_texture2d_create(directx_device_t* p_device, int width, int height, DXGI_FORMAT fmt, ID3D11Texture2D** pp_texture2d);
extern void directx_texture2d_destroy(ID3D11Texture2D* p_texture2d);
extern bool directx_video_processor_create(directx_device_t* p_device, int in_width, int in_height, int out_width, int out_height, directx_video_processor_t* p_processor);
extern void directx_video_processor_destroy(directx_video_processor_t* p_processor);
extern bool directx_bgra_to_nv12(directx_video_processor_t* p_processor, ID3D11Texture2D* p_in, ID3D11Texture2D* p_out);
extern void directx_texture2d_dump(directx_device_t* p_device, const char* filename, ID3D11Texture2D* p_texture2d, DXGI_FORMAT fmt);