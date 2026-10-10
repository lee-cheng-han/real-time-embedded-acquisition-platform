cmake_minimum_required(VERSION 3.20)
project(drats_development_bootloader LANGUAGES C ASM)

get_filename_component(DRATS_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(DRATS_DEPS "${DRATS_ROOT}/build/deps" CACHE PATH "Downloaded dependencies")
set(CMSIS_CORE "${DRATS_DEPS}/STM32CubeH5/Drivers/CMSIS/Include")
set(CMSIS_DEVICE "${DRATS_DEPS}/cmsis_device_h5")

foreach(required_path IN ITEMS "${CMSIS_CORE}" "${CMSIS_DEVICE}")
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR "Missing dependency ${required_path}; run scripts/fetch_deps.sh")
    endif()
endforeach()

set(CPU_FLAGS -mcpu=cortex-m33 -mthumb -mfloat-abi=soft)

add_executable(drats_development_bootloader
    main.c
    "${DRATS_ROOT}/common/src/boot_vector.c"
    "${DRATS_ROOT}/firmware/startup/syscalls.c"
    "${CMSIS_DEVICE}/Source/Templates/gcc/startup_stm32h563xx.s"
    "${CMSIS_DEVICE}/Source/Templates/system_stm32h5xx.c"
)
target_include_directories(drats_development_bootloader PRIVATE
    "${DRATS_ROOT}/common/include"
    "${CMSIS_CORE}"
    "${CMSIS_DEVICE}/Include"
)
target_compile_definitions(drats_development_bootloader PRIVATE STM32H563xx)
target_compile_options(drats_development_bootloader PRIVATE
    ${CPU_FLAGS}
    -ffunction-sections -fdata-sections
    $<$<COMPILE_LANGUAGE:C>:-std=c17 -Wall -Wextra -Wpedantic -Wconversion
      -Wsign-conversion -Wshadow -Werror>
)
target_link_options(drats_development_bootloader PRIVATE
    ${CPU_FLAGS}
    -T${CMAKE_CURRENT_LIST_DIR}/stm32h563_boot.ld
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/drats_development_bootloader.map
    -Wl,--print-memory-usage
    --specs=nano.specs
)
add_custom_command(TARGET drats_development_bootloader POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:drats_development_bootloader>
            ${CMAKE_CURRENT_BINARY_DIR}/drats_development_bootloader.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:drats_development_bootloader>
            ${CMAKE_CURRENT_BINARY_DIR}/drats_development_bootloader.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:drats_development_bootloader>
    VERBATIM
)
