_Pragma("once")

#include "common.h"

extern void d3d11_video_device_create(void);
extern void d3d11_video_device_destroy(void);
extern void d3d11_bgra_to_nv12(ID3D11Texture2D* p_in, ID3D11Texture2D* p_out);