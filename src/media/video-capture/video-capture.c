#include "platform/platform-video-capture.h"

void video_capture_start(synchronized_queue_t* p_nalus) {
	platform_video_capture_start(p_nalus);
}

void video_capture_stop(void) {
	platform_video_capture_stop();
}