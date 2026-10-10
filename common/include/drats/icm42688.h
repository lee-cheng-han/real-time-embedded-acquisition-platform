#ifndef DRATS_ICM42688_H
#define DRATS_ICM42688_H

#include "drats/status.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DRATS_ICM42688_WHO_AM_I_VALUE 0x47U

typedef drats_status_t (*drats_spi_transfer_t)(void *context,
                                               const uint8_t *tx,
                                               uint8_t *rx,
                                               size_t length,
                                               uint32_t timeout_us);
typedef void (*drats_delay_us_t)(void *context, uint32_t delay_us);

typedef struct {
    drats_spi_transfer_t transfer;
    drats_delay_us_t delay_us;
    void *context;
    uint32_t timeout_us;
} drats_icm42688_bus_t;

typedef struct {
    drats_icm42688_bus_t bus;
    uint32_t transfers;
    uint32_t errors;
    uint8_t who_am_i;
    bool initialized;
} drats_icm42688_t;

drats_status_t drats_icm42688_init(drats_icm42688_t *sensor,
                                   const drats_icm42688_bus_t *bus);
drats_status_t drats_icm42688_read_axes(drats_icm42688_t *sensor,
                                        int16_t axes[6]);
drats_status_t drats_icm42688_probe(drats_icm42688_t *sensor);
void drats_icm42688_shutdown(drats_icm42688_t *sensor);

#endif
