cmake_minimum_required(VERSION 3.20)
project(drats_firmware LANGUAGES C ASM)

option(DRATS_REAL_SENSOR "Acquire from an ICM-42688-P on SPI1" OFF)
option(DRATS_ENABLE_WATCHDOG "Enable the independent hardware watchdog" OFF)

get_filename_component(DRATS_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(DRATS_DEPS "${DRATS_ROOT}/build/deps" CACHE PATH "Downloaded dependencies")
set(CMSIS_CORE "${DRATS_DEPS}/STM32CubeH5/Drivers/CMSIS/Include")
set(CMSIS_DEVICE "${DRATS_DEPS}/cmsis_device_h5")
set(FREERTOS "${DRATS_DEPS}/FreeRTOS-Kernel")

foreach(required_path IN ITEMS "${CMSIS_CORE}" "${CMSIS_DEVICE}" "${FREERTOS}")
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR "Missing dependency ${required_path}; run scripts/fetch_deps.sh")
    endif()
endforeach()

set(CPU_FLAGS -mcpu=cortex-m33 -mthumb -mfloat-abi=soft)
set(COMMON_WARNINGS
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror
    -Wno-error=unused-parameter
)

add_executable(drats_firmware
    app/main.c
    app/interrupts.c
    drivers/spi1.c
    drivers/sample_timer.c
    drivers/tim2.c
    drivers/uart3.c
    drivers/watchdog.c
    startup/fault_handlers.c
    startup/syscalls.c
    "${CMSIS_DEVICE}/Source/Templates/gcc/startup_stm32h563xx.s"
    "${CMSIS_DEVICE}/Source/Templates/system_stm32h5xx.c"
    "${FREERTOS}/tasks.c"
    "${FREERTOS}/queue.c"
    "${FREERTOS}/list.c"
    "${FREERTOS}/portable/GCC/ARM_CM33_NTZ/non_secure/port.c"
    "${FREERTOS}/portable/GCC/ARM_CM33_NTZ/non_secure/portasm.c"
    "${DRATS_ROOT}/common/src/acquisition.c"
    "${DRATS_ROOT}/common/src/block_pool.c"
    "${DRATS_ROOT}/common/src/config_store.c"
    "${DRATS_ROOT}/common/src/config_protocol.c"
    "${DRATS_ROOT}/common/src/crc32c.c"
    "${DRATS_ROOT}/common/src/fault_manager.c"
    "${DRATS_ROOT}/common/src/handle_queue.c"
    "${DRATS_ROOT}/common/src/icm42688.c"
    "${DRATS_ROOT}/common/src/image.c"
    "${DRATS_ROOT}/common/src/protocol.c"
    "${DRATS_ROOT}/common/src/supervisor.c"
    "${DRATS_ROOT}/common/src/stream_decoder.c"
    "${DRATS_ROOT}/common/src/update_manager.c"
)

target_include_directories(drats_firmware PRIVATE
    app
    drivers
    rtos
    "${DRATS_ROOT}/common/include"
    "${CMSIS_CORE}"
    "${CMSIS_DEVICE}/Include"
    "${FREERTOS}/include"
    "${FREERTOS}/portable/GCC/ARM_CM33_NTZ/non_secure"
)
target_compile_definitions(drats_firmware PRIVATE
    STM32H563xx
    VECT_TAB_OFFSET=0x20000U
    $<$<BOOL:${DRATS_REAL_SENSOR}>:DRATS_REAL_SENSOR=1>
    $<$<BOOL:${DRATS_ENABLE_WATCHDOG}>:DRATS_ENABLE_WATCHDOG=1>
)
target_compile_options(drats_firmware PRIVATE
    ${CPU_FLAGS}
    -ffunction-sections -fdata-sections
    $<$<COMPILE_LANGUAGE:C>:${COMMON_WARNINGS}>
)
target_link_options(drats_firmware PRIVATE
    ${CPU_FLAGS}
    -T${CMAKE_CURRENT_LIST_DIR}/startup/stm32h563_app.ld
    -Wl,--gc-sections
    -Wl,--cref
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/drats_firmware.map
    -Wl,--print-memory-usage
    --specs=nano.specs
)

add_custom_command(TARGET drats_firmware POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:drats_firmware>
            ${CMAKE_CURRENT_BINARY_DIR}/drats_firmware.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:drats_firmware>
            ${CMAKE_CURRENT_BINARY_DIR}/drats_firmware.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:drats_firmware>
    VERBATIM
)
