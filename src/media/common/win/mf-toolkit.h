_Pragma("once")

#include "d3d11-toolkit.h"

#pragma comment(lib, "Mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "strmiids.lib")

extern void mf_hw_video_encode(ID3D11Texture2D* p_indata, uint8_t* p_outdata);
extern void mf_hw_video_encoder_create(int bitrate, int framerate, int width, int height);
extern void mf_hw_video_encoder_destroy();