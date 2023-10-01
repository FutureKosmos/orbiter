#include "platform/platform-video-encode.h"

video_encode_frame_t* video_encode(video_capture_frame_t* frame) {
	return platform_video_encode(frame);
}