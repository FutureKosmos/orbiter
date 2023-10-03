_Pragma("once")

#include "common/synchronized-queue.h"

extern void platform_video_capture_start(synchronized_queue_t* p_nalus);
extern void platform_video_capture_stop(void);