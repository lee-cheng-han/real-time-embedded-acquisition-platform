#include "stm32h563xx.h"

#include <stdint.h>

typedef struct {
    uint32_t magic;
    uint32_t count;
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t mmfar;
    uint32_t bfar;
} drats_fault_snapshot_t;

__attribute__((section(".noinit")))
volatile drats_fault_snapshot_t drats_last_fault;

void HardFault_Handler(void) {
    __disable_irq();
    drats_last_fault.magic = UINT32_C(0x4641554C);
    ++drats_last_fault.count;
    drats_last_fault.cfsr = SCB->CFSR;
    drats_last_fault.hfsr = SCB->HFSR;
    drats_last_fault.mmfar = SCB->MMFAR;
    drats_last_fault.bfar = SCB->BFAR;
    __DSB();
    for (;;) {
        __NOP();
    }
}
