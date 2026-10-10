#include "stm32h563xx.h"
#include "drats/boot_vector.h"

#include <stddef.h>
#include <stdint.h>

enum {
    APPLICATION_ORIGIN = 0x08020000U,
    APPLICATION_LIMIT = 0x080e0000U,
    SRAM_ORIGIN = 0x20000000U,
    SRAM_LIMIT = 0x200a0000U
};

__attribute__((noreturn)) static void halt(void) {
    __disable_irq();
    for (;;) {
        __WFI();
    }
}

__attribute__((naked, noreturn)) static void handoff(
    uint32_t stack __attribute__((unused)),
    uint32_t reset __attribute__((unused))) {
    __asm volatile (
        "movs r2, #0\n"
        "msr control, r2\n"
        "msr msp, r0\n"
        "dsb\n"
        "isb\n"
        "bx r1\n");
}

__attribute__((noreturn)) static void launch_application(
    const uint32_t vectors[2]) {
    __disable_irq();
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;
    for (size_t bank = 0U; bank < 8U; ++bank) {
        NVIC->ICER[bank] = UINT32_MAX;
        NVIC->ICPR[bank] = UINT32_MAX;
    }
    SCB->VTOR = APPLICATION_ORIGIN;
    __DSB();
    __ISB();
    handoff(vectors[0], vectors[1]);
    halt();
}

int main(void) {
    const uint32_t *const vectors =
        (const uint32_t *)(uintptr_t)APPLICATION_ORIGIN;
    if (drats_boot_vector_validate(vectors[0], vectors[1],
                                   APPLICATION_ORIGIN,
                                   APPLICATION_LIMIT - APPLICATION_ORIGIN,
                                   SRAM_ORIGIN,
                                   SRAM_LIMIT - SRAM_ORIGIN) != DRATS_OK) {
        halt();
    }
    launch_application(vectors);
}
