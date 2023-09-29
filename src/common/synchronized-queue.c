#include "common/synchronized-queue.h"

void synchronized_queue_init(synchronized_queue_t* queue) {
	cdk_queue_init(&queue->q);
	mtx_init(&queue->m, mtx_plain);
}

void synchronized_queue_enqueue(synchronized_queue_t* queue, synchronized_queue_node_t* node) {
	mtx_lock(&queue->m);
	cdk_queue_enqueue(&queue->q, &node->q);
	mtx_unlock(&queue->m);
}

synchronized_queue_node_t* synchronized_queue_dequeue(synchronized_queue_t* queue) {

}

bool synchronized_queue_empty(synchronized_queue_t* queue) {

}