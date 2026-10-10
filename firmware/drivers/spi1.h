#ifndef DRATS_SPI1_H
#define DRATS_SPI1_H

#include "drats/status.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t transfers;
    uint32_t bytes;
    uint32_t timeouts;
    uint32_t hardware_errors;
} drats_spi1_counters_t;

drats_status_t drats_spi1_init(uint32_t peripheral_clock_hz,
                               uint32_t maximum_bus_hz);
void drats_spi1_shutdown(void);
drats_status_t drats_spi1_transfer(void *context,
                                   const uint8_t *tx,
                                   uint8_t *rx,
                                   size_t length,
                                   uint32_t timeout_us);
drats_status_t drats_spi1_transfer_dma(void *context,
                                       const uint8_t *tx,
                                       uint8_t *rx,
                                       size_t length,
                                       uint32_t timeout_us);
void drats_spi1_dma_irq(void);
void drats_spi1_delay_us(void *context, uint32_t delay_us);
const drats_spi1_counters_t *drats_spi1_counters(void);

#endif
