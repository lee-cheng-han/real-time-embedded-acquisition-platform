#!/usr/bin/env bash
set -euo pipefail

project_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
deps_dir="${project_root}/build/deps"
cube_commit=e5c73ea1bbe384cdadb0cf161ae97a0157990766
device_commit=884b8dc78e41cbfca008363342b17f4a9e8641f7
freertos_commit=8be86d4a24fd4091f8f4192018423ab590f408db
mbedtls_commit=068ff080b369adfac81509f9b57b2afabaf82dc5

mkdir -p "${deps_dir}"

if [[ ! -d "${deps_dir}/STM32CubeH5/.git" ]]; then
  git clone --filter=blob:none --no-checkout \
    https://github.com/STMicroelectronics/STM32CubeH5.git \
    "${deps_dir}/STM32CubeH5"
fi
git -C "${deps_dir}/STM32CubeH5" fetch --depth 1 origin "${cube_commit}"
git -C "${deps_dir}/STM32CubeH5" sparse-checkout init --cone
git -C "${deps_dir}/STM32CubeH5" sparse-checkout set Drivers/CMSIS/Include
git -C "${deps_dir}/STM32CubeH5" checkout --detach "${cube_commit}"

if [[ ! -d "${deps_dir}/cmsis_device_h5/.git" ]]; then
  git clone https://github.com/STMicroelectronics/cmsis_device_h5.git \
    "${deps_dir}/cmsis_device_h5"
fi
git -C "${deps_dir}/cmsis_device_h5" fetch --depth 1 origin "${device_commit}"
git -C "${deps_dir}/cmsis_device_h5" checkout --detach "${device_commit}"

if [[ ! -d "${deps_dir}/FreeRTOS-Kernel/.git" ]]; then
  git clone https://github.com/FreeRTOS/FreeRTOS-Kernel.git \
    "${deps_dir}/FreeRTOS-Kernel"
fi
git -C "${deps_dir}/FreeRTOS-Kernel" fetch --depth 1 origin "${freertos_commit}"
git -C "${deps_dir}/FreeRTOS-Kernel" checkout --detach "${freertos_commit}"

if [[ ! -d "${deps_dir}/mbedtls/.git" ]]; then
  git clone https://github.com/Mbed-TLS/mbedtls.git "${deps_dir}/mbedtls"
fi
git -C "${deps_dir}/mbedtls" fetch --depth 1 origin "${mbedtls_commit}"
git -C "${deps_dir}/mbedtls" checkout --detach "${mbedtls_commit}"

printf '%s\n' \
  "STM32CubeH5 ${cube_commit}" \
  "cmsis_device_h5 ${device_commit}" \
  "FreeRTOS-Kernel ${freertos_commit}" \
  "Mbed-TLS ${mbedtls_commit}"
