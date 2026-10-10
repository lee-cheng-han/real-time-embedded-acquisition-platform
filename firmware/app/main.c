#include "FreeRTOS.h"
#include "task.h"

#include "drats/block_pool.h"
#include "drats/acquisition.h"
#include "drats/handle_queue.h"
#include "drats/supervisor.h"
#include "interrupts.h"
#include "uart3.h"
#include "drats/protocol.h"

#include <stdint.h>

enum {
    PRIORITY_DIAGNOSTICS = 1,
    PRIORITY_SUPERVISOR = 2,
    PRIORITY_TELEMETRY = 3,
    PRIORITY_PROCESSING = 4,
    PRIORITY_ACQUISITION = 5,
    TASK_STACK_WORDS = 256,
    IDLE_STACK_WORDS = 192
};

_Static_assert(PRIORITY_ACQUISITION < configMAX_PRIORITIES,
               "task priority exceeds FreeRTOS configuration");

static drats_block_pool_t block_pool;
static drats_handle_queue_t ready_queue;
static drats_handle_queue_t telemetry_queue;
static drats_supervisor_t supervisor;
static drats_acquisition_t acquisition;

static StaticTask_t acquisition_tcb;
static StaticTask_t processing_tcb;
static StaticTask_t telemetry_tcb;
static StaticTask_t supervisor_tcb;
static StaticTask_t diagnostics_tcb;
static StackType_t acquisition_stack[TASK_STACK_WORDS];
static StackType_t processing_stack[TASK_STACK_WORDS];
static StackType_t telemetry_stack[TASK_STACK_WORDS];
static StackType_t supervisor_stack[TASK_STACK_WORDS];
static StackType_t diagnostics_stack[TASK_STACK_WORDS];

static StaticTask_t idle_tcb;
static StackType_t idle_stack[IDLE_STACK_WORDS];
static uint8_t telemetry_frame[DRATS_PROTOCOL_MAX_FRAME];
static uint8_t telemetry_payload[12U +
    (DRATS_SAMPLES_PER_BLOCK * DRATS_CHANNEL_COUNT * 2U)];

static void put_u16(uint8_t *destination, uint16_t value) {
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8U);
}

static void put_u32(uint8_t *destination, uint32_t value) {
    for (unsigned i = 0; i < 4U; ++i) {
        destination[i] = (uint8_t)(value >> (8U * i));
    }
}

static drats_status_t transmit_block(const drats_sample_block_t *block) {
    put_u32(&telemetry_payload[0], block->sequence_first);
    put_u32(&telemetry_payload[4], block->sample_period_ns);
    put_u16(&telemetry_payload[8], block->sample_count);
    put_u16(&telemetry_payload[10], DRATS_CHANNEL_COUNT);
    size_t offset = 12U;
    for (size_t sample = 0; sample < block->sample_count; ++sample) {
        for (size_t channel = 0; channel < DRATS_CHANNEL_COUNT; ++channel) {
            put_u16(&telemetry_payload[offset],
                    (uint16_t)block->samples[sample][channel]);
            offset += 2U;
        }
    }
    const drats_frame_header_t header = {
        .version = DRATS_PROTOCOL_VERSION,
        .type = DRATS_MSG_SAMPLE_BLOCK,
        .flags = block->flags,
        .sequence = block->sequence_first,
        .timestamp_ns = block->timestamp_first_ns,
        .payload_length = (uint16_t)offset
    };
    size_t frame_length = 0U;
    drats_status_t status = drats_frame_encode(
        &header, telemetry_payload, telemetry_frame,
        sizeof(telemetry_frame), &frame_length);
    if (status == DRATS_OK) {
        status = drats_uart3_write(telemetry_frame, frame_length);
    }
    return status;
}

static void heartbeat(drats_task_id_t task) {
    (void)drats_supervisor_heartbeat(&supervisor, task,
                                     (uint64_t)xTaskGetTickCount());
}

static void acquisition_task(void *argument) {
    (void)argument;
    TickType_t wake_time = xTaskGetTickCount();
    uint32_t synthetic_value = 0U;
    for (;;) {
        heartbeat(DRATS_TASK_ACQUISITION);
        drats_sample_block_t *block = NULL;
        if (drats_acq_arm_next(&acquisition, &block) == DRATS_OK) {
            for (size_t sample = 0; sample < DRATS_SAMPLES_PER_BLOCK; ++sample) {
                for (size_t channel = 0; channel < DRATS_CHANNEL_COUNT; ++channel) {
                    block->samples[sample][channel] =
                        (int16_t)(synthetic_value & UINT32_C(0x7fff));
                    ++synthetic_value;
                }
            }
            const uint64_t timestamp_ns =
                (uint64_t)xTaskGetTickCount() * UINT64_C(1000000);
            (void)drats_acq_complete_dma(&acquisition, timestamp_ns,
                                         DRATS_SAMPLES_PER_BLOCK);
        }
        vTaskDelayUntil(&wake_time, pdMS_TO_TICKS(32U));
    }
}

