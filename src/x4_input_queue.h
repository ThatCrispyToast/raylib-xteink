#ifndef X4_INPUT_QUEUE_H
#define X4_INPUT_QUEUE_H
#include "x4_hal.h"
#include <string.h>
typedef struct {
    uint8_t keys[X4_INPUT_CAPACITY], count;
    unsigned int dropped;
} X4InputQueue;
// Call under the HAL's input lock when producer and consumer are different tasks.
static inline void x4_input_push(X4InputQueue *queue, uint8_t key)
{
    if (queue->count < X4_INPUT_CAPACITY) queue->keys[queue->count++]=key;
    else queue->dropped++;
}
static inline void x4_input_drain(X4InputQueue *queue, X4Input *input)
{
    input->press_count=queue->count;
    memcpy(input->press_order,queue->keys,queue->count);
    for (int i=0; i<queue->count; i++) input->pressed |= 1u<<queue->keys[i];
    queue->count=0;
}
#endif
