#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

extern uint32_t SystemCoreClock;
void drats_assert_failed(const char *file, int line);

#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                   0
#define configCPU_CLOCK_HZ                       SystemCoreClock
#define configTICK_RATE_HZ                       ((TickType_t)1000U)
#define configMAX_PRIORITIES                     7U
#define configMINIMAL_STACK_SIZE                 128U
#define configMAX_TASK_NAME_LEN                  16U
#define configUSE_16_BIT_TICKS                   0
#define configIDLE_SHOULD_YIELD                   0
#define configUSE_IDLE_HOOK                       0
#define configUSE_TICK_HOOK                       0
#define configUSE_TASK_NOTIFICATIONS             1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES    2U
#define configUSE_MUTEXES                        1
#define configUSE_RECURSIVE_MUTEXES              0
#define configUSE_COUNTING_SEMAPHORES            0
#define configQUEUE_REGISTRY_SIZE                4U
#define configUSE_TIMERS                         0
#define configSUPPORT_STATIC_ALLOCATION          1
#define configSUPPORT_DYNAMIC_ALLOCATION         0
#define configCHECK_FOR_STACK_OVERFLOW           2
#define configUSE_MALLOC_FAILED_HOOK             0
#define configUSE_TRACE_FACILITY                  1
#define configGENERATE_RUN_TIME_STATS            0
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  1
#define configENABLE_FPU                         0
#define configENABLE_MPU                         0
#define configENABLE_TRUSTZONE                   0
#define configRUN_FREERTOS_SECURE_ONLY           1

#define configPRIO_BITS                          4U
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY  15U
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5U
#define configKERNEL_INTERRUPT_PRIORITY          (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY     (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))

#define configASSERT(expression) do {                                      \
    if ((expression) == 0) {                                               \
        drats_assert_failed(__FILE__, __LINE__);                            \
    }                                                                      \
} while (0)

#define INCLUDE_vTaskDelay                     1
#define INCLUDE_xTaskDelayUntil                1
#define INCLUDE_vTaskSuspend                   0
#define INCLUDE_xTaskGetSchedulerState         1
#define INCLUDE_uxTaskGetStackHighWaterMark    1

#endif
