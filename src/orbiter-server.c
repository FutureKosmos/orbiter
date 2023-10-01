

#include "cdk.h"
#include "common/synchronized-queue.h"
#include "media/video-capture/video-capture.h"
#include "media/video-encode/video-encode.h"

int video_capture_thread(void* param) {
	synchronized_queue_t* queue = param;
	video_capture(queue);
	return 0;
}

int video_encode_thread(void* param) {
	synchronized_queue_t** queues = param;

	while (true) {
		video_capture_frame_t* frame
			= synchronized_queue_data(synchronized_queue_dequeue(queues[0]), video_capture_frame_t, node);

		video_encode_frame_t* bitstream = video_encode(frame);
		synchronized_queue_enqueue(queues[1], &bitstream->node);
	}
	return 0;
}

void create_video_capture_thread(synchronized_queue_t* queue) {
	thrd_t tid;
	thrd_create(&tid, video_capture_thread, queue);
	thrd_detach(tid);
}

void create_video_encode_thread(synchronized_queue_t* in, synchronized_queue_t* out) {
	synchronized_queue_t* queues[] = {in, out};
	thrd_t tid;
	thrd_create(&tid, video_encode_thread, queues);
	thrd_detach(tid);
}

int main(void) {
	synchronized_queue_t captured;
	synchronized_queue_t encoded;

	synchronized_queue_create(&captured);
	synchronized_queue_create(&encoded);

	create_video_capture_thread(&captured);
	create_video_encode_thread(&captured, &encoded);

	

	

	synchronized_queue_destroy(&captured);
	synchronized_queue_destroy(&encoded);
	return 0;
}

