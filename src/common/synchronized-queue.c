#include "common/synchronized-queue.h"

void synchronized_queue_create(synchronized_queue_t* queue) {
	cdk_queue_init(&queue->data);
	mtx_init(&queue->lock, mtx_plain);
	cnd_init(&queue->cond);
}

void synchronized_queue_enqueue(synchronized_queue_t* queue, synchronized_queue_node_t* node) {
	mtx_lock(&queue->lock);
	cdk_queue_enqueue(&queue->data, node);
	cnd_signal(&queue->cond);
	mtx_unlock(&queue->lock);
}

synchronized_queue_node_t* synchronized_queue_dequeue(synchronized_queue_t* queue) {
	mtx_lock(&queue->lock);
	while (synchronized_queue_empty(queue)) {
		cnd_wait(&queue->cond, &queue->lock);
	}
	cdk_queue_node_t* node = cdk_queue_dequeue(&queue->data);
	mtx_unlock(&queue->lock);
	return node;
}

bool synchronized_queue_empty(synchronized_queue_t* queue) {
	return cdk_queue_empty(&queue->data);
}

void synchronized_queue_destroy(synchronized_queue_t* queue) {
	mtx_destroy(&queue->lock);
	cnd_destroy(&queue->cond);
}