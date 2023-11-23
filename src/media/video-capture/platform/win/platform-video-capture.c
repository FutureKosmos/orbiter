#include "media/common/win/directx-toolkit.h"
#include "media/common/win/mf-toolkit.h"
#include "media/video-capture/video-capture.h"

static directx_device_t device_;
static video_capture_conf_t config_;
static directx_duplicator_t duplicator_;
static directx_video_processor_t processor_;
static mf_video_encoder_t mfencoder_;
static ID3D11Texture2D* p_tex2d_;
static mtx_t mtx_;

static inline int _estimated_bitrate(int framerate, int width, int height) {
	/**
	 * bitrate = resolution x framerate x multiplier.
	 *
	 * multiplier = 0.1 (baseline)
	 * multiplier = 0.2 (main)
	 * multiplier = 0.3 (high)
	 */
	double multiplier = 0.1;
	return framerate * width * height;
}

void platform_video_capture_create(video_capture_conf_t conf) {
	mtx_init(&mtx_, mtx_plain);

	if (!directx_device_create(&device_)) {
		cdk_loge("failed to directx_device_create\n");
		return;
	}
	if (!directx_duplicator_create(&device_, &duplicator_)) {
		cdk_loge("failed to directx_duplicator_create\n");
		return;
	}
	if (!directx_texture2d_create(&device_, conf.resolution.width, conf.resolution.height, DXGI_FORMAT_NV12, &p_tex2d_)) {
		cdk_loge("failed to directx_texture2d_create\n");
		return;
	}
	if (!directx_video_processor_create(&device_, duplicator_.w, duplicator_.h, conf.resolution.width, conf.resolution.height, &processor_)) {
		cdk_loge("failed to directx_video_processor_create\n");
		return;
	}
	if (!mf_hw_video_encoder_create(&device_, _estimated_bitrate(conf.framerate, conf.resolution.width, conf.resolution.height), conf.framerate, conf.resolution.width, conf.resolution.height, &mfencoder_)) {
		cdk_loge("failed to mf_hw_video_encoder_create\n");
		return;
	}
	config_ = conf;
}

void platform_video_capture_update_resolution(int width, int height) {
	mtx_lock(&mtx_);
	if (width != config_.resolution.width || height != config_.resolution.height) {
		directx_texture2d_destroy(p_tex2d_);
		if (!directx_texture2d_create(&device_, width, height, DXGI_FORMAT_NV12, &p_tex2d_)) {
			cdk_loge("failed to directx_texture2d_create\n");
			return;
		}
		directx_video_processor_destroy(&processor_);
		if (!directx_video_processor_create(&device_, duplicator_.w, duplicator_.h, width, height, &processor_)) {
			cdk_loge("failed to directx_video_processor_create\n");
			return;
		}
		mf_hw_video_encoder_destroy(&mfencoder_);
		if (!mf_hw_video_encoder_create(&device_, _estimated_bitrate(config_.framerate, width, height), config_.framerate, width, height, &mfencoder_)) {
			cdk_loge("failed to mf_hw_video_encoder_create\n");
			return;
		}
	}
	config_.resolution.width = width;
	config_.resolution.height = height;
	mtx_unlock(&mtx_);
}

void platform_video_capture_force_keyframe(void) {
	mtx_lock(&mtx_);
	mf_hw_video_encoder_force_keyframe(&mfencoder_);
	mtx_unlock(&mtx_);
}

void platform_video_capture_destroy(void) {
	directx_device_destroy(&device_);
	directx_texture2d_destroy(p_tex2d_);
	directx_duplicator_destroy(&duplicator_);
	directx_video_processor_destroy(&processor_);
	mf_hw_video_encoder_destroy(&mfencoder_);
	mtx_destroy(&mtx_);
}

void platform_video_capture(synchronized_queue_t* p_frames) {
	HRESULT hr = S_OK;
	directx_duplicator_frame_t frame;	/** resource release by dxgi_capture_frame. */
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
			continue;
		}
		mtx_lock(&mtx_);
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
		mtx_unlock(&mtx_);
	}
}