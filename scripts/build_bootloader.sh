#!/usr/bin/env bash
set -euo pipefail

project_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="${project_root}/build/bootloader"

command -v arm-none-eabi-gcc >/dev/null
"${project_root}/scripts/fetch_deps.sh"

generator_args=()
if [[ ! -f "${build_dir}/CMakeCache.txt" ]] && command -v ninja >/dev/null; then
  generator_args=(-G Ninja)
fi
cmake -S "${project_root}/bootloader" -B "${build_dir}" \
  "${generator_args[@]}" \
  -DCMAKE_TOOLCHAIN_FILE="${project_root}/firmware/cmake/arm-none-eabi-gcc.cmake" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "${build_dir}"
python3 "${project_root}/scripts/check_target.py" \
  "${build_dir}/drats_development_bootloader" \
  "${build_dir}/drats_development_bootloader.bin" \
  --flash-origin 0x08000000 --flash-size 0x20000 \
  --output "${build_dir}/target-report.json"
