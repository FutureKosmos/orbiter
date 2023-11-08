#include "media/common/win/directx-toolkit.h"
#include "media/common/win/mf-toolkit.h"
#include "media/video-capture/video-capture.h"

static directx_device_t device_;
static video_capture_conf_t config_;
static directx_duplicator_t duplicator_;
static directx_video_processor_t processor_;
static mf_video_encoder_t mfencoder_;
static ID3D11Texture2D* p_tex2d_;


static inline DXGI_FORMAT _d3d11_format(video_capture_pixelfmt_t pixelfmt)
{
	switch (pixelfmt)
	{
	case VIDEO_CAPTURE_PIXEL_FMT_NV12:
		return DXGI_FORMAT_NV12;
	case VIDEO_CAPTURE_PIXEL_FMT_BGRA:
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	default:
		return DXGI_FORMAT_UNKNOWN;
	}
}

void platform_video_capture_create(video_capture_conf_t conf) {
	config_ = conf;

	if (!directx_device_create(&device_)) {
		cdk_loge("failed to directx_device_create\n");
		return;
	}
	if (!directx_duplicator_create(&device_, &duplicator_)) {
		cdk_loge("failed to directx_duplicator_create\n");
		return;
	}
	if (!directx_texture2d_create(&device_, config_.resolution.width, config_.resolution.height, _d3d11_format(config_.pixelfmt), &p_tex2d_)) {
		cdk_loge("failed to directx_texture2d_create\n");
		return;
	}
	if (config_.pixelfmt == VIDEO_CAPTURE_PIXEL_FMT_NV12) {
		if (!directx_video_processor_create(&device_, duplicator_.w, duplicator_.h, config_.resolution.width, config_.resolution.height, &processor_)) {
			cdk_loge("failed to directx_video_processor_create\n");
			return;
		}
	}
	if (config_.encfmt == VIDEO_CAPTURE_ENC_FMT_H264) {
		if (!mf_hw_video_encoder_create(&device_, config_.bitrate, config_.framerate, config_.resolution.width, config_.resolution.height, &mfencoder_)) {
			cdk_loge("failed to mf_hw_video_encoder_create\n");
			return;
		}
	}
	if (config_.encfmt == VIDEO_CAPTURE_ENC_FMT_HEVC) {
		//create nvidia or amd encoder by sdk
	}
}

void platform_video_capture_reconfigure(video_capture_conf_t conf) {

}

void platform_video_capture_destroy(void) {
	directx_device_destroy(&device_);
	directx_texture2d_destroy(p_tex2d_);
	directx_duplicator_destroy(&duplicator_);
	directx_video_processor_destroy(&processor_);
	mf_hw_video_encoder_destroy(&mfencoder_);
}

void platform_video_capture(synchronized_queue_t* p_frames) {
	HRESULT hr = S_OK;
	directx_duplicator_frame_t frame;	/** resource release by dxgi_capture_frame. */
	bool reset = false;

	while (true) {
		hr = directx_duplicator_capture(&duplicator_, &frame);
		if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
			continue;
		}
		if (hr != S_OK) {
			cdk_loge("re-create duplicator.\n");

			directx_duplicator_destroy(&duplicator_);
			if (!directx_duplicator_create(&device_, &duplicator_)) {
				cdk_loge("failed to directx_duplicator_create\n");
				return;
			}
			reset = true;
			continue;
		}
		if (reset) {
			if (config_.pixelfmt == VIDEO_CAPTURE_PIXEL_FMT_NV12) {
				directx_video_processor_destroy(&processor_);
				if (!directx_video_processor_create(&device_, duplicator_.w, duplicator_.h, config_.resolution.width, config_.resolution.height, &processor_)) {
					cdk_loge("failed to directx_video_processor_create\n");
					return;
				}
			}
			if (config_.encfmt == VIDEO_CAPTURE_ENC_FMT_H264) {
				mf_hw_video_encoder_destroy(&mfencoder_);
				if (!mf_hw_video_encoder_create(&device_, config_.bitrate, config_.framerate, config_.resolution.width, config_.resolution.height, &mfencoder_)) {
					cdk_loge("failed to mf_hw_video_encoder_create\n");
					return;
				}
			}
			if (config_.encfmt == VIDEO_CAPTURE_ENC_FMT_HEVC) {
				//create nvidia or amd encoder by sdk
			}
			reset = false;
		}
		if (!directx_bgra_to_nv12(&processor_, frame.p_tex2d, p_tex2d_)) {
			cdk_loge("failed to directx_bgra_to_nv12\n");
			return;
		}
		video_frame_t* p_frame = malloc(sizeof(video_frame_t));
		if (p_frame) {
			memset(p_frame, 0, sizeof(video_frame_t));
			mf_hw_video_encode(&mfencoder_, p_tex2d_, p_frame);
			synchronized_queue_enqueue(p_frames, &p_frame->node);
		}
	}
}