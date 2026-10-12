cmake_minimum_required(VERSION 3.20)
project(drats_development_bootloader LANGUAGES C ASM)

get_filename_component(DRATS_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(DRATS_DEPS "${DRATS_ROOT}/build/deps" CACHE PATH "Downloaded dependencies")
set(CMSIS_CORE "${DRATS_DEPS}/STM32CubeH5/Drivers/CMSIS/Include")
set(CMSIS_DEVICE "${DRATS_DEPS}/cmsis_device_h5")
set(MBEDTLS "${DRATS_DEPS}/mbedtls")
set(DRATS_GENERATED "${DRATS_ROOT}/build/generated")

foreach(required_path IN ITEMS "${CMSIS_CORE}" "${CMSIS_DEVICE}" "${MBEDTLS}"
                               "${DRATS_GENERATED}/development_public_key.h")
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR "Missing dependency ${required_path}; run scripts/fetch_deps.sh")
    endif()
endforeach()

set(CPU_FLAGS -mcpu=cortex-m33 -mthumb -mfloat-abi=soft)

add_library(drats_mbedcrypto STATIC
    "${MBEDTLS}/library/asn1parse.c"
    "${MBEDTLS}/library/asn1write.c"
    "${MBEDTLS}/library/bignum.c"
    "${MBEDTLS}/library/bignum_core.c"
    "${MBEDTLS}/library/constant_time.c"
    "${MBEDTLS}/library/ecp.c"
    "${MBEDTLS}/library/ecp_curves.c"
    "${MBEDTLS}/library/ecdsa.c"
    "${MBEDTLS}/library/platform_util.c"
    "${MBEDTLS}/library/sha256.c"
)
target_include_directories(drats_mbedcrypto PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}"
    "${MBEDTLS}/include"
    "${MBEDTLS}/library"
)
target_compile_definitions(drats_mbedcrypto PRIVATE
    MBEDTLS_CONFIG_FILE="drats_mbedtls_config.h"
)
target_compile_options(drats_mbedcrypto PRIVATE
    ${CPU_FLAGS} -ffunction-sections -fdata-sections -w
)

add_executable(drats_development_bootloader
    main.c
    image_verify.c
    "${DRATS_ROOT}/common/src/boot_vector.c"
    "${DRATS_ROOT}/common/src/image.c"
    "${DRATS_ROOT}/firmware/startup/syscalls.c"
    "${CMSIS_DEVICE}/Source/Templates/gcc/startup_stm32h563xx.s"
    "${CMSIS_DEVICE}/Source/Templates/system_stm32h5xx.c"
)
target_include_directories(drats_development_bootloader PRIVATE
    "${DRATS_GENERATED}"
    "${DRATS_ROOT}/common/include"
    "${CMSIS_CORE}"
    "${CMSIS_DEVICE}/Include"
    "${MBEDTLS}/include"
)
target_compile_definitions(drats_development_bootloader PRIVATE STM32H563xx)
target_compile_options(drats_development_bootloader PRIVATE
    ${CPU_FLAGS}
    -ffunction-sections -fdata-sections
    $<$<COMPILE_LANGUAGE:C>:-std=c17 -Wall -Wextra -Wpedantic -Wconversion
      -Wsign-conversion -Wshadow -Werror>
)
target_link_libraries(drats_development_bootloader PRIVATE drats_mbedcrypto)
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
