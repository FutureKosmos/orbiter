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

typedef struct video_capture_conf_s {
	struct {
		int width;
		int height;
	}resolution;

	int framerate;
}video_capture_conf_t;

extern void video_capture_create(video_capture_conf_t conf);
extern void video_capture(synchronized_queue_t* p_frames);
extern void video_capture_update_resolution(int width, int height);
extern void video_capture_force_keyframe(void);
extern void video_capture_destroy(void);