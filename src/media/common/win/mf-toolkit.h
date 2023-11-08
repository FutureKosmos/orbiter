_Pragma("once")

#include "directx-toolkit.h"
#include "media/video-capture/video-capture.h"
#include "common/string-utils.h"
#include <mfapi.h>
#include <mfobjects.h>
#include <icodecapi.h>
#include <mftransform.h>
#include <Mferror.h>
#include <Codecapi.h>

#pragma comment(lib, "Mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "strmiids.lib")

typedef struct mf_video_encoder_s {
	IMFMediaEventGenerator* p_gen;
	ICodecAPI* p_codec_api;
	IMFTransform* p_trans;
	IMFDXGIDeviceManager* p_manager;
	bool async_need_input;
	bool async_have_output;
	DWORD in_stm_id;
	DWORD out_stm_id;
}mf_video_encoder_t;

extern void mf_hw_video_encode(mf_video_encoder_t* p_encoder, ID3D11Texture2D* p_indata, video_frame_t* p_outdata);
extern bool mf_hw_video_encoder_create(directx_device_t* p_device, int bitrate, int framerate, int width, int height, mf_video_encoder_t* p_encoder);
extern void mf_hw_video_encoder_reconfigure(int bitrate, int framerate, int width, int height);
extern void mf_hw_video_encoder_destroy(mf_video_encoder_t* p_encoder);
extern void mf_dump_video(const char* filename, video_frame_t* frame);