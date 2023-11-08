#include "media/video-capture/video-capture.h"
#include "platform/platform-video-capture.h"

void video_capture_create(video_capture_conf_t conf) {
	platform_video_capture_create(conf);
}

void video_capture(synchronized_queue_t* p_frames) {
	platform_video_capture(p_frames);
}

void video_capture_reconfigure(video_capture_conf_t conf) {
	platform_video_capture_reconfigure(conf);
}

void video_capture_destroy(void) {
	platform_video_capture_destroy();
}