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

typedef enum video_capture_pixelfmt_e {
	VIDEO_CAPTURE_PIXEL_FMT_NONE,
	VIDEO_CAPTURE_PIXEL_FMT_NV12,
	VIDEO_CAPTURE_PIXEL_FMT_BGRA, /* B in the lowest 8 bits and A in the highest 8 bits. */
	VIDEO_CAPTURE_PIXEL_FMT_END
}video_capture_pixelfmt_t;

typedef struct video_capture_conf_s {
	struct {
		int width;
		int height;
	}resolution;

	int framerate;
	int bitrate;

	video_capture_pixelfmt_t pixelfmt;
}video_capture_conf_t;

extern void video_capture_create(video_capture_conf_t conf);
extern void video_capture(synchronized_queue_t* p_frames);
extern void video_capture_reconfigure(int width, int height);
extern void video_capture_destroy(void);