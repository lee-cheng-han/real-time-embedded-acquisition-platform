#ifndef DRATS_BOOT_VECTOR_H
#define DRATS_BOOT_VECTOR_H

#include "drats/status.h"

#include <stdint.h>

drats_status_t drats_boot_vector_validate(uint32_t initial_stack,
                                          uint32_t reset_vector,
                                          uint32_t flash_origin,
                                          uint32_t flash_size,
                                          uint32_t ram_origin,
                                          uint32_t ram_size);

#endif
