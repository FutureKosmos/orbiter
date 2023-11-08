_Pragma("once")

#include "common/synchronized-queue.h"
#include "media/video-capture/video-capture.h"

extern void platform_video_capture_create(video_capture_conf_t conf);
extern void platform_video_capture(synchronized_queue_t* p_frames);
extern void platform_video_capture_reconfigure(video_capture_conf_t conf);
extern void platform_video_capture_destroy(void);