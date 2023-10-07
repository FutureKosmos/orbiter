#include "cdk.h"
#include "common/synchronized-queue.h"
#include "media/video-capture/video-capture.h"

static int _video_capture_thread(void* param) {
	synchronized_queue_t* queue = param;
	video_capture(queue);
	return 0;
}

static int _network_send_thread(void* param) {
	synchronized_queue_t* queue = param;
	while (true) {
		video_frame_t* frame = synchronized_queue_data(synchronized_queue_dequeue(queue), video_frame_t, node);

		//// here handle data.
		////
		////
		

		if (frame->bitstream) {
			free(frame->bitstream);
			frame->bitstream = NULL;
		}
		if (frame) {
			free(frame);
			frame = NULL;
		}
	}
	return 0;
}

void create_video_capture_thread(synchronized_queue_t* queue) {
	thrd_t tid;
	thrd_create(&tid, _video_capture_thread, queue);
	thrd_detach(tid);
}

void create_network_send_thread(synchronized_queue_t* queue) {
	thrd_t tid;
	thrd_create(&tid, _network_send_thread, queue);
	thrd_detach(tid);
}

int main(void) {
	cdk_logger_create(NULL, 1);

	synchronized_queue_t frames;
	synchronized_queue_create(&frames);

	create_video_capture_thread(&frames);
	create_network_send_thread(&frames);

	while (true) {
		cdk_time_sleep(5000);
	}
	synchronized_queue_destroy(&frames);
	cdk_logger_destroy();
	return 0;
}

