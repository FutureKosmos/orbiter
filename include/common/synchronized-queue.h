_Pragma("once")

#include "cdk.h"

typedef struct synchronized_queue_s {
	cdk_queue_t que;
	mtx_t mtx;
}synchronized_queue_t;

typedef struct synchronized_queue_node_s {
	cdk_queue_node_t node;
	mtx_t m;
}synchronized_queue_node_t;

#define synchronized_queue_data cdk_list_data

extern void synchronized_queue_init(synchronized_queue_t* queue);
extern void synchronized_queue_enqueue(synchronized_queue_t* queue, synchronized_queue_node_t* node);
extern synchronized_queue_node_t* synchronized_queue_dequeue(synchronized_queue_t* queue);
extern bool synchronized_queue_empty(synchronized_queue_t* queue);