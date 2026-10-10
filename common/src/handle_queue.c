#include "drats/handle_queue.h"

void drats_handle_queue_init(drats_handle_queue_t *queue) {
    if (queue == NULL) {
        return;
    }
    atomic_init(&queue->head, 0U);
    atomic_init(&queue->tail, 0U);
}

drats_status_t drats_handle_queue_push(drats_handle_queue_t *queue,
                                       drats_block_handle_t handle) {
    if (queue == NULL) {
        return DRATS_E_ARGUMENT;
    }
    const size_t head = atomic_load_explicit(&queue->head, memory_order_relaxed);
    const size_t next = (head + 1U) % DRATS_QUEUE_CAPACITY;
    if (next == atomic_load_explicit(&queue->tail, memory_order_acquire)) {
        return DRATS_E_FULL;
    }
    queue->entries[head] = handle;
    atomic_store_explicit(&queue->head, next, memory_order_release);
    return DRATS_OK;
}

drats_status_t drats_handle_queue_pop(drats_handle_queue_t *queue,
                                      drats_block_handle_t *handle) {
    if ((queue == NULL) || (handle == NULL)) {
        return DRATS_E_ARGUMENT;
    }
    const size_t tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    if (tail == atomic_load_explicit(&queue->head, memory_order_acquire)) {
        return DRATS_E_EMPTY;
    }
    *handle = queue->entries[tail];
    atomic_store_explicit(&queue->tail,
                          (tail + 1U) % DRATS_QUEUE_CAPACITY,
                          memory_order_release);
    return DRATS_OK;
}

size_t drats_handle_queue_size(const drats_handle_queue_t *queue) {
    if (queue == NULL) {
        return 0U;
    }
    const size_t head = atomic_load_explicit(&queue->head, memory_order_acquire);
    const size_t tail = atomic_load_explicit(&queue->tail, memory_order_acquire);
    return (head + DRATS_QUEUE_CAPACITY - tail) % DRATS_QUEUE_CAPACITY;
}
