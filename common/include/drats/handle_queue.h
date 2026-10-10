#ifndef DRATS_HANDLE_QUEUE_H
#define DRATS_HANDLE_QUEUE_H

#include "drats/block_pool.h"
#include "drats/status.h"

#include <stdatomic.h>
#include <stddef.h>

#define DRATS_QUEUE_CAPACITY 8U

typedef struct {
    drats_block_handle_t entries[DRATS_QUEUE_CAPACITY];
    atomic_size_t head;
    atomic_size_t tail;
} drats_handle_queue_t;

void drats_handle_queue_init(drats_handle_queue_t *queue);
drats_status_t drats_handle_queue_push(drats_handle_queue_t *queue,
                                       drats_block_handle_t handle);
drats_status_t drats_handle_queue_pop(drats_handle_queue_t *queue,
                                      drats_block_handle_t *handle);
size_t drats_handle_queue_size(const drats_handle_queue_t *queue);

#endif
