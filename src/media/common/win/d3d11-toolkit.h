_Pragma("once")

#include "cdk.h"

#define COBJMACROS
#include <dxgi1_5.h>
#include <d3d11.h>

#pragma comment(lib, "d3d11.lib")

#define SAFE_RELEASE(u) \
    do { if ((u) != NULL) (u)->lpVtbl->Release(u); (u) = NULL; } while(0)

typedef struct d3d11_device_s {
	ID3D11Device* p_device;
	ID3D11DeviceContext* p_context;
	int vendor;
}d3d11_device_t;

d3d11_device_t d3d11_;

typedef struct d3d11_video_processor_s {
	ID3D11VideoProcessor* p_processor;
	ID3D11VideoProcessorEnumerator* p_enumerator;
}d3d11_video_processor_t;

d3d11_video_processor_t d3d11_vp_;

extern void d3d11_device_create(void);
extern void d3d11_device_destroy(void);
extern void d3d11_texture2d_create(ID3D11Texture2D** pp_texture2d, int width, int height);
extern void d3d11_texture2d_destroy(ID3D11Texture2D* p_texture2d);
extern void d3d11_video_processor_create(int in_width, int in_height, int out_width, int out_height);
extern void d3d11_bgra_to_nv12(ID3D11Texture2D* p_in, ID3D11Texture2D* p_out);
extern void d3d11_video_processor_destroy(void);
extern void d3d11_dump_rawdata(const char* filename, ID3D11Texture2D* p_texture2d, DXGI_FORMAT fmt);