static void processing_task(void *argument) {
    (void)argument;
    for (;;) {
        heartbeat(DRATS_TASK_PROCESSING);
        drats_block_handle_t handle;
        if (drats_handle_queue_pop(&ready_queue, &handle) == DRATS_OK) {
            if (drats_block_transition(&block_pool, handle, DRATS_BLOCK_READY,
                                       DRATS_BLOCK_PROCESSING) == DRATS_OK) {
                (void)drats_block_transition(&block_pool, handle,
                                             DRATS_BLOCK_PROCESSING,
                                             DRATS_BLOCK_TELEMETRY);
                (void)drats_handle_queue_push(&telemetry_queue, handle);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
}

static void telemetry_task(void *argument) {
    (void)argument;
    bool has_pending = false;
    drats_block_handle_t pending = {0};
    for (;;) {
        heartbeat(DRATS_TASK_TELEMETRY);
        if (!has_pending &&
            (drats_handle_queue_pop(&telemetry_queue, &pending) == DRATS_OK)) {
            has_pending = true;
        }
        if (has_pending) {
            drats_sample_block_t *block = NULL;
            if (drats_block_get(&block_pool, pending, DRATS_BLOCK_TELEMETRY,
                                &block) != DRATS_OK) {
                has_pending = false;
            } else if (transmit_block(block) == DRATS_OK) {
                (void)drats_block_transition(&block_pool, pending,
                                             DRATS_BLOCK_TELEMETRY,
                                             DRATS_BLOCK_FREE);
                has_pending = false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
}

static void supervisor_task(void *argument) {
    (void)argument;
    for (;;) {
        heartbeat(DRATS_TASK_SUPERVISOR);
        (void)drats_supervisor_watchdog_eligible(
            &supervisor, (uint64_t)xTaskGetTickCount());
        vTaskDelay(pdMS_TO_TICKS(10U));
    }
}

static void diagnostics_task(void *argument) {
    (void)argument;
    for (;;) {
        heartbeat(DRATS_TASK_DIAGNOSTICS);
        vTaskDelay(pdMS_TO_TICKS(250U));
    }
}

void vApplicationGetIdleTaskMemory(StaticTask_t **tcb,
                                   StackType_t **stack,
                                   configSTACK_DEPTH_TYPE *stack_size) {
    *tcb = &idle_tcb;
    *stack = idle_stack;
    *stack_size = IDLE_STACK_WORDS;
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name) {
    (void)task;
    (void)task_name;
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

void drats_assert_failed(const char *file, int line) {
    (void)file;
    (void)line;
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

int main(void) {
    drats_block_pool_init(&block_pool);
    drats_handle_queue_init(&ready_queue);
    drats_handle_queue_init(&telemetry_queue);
    const drats_acq_config_t acquisition_config = {
        .sample_rate_hz = 1000U,
        .samples_per_block = DRATS_SAMPLES_PER_BLOCK,
        .overflow_policy = DRATS_OVERFLOW_STOP
    };
    if ((drats_acq_init(&acquisition, &block_pool, &ready_queue,
                        &acquisition_config) != DRATS_OK) ||
        (drats_acq_start(&acquisition) != DRATS_OK)) {
        drats_assert_failed(__FILE__, __LINE__);
    }
    const uint32_t deadlines[DRATS_SUPERVISOR_TASK_COUNT] = {
        50U, 20U, 50U, 1000U, 100U
    };
    const uint32_t required = (UINT32_C(1) << DRATS_TASK_ACQUISITION) |
                              (UINT32_C(1) << DRATS_TASK_PROCESSING) |
                              (UINT32_C(1) << DRATS_TASK_TELEMETRY) |
                              (UINT32_C(1) << DRATS_TASK_DIAGNOSTICS) |
                              (UINT32_C(1) << DRATS_TASK_SUPERVISOR);
    if (drats_supervisor_init(&supervisor, deadlines, required) != DRATS_OK) {
        drats_assert_failed(__FILE__, __LINE__);
    }
    if (drats_uart3_init(SystemCoreClock, 2000000U) != DRATS_OK) {
        drats_assert_failed(__FILE__, __LINE__);
    }

    TaskHandle_t acquisition_handle = xTaskCreateStatic(
        acquisition_task, "acquisition", TASK_STACK_WORDS, NULL,
        PRIORITY_ACQUISITION, acquisition_stack, &acquisition_tcb);
    drats_irq_set_acquisition_task(acquisition_handle);
    (void)xTaskCreateStatic(processing_task, "processing", TASK_STACK_WORDS,
                            NULL, PRIORITY_PROCESSING, processing_stack,
                            &processing_tcb);
    (void)xTaskCreateStatic(telemetry_task, "telemetry", TASK_STACK_WORDS,
                            NULL, PRIORITY_TELEMETRY, telemetry_stack,
                            &telemetry_tcb);
    (void)xTaskCreateStatic(supervisor_task, "supervisor", TASK_STACK_WORDS,
                            NULL, PRIORITY_SUPERVISOR, supervisor_stack,
                            &supervisor_tcb);
    (void)xTaskCreateStatic(diagnostics_task, "diagnostics", TASK_STACK_WORDS,
                            NULL, PRIORITY_DIAGNOSTICS, diagnostics_stack,
                            &diagnostics_tcb);
    vTaskStartScheduler();
    drats_assert_failed(__FILE__, __LINE__);
    return 0;
}
