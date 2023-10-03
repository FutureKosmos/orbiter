#include "cdk.h"
#include "common/synchronized-queue.h"
#include "media/video-capture/video-capture.h"

int video_capture_thread(void* param) {
	synchronized_queue_t* queue = param;
	video_capture_start(queue);
	return 0;
}

int video_encode_thread(void* param) {
	synchronized_queue_t** queues = param;
	synchronized_queue_t* in = queues[0];
	synchronized_queue_t* out = queues[1];

	free(queues);

	/*while (true) {
		video_capture_frame_t* frame
			= synchronized_queue_data(synchronized_queue_dequeue(in), video_capture_frame_t, node);

		video_encode_frame_t* bitstream = video_encode(frame);
		synchronized_queue_enqueue(out, &bitstream->node);
	}*/
	return 0;
}

void create_video_capture_thread(synchronized_queue_t* queue) {
	thrd_t tid;
	thrd_create(&tid, video_capture_thread, queue);
	thrd_detach(tid);
}

void create_video_encode_thread(synchronized_queue_t* in, synchronized_queue_t* out) {
	synchronized_queue_t** queues = malloc(sizeof(void*) * 2);
	if (!queues) {
		return;
	}
	queues[0] = in;
	queues[1] = out;

	thrd_t tid;
	thrd_create(&tid, video_encode_thread, queues);
	thrd_detach(tid);
}

int main(void) {
	cdk_logger_create(NULL, 1);

	synchronized_queue_t nalus;

	synchronized_queue_create(&nalus);

	create_video_capture_thread(&nalus);

	

	while (true) {
		cdk_time_sleep(5000);
	}

	synchronized_queue_destroy(&nalus);
	cdk_logger_destroy();
	return 0;
}

