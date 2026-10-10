#include "watchdog.h"

#include "stm32h563xx.h"

enum {
    LSI_NOMINAL_HZ = 32000,
    IWDG_DIVIDER = 32,
    IWDG_PRESCALER_ENCODING = 3,
    READY_SPIN_LIMIT = 1000000
};

drats_status_t drats_watchdog_start(uint32_t timeout_ms) {
    const uint64_t reload =
        ((uint64_t)timeout_ms * LSI_NOMINAL_HZ) /
        (UINT64_C(1000) * IWDG_DIVIDER);
    if ((timeout_ms == 0U) || (reload == 0U) || (reload > 4096U)) {
        return DRATS_E_ARGUMENT;
    }
    RCC->BDCR |= RCC_BDCR_LSION;
    uint32_t spins = 0U;
    while ((RCC->BDCR & RCC_BDCR_LSIRDY) == 0U) {
        if (++spins == READY_SPIN_LIMIT) {
            return DRATS_E_TIMEOUT;
        }
    }
    IWDG->KR = UINT32_C(0x5555);
    IWDG->PR = IWDG_PRESCALER_ENCODING;
    IWDG->RLR = (uint32_t)reload - 1U;
    spins = 0U;
    while ((IWDG->SR & (IWDG_SR_PVU | IWDG_SR_RVU)) != 0U) {
        if (++spins == READY_SPIN_LIMIT) {
            return DRATS_E_TIMEOUT;
        }
    }
    IWDG->KR = UINT32_C(0xcccc);
    IWDG->KR = UINT32_C(0xaaaa);
    return DRATS_OK;
}

void drats_watchdog_kick(void) {
    IWDG->KR = UINT32_C(0xaaaa);
}

uint32_t drats_reset_reason_capture(void) {
    const uint32_t flags = RCC->RSR;
    RCC->RSR |= RCC_RSR_RMVF;
    return flags;
}

bool drats_reset_was_watchdog(uint32_t reset_flags) {
    return (reset_flags & RCC_RSR_IWDGRSTF) != 0U;
}
