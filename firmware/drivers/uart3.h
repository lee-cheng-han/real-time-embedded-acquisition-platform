#ifndef DRATS_UART3_H
#define DRATS_UART3_H

#include "drats/status.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t bytes_accepted;
    uint32_t bytes_transmitted;
    uint32_t submissions_rejected;
    uint32_t hardware_errors;
    uint32_t bytes_received;
    uint32_t receive_overflows;
} drats_uart3_counters_t;

drats_status_t drats_uart3_init(uint32_t peripheral_clock_hz,
                                uint32_t baud_rate);
void drats_uart3_shutdown(void);
drats_status_t drats_uart3_write(const uint8_t *data, size_t length);
size_t drats_uart3_available(void);
size_t drats_uart3_read(uint8_t *output, size_t capacity);
void drats_uart3_irq(void);
const drats_uart3_counters_t *drats_uart3_counters(void);

#endif
