#include "drats/icm42688.h"

#include <stdbool.h>
#include <string.h>

enum {
    REG_DEVICE_CONFIG = 0x11,
    REG_ACCEL_DATA_X1 = 0x1f,
    REG_PWR_MGMT0 = 0x4e,
    REG_GYRO_CONFIG0 = 0x4f,
    REG_ACCEL_CONFIG0 = 0x50,
    REG_WHO_AM_I = 0x75,
    REG_BANK_SEL = 0x76,
    SPI_READ = 0x80,
    SOFT_RESET_CONFIG = 0x01,
    SENSOR_MODES_LOW_NOISE = 0x0f,
    ODR_1KHZ = 0x06
};

static drats_status_t transfer(drats_icm42688_t *sensor,
                               const uint8_t *tx,
                               uint8_t *rx,
                               size_t length) {
    drats_status_t status = sensor->bus.transfer(
        sensor->bus.context, tx, rx, length, sensor->bus.timeout_us);
    ++sensor->transfers;
    if (status != DRATS_OK) {
        ++sensor->errors;
    }
    return status;
}

static drats_status_t write_register(drats_icm42688_t *sensor,
                                     uint8_t address,
                                     uint8_t value) {
    const uint8_t tx[2] = {address, value};
    uint8_t rx[2] = {0};
    return transfer(sensor, tx, rx, sizeof(tx));
}

static drats_status_t read_registers(drats_icm42688_t *sensor,
                                     uint8_t address,
                                     uint8_t *output,
                                     size_t output_length) {
    if ((output == NULL) || (output_length == 0U) || (output_length > 16U)) {
        return DRATS_E_ARGUMENT;
    }
    uint8_t tx[17] = {0};
    uint8_t rx[17] = {0};
    tx[0] = address | SPI_READ;
    drats_status_t status = transfer(sensor, tx, rx, output_length + 1U);
    if (status == DRATS_OK) {
        memcpy(output, &rx[1], output_length);
    }
    return status;
}

drats_status_t drats_icm42688_probe(drats_icm42688_t *sensor) {
    if ((sensor == NULL) || (sensor->bus.transfer == NULL)) {
        return DRATS_E_ARGUMENT;
    }
    uint8_t identity = 0U;
    drats_status_t status = read_registers(sensor, REG_WHO_AM_I,
                                            &identity, 1U);
    sensor->who_am_i = identity;
    if (status != DRATS_OK) {
        return status;
    }
    return identity == DRATS_ICM42688_WHO_AM_I_VALUE ? DRATS_OK : DRATS_E_IO;
}

drats_status_t drats_icm42688_init(drats_icm42688_t *sensor,
                                   const drats_icm42688_bus_t *bus) {
    if ((sensor == NULL) || (bus == NULL) || (bus->transfer == NULL) ||
        (bus->delay_us == NULL) || (bus->timeout_us == 0U)) {
        return DRATS_E_ARGUMENT;
    }
    memset(sensor, 0, sizeof(*sensor));
    sensor->bus = *bus;
    drats_status_t status = write_register(sensor, REG_BANK_SEL, 0U);
    if (status == DRATS_OK) {
        status = drats_icm42688_probe(sensor);
    }
    if (status == DRATS_OK) {
        status = write_register(sensor, REG_DEVICE_CONFIG, SOFT_RESET_CONFIG);
    }
    if (status != DRATS_OK) {
        return status;
    }
    sensor->bus.delay_us(sensor->bus.context, 2000U);
    status = drats_icm42688_probe(sensor);
    if (status == DRATS_OK) {
        status = write_register(sensor, REG_GYRO_CONFIG0, ODR_1KHZ);
    }
    if (status == DRATS_OK) {
        status = write_register(sensor, REG_ACCEL_CONFIG0, ODR_1KHZ);
    }
    if (status == DRATS_OK) {
        status = write_register(sensor, REG_PWR_MGMT0, SENSOR_MODES_LOW_NOISE);
    }
    if (status != DRATS_OK) {
        return status;
    }
    sensor->bus.delay_us(sensor->bus.context, 50000U);
    sensor->initialized = true;
    return DRATS_OK;
}

drats_status_t drats_icm42688_read_axes(drats_icm42688_t *sensor,
                                        int16_t axes[6]) {
    if ((sensor == NULL) || (axes == NULL) || !sensor->initialized) {
        return DRATS_E_STATE;
    }
    uint8_t raw[12];
    drats_status_t status = read_registers(sensor, REG_ACCEL_DATA_X1,
                                            raw, sizeof(raw));
    if (status != DRATS_OK) {
        return status;
    }
    for (size_t i = 0; i < 6U; ++i) {
        const uint16_t value = (uint16_t)(
            ((uint16_t)raw[2U * i] << 8U) |
            (uint16_t)raw[(2U * i) + 1U]);
        axes[i] = (int16_t)value;
    }
    return DRATS_OK;
}

void drats_icm42688_shutdown(drats_icm42688_t *sensor) {
    if (sensor != NULL) {
        if (sensor->initialized) {
            (void)write_register(sensor, REG_PWR_MGMT0, 0U);
        }
        sensor->initialized = false;
    }
}
