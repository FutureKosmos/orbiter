_Pragma("once")

#include "common/synchronized-queue.h"

extern void video_capture_start(synchronized_queue_t* p_nalus);
extern void video_capture_stop(void);