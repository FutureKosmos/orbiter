#include "cdk.h"
#include "common/synchronized-queue.h"
#include "media/video-capture/video-capture.h"

int video_capture_thread(void* param) {
	synchronized_queue_t* queue = param;
	video_capture(queue);
	return 0;
}

void create_video_capture_thread(synchronized_queue_t* queue) {
	thrd_t tid;
	thrd_create(&tid, video_capture_thread, queue);
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

