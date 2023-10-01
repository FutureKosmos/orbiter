_Pragma("once")

#include "common/synchronized-queue.h"

typedef struct video_encode_frame_s {
	uint8_t* bitstream;
	synchronized_queue_node_t node;
}video_encode_frame_t;