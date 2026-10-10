#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "drats/block_pool.h"
#include "drats/acquisition.h"
#include "drats/config_protocol.h"
#include "drats/handle_queue.h"
#include "drats/supervisor.h"
#include "interrupts.h"
#include "uart3.h"
#include "tim2.h"
#include "watchdog.h"
#include "sample_timer.h"
#include "drats/protocol.h"
#include "drats/stream_decoder.h"
#if defined(DRATS_REAL_SENSOR)
#include "drats/icm42688.h"
#include "spi1.h"
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>

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

static StaticQueue_t config_queue_object;
static StaticQueue_t ack_queue_object;
static uint8_t config_queue_storage[2U * sizeof(drats_config_command_t)];
static uint8_t ack_queue_storage[4U * sizeof(drats_config_ack_t)];
static QueueHandle_t config_queue;
static QueueHandle_t ack_queue;
static drats_stream_decoder_t command_decoder;
static uint32_t tx_frame_sequence;
static atomic_bool telemetry_enabled;
static uint32_t startup_reset_flags;
#if defined(DRATS_REAL_SENSOR)
static drats_icm42688_t sensor;
#endif

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
        .sequence = tx_frame_sequence,
        .timestamp_ns = block->timestamp_first_ns,
        .payload_length = (uint16_t)offset
    };
    size_t frame_length = 0U;
    drats_status_t status = drats_frame_encode(
        &header, telemetry_payload, telemetry_frame,
        sizeof(telemetry_frame), &frame_length);
    if (status == DRATS_OK) {
        status = drats_uart3_write(telemetry_frame, frame_length);
        if (status == DRATS_OK) {
            ++tx_frame_sequence;
        }
    }
    return status;
}

static drats_status_t transmit_ack(const drats_config_ack_t *ack) {
    uint8_t payload[DRATS_CONFIG_ACK_SIZE];
    drats_status_t status = drats_config_ack_encode(ack, payload);
    if (status != DRATS_OK) {
        return status;
    }
    const drats_frame_header_t header = {
        .version = DRATS_PROTOCOL_VERSION,
        .type = DRATS_MSG_CONFIG_ACK,
        .sequence = tx_frame_sequence,
        .payload_length = DRATS_CONFIG_ACK_SIZE
    };
    size_t frame_length = 0U;
    status = drats_frame_encode(
        &header, payload, telemetry_frame, sizeof(telemetry_frame),
        &frame_length);
    if (status == DRATS_OK) {
        status = drats_uart3_write(telemetry_frame, frame_length);
        if (status == DRATS_OK) {
            ++tx_frame_sequence;
        }
    }
    return status;
}

static void command_received(const drats_frame_header_t *header,
                             const uint8_t *payload,
                             void *context) {
    (void)context;
    if ((header->type != DRATS_MSG_CONFIG_SET) ||
        (header->payload_length != DRATS_CONFIG_COMMAND_SIZE)) {
        return;
    }
    drats_config_command_t request = {0};
    if (drats_config_command_decode(payload, header->payload_length,
                                    &request) == DRATS_OK) {
#if defined(DRATS_REAL_SENSOR)
        if (request.config.sample_rate_hz != 1000U) {
            goto reject;
        }
#endif
        if (xQueueSend(config_queue, &request, 0U) == pdPASS) {
            return;
        }
    }
#if defined(DRATS_REAL_SENSOR)
reject:
    ;
#endif
    const drats_config_ack_t rejection = {
        .request_id = request.request_id,
        .status = DRATS_E_ARGUMENT,
        .config = {0},
        .telemetry_enabled = false
    };
    (void)xQueueSend(ack_queue, &rejection, 0U);
}

static void heartbeat(drats_task_id_t task) {
    (void)drats_supervisor_heartbeat(&supervisor, task,
                                     (uint64_t)xTaskGetTickCount());
}

static void acquisition_task(void *argument) {
    (void)argument;
#if defined(DRATS_REAL_SENSOR)
    if (drats_sample_timer_start(xTaskGetCurrentTaskHandle()) != DRATS_OK) {
        drats_assert_failed(__FILE__, __LINE__);
    }
#else
    TickType_t wake_time = xTaskGetTickCount();
#endif
#if !defined(DRATS_REAL_SENSOR)
    uint32_t synthetic_value = 0U;
#endif
    for (;;) {
        heartbeat(DRATS_TASK_ACQUISITION);
        drats_config_command_t request;
        if (xQueueReceive(config_queue, &request, 0U) == pdPASS) {
#if defined(DRATS_REAL_SENSOR)
            drats_sample_timer_stop();
#endif
            drats_status_t status = drats_acq_stop(&acquisition);
            if (status == DRATS_OK) {
                status = drats_acq_reconfigure(&acquisition, &request.config);
            }
            if (status == DRATS_OK) {
                status = drats_acq_start(&acquisition);
            }
#if defined(DRATS_REAL_SENSOR)
            (void)ulTaskNotifyTakeIndexed(0U, pdTRUE, 0U);
            if ((status == DRATS_OK) &&
                (drats_sample_timer_start(xTaskGetCurrentTaskHandle()) !=
                 DRATS_OK)) {
                status = DRATS_E_IO;
            }
#endif
            if (status == DRATS_OK) {
                atomic_store_explicit(&telemetry_enabled,
                                      request.telemetry_enabled,
                                      memory_order_release);
            }
            const drats_config_ack_t ack = {
                .request_id = request.request_id,
                .status = status,
                .config = acquisition.config,
                .telemetry_enabled = atomic_load_explicit(
                    &telemetry_enabled, memory_order_acquire)
            };
            (void)xQueueSend(ack_queue, &ack, 0U);
        }
        drats_sample_block_t *block = NULL;
        if (drats_acq_arm_next(&acquisition, &block) == DRATS_OK) {
            const uint16_t sample_count = acquisition.config.samples_per_block;
            uint64_t timestamp_ns = 0U;
#if !defined(DRATS_REAL_SENSOR)
            timestamp_ns = drats_tim2_now_us() * UINT64_C(1000);
#endif
            drats_status_t sample_status = DRATS_OK;
            for (size_t sample = 0; sample < sample_count; ++sample) {
#if defined(DRATS_REAL_SENSOR)
                if (ulTaskNotifyTakeIndexed(0U, pdTRUE,
                                            pdMS_TO_TICKS(5U)) == 0U) {
                    sample_status = DRATS_E_TIMEOUT;
                } else {
                    if (sample == 0U) {
                        timestamp_ns = drats_tim2_now_us() * UINT64_C(1000);
                    }
                    sample_status = drats_icm42688_read_axes(
                        &sensor, block->samples[sample]);
                }
                if (sample_status != DRATS_OK) {
                    break;
                }
#else
                for (size_t channel = 0; channel < DRATS_CHANNEL_COUNT; ++channel) {
                    block->samples[sample][channel] =
                        (int16_t)(synthetic_value & UINT32_C(0x7fff));
                    ++synthetic_value;
                }
#endif
            }
            if (sample_status == DRATS_OK) {
                (void)drats_acq_complete_dma(&acquisition, timestamp_ns,
                                             sample_count);
            } else {
                (void)drats_acq_fail_dma(&acquisition,
                                         sample_status == DRATS_E_TIMEOUT);
#if defined(DRATS_REAL_SENSOR)
                (void)drats_acq_recovery_result(
                    &acquisition, drats_icm42688_probe(&sensor) == DRATS_OK);
#endif
            }
        }
#if !defined(DRATS_REAL_SENSOR)
        const uint32_t delay_ms =
            ((uint32_t)acquisition.config.samples_per_block * 1000U +
             acquisition.config.sample_rate_hz - 1U) /
            acquisition.config.sample_rate_hz;
        vTaskDelayUntil(&wake_time, pdMS_TO_TICKS(delay_ms == 0U ? 1U : delay_ms));
#endif
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
    bool has_ack = false;
    drats_config_ack_t pending_ack = {0};
    for (;;) {
        heartbeat(DRATS_TASK_TELEMETRY);
        if (!has_ack &&
            (xQueueReceive(ack_queue, &pending_ack, 0U) == pdPASS)) {
            has_ack = true;
        }
        if (has_ack) {
            if (transmit_ack(&pending_ack) == DRATS_OK) {
                has_ack = false;
            }
            vTaskDelay(pdMS_TO_TICKS(1U));
            continue;
        }
        if (!has_pending &&
            (drats_handle_queue_pop(&telemetry_queue, &pending) == DRATS_OK)) {
            has_pending = true;
        }
        if (has_pending) {
            drats_sample_block_t *block = NULL;
            if (drats_block_get(&block_pool, pending, DRATS_BLOCK_TELEMETRY,
                                &block) != DRATS_OK) {
                has_pending = false;
            } else if (!atomic_load_explicit(&telemetry_enabled,
                                             memory_order_acquire) ||
                       (transmit_block(block) == DRATS_OK)) {
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
        const bool watchdog_eligible = drats_supervisor_watchdog_eligible(
            &supervisor, (uint64_t)xTaskGetTickCount());
#if defined(DRATS_ENABLE_WATCHDOG)
        if (watchdog_eligible) {
            drats_watchdog_kick();
        }
#else
        (void)watchdog_eligible;
#endif
        vTaskDelay(pdMS_TO_TICKS(10U));
    }
}

static void diagnostics_task(void *argument) {
    (void)argument;
    uint8_t received[64];
    for (;;) {
        heartbeat(DRATS_TASK_DIAGNOSTICS);
        const size_t length = drats_uart3_read(received, sizeof(received));
        drats_stream_decoder_feed(&command_decoder, received, length,
                                  command_received, NULL);
        vTaskDelay(pdMS_TO_TICKS(5U));
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
    startup_reset_flags = drats_reset_reason_capture();
    drats_block_pool_init(&block_pool);
    drats_handle_queue_init(&ready_queue);
    drats_handle_queue_init(&telemetry_queue);
    drats_stream_decoder_init(&command_decoder);
    atomic_init(&telemetry_enabled, true);
    config_queue = xQueueCreateStatic(2U, sizeof(drats_config_command_t),
                                      config_queue_storage,
                                      &config_queue_object);
    ack_queue = xQueueCreateStatic(4U, sizeof(drats_config_ack_t),
                                   ack_queue_storage, &ack_queue_object);
    if ((config_queue == NULL) || (ack_queue == NULL)) {
        drats_assert_failed(__FILE__, __LINE__);
    }
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
    if (drats_tim2_init(SystemCoreClock) != DRATS_OK) {
        drats_assert_failed(__FILE__, __LINE__);
    }
#if defined(DRATS_ENABLE_WATCHDOG)
    if (drats_watchdog_start(2000U) != DRATS_OK) {
        drats_assert_failed(__FILE__, __LINE__);
    }
#endif
#if defined(DRATS_REAL_SENSOR)
    if (drats_sample_timer_init(SystemCoreClock, 1000U) != DRATS_OK) {
        drats_assert_failed(__FILE__, __LINE__);
    }
    const drats_icm42688_bus_t sensor_bus = {
        .transfer = drats_spi1_transfer_dma,
        .delay_us = drats_spi1_delay_us,
        .context = NULL,
        .timeout_us = 1000U
    };
    if ((drats_spi1_init(SystemCoreClock, 16000000U) != DRATS_OK) ||
        (drats_icm42688_init(&sensor, &sensor_bus) != DRATS_OK)) {
        drats_assert_failed(__FILE__, __LINE__);
    }
#endif

    (void)xTaskCreateStatic(
        acquisition_task, "acquisition", TASK_STACK_WORDS, NULL,
        PRIORITY_ACQUISITION, acquisition_stack, &acquisition_tcb);
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
