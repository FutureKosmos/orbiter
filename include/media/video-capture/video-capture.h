_Pragma("once")

#include "common/synchronized-queue.h"

typedef struct video_frame_s {
	uint8_t* bitstream;
	uint64_t bslen;
	uint64_t dts;
	uint64_t pts;
	bool keyframe;
	synchronized_queue_node_t node;
}video_frame_t;

extern void video_capture(synchronized_queue_t* p_frames);
