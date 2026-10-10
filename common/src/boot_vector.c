#include "drats/boot_vector.h"

drats_status_t drats_boot_vector_validate(uint32_t initial_stack,
                                          uint32_t reset_vector,
                                          uint32_t flash_origin,
                                          uint32_t flash_size,
                                          uint32_t ram_origin,
                                          uint32_t ram_size) {
    if ((flash_size == 0U) || (ram_size == 0U) ||
        (flash_origin > UINT32_MAX - flash_size) ||
        (ram_origin > UINT32_MAX - ram_size)) {
        return DRATS_E_ARGUMENT;
    }
    const uint32_t reset_address = reset_vector & ~UINT32_C(1);
    if ((initial_stack <= ram_origin) ||
        (initial_stack > ram_origin + ram_size) ||
        ((initial_stack & UINT32_C(7)) != 0U) ||
        ((reset_vector & UINT32_C(1)) == 0U) ||
        (reset_address < flash_origin) ||
        (reset_address >= flash_origin + flash_size)) {
        return DRATS_E_FORMAT;
    }
    return DRATS_OK;
}
