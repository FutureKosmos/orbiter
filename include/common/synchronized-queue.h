_Pragma("once")

#include "cdk.h"

typedef struct synchronized_queue_s {
	cdk_queue_t data;
	mtx_t lock;
	cnd_t cond;
}synchronized_queue_t;

typedef cdk_queue_node_t synchronized_queue_node_t;

#define synchronized_queue_data cdk_list_data

extern void synchronized_queue_create(synchronized_queue_t* queue);
extern void synchronized_queue_enqueue(synchronized_queue_t* queue, synchronized_queue_node_t* node);
extern synchronized_queue_node_t* synchronized_queue_dequeue(synchronized_queue_t* queue);
extern bool synchronized_queue_empty(synchronized_queue_t* queue);
extern void synchronized_queue_destroy(synchronized_queue_t* queue);