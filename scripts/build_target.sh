#!/usr/bin/env bash
set -euo pipefail

project_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="${DRATS_BUILD_DIR:-${project_root}/build/target}"
real_sensor="${DRATS_REAL_SENSOR:-OFF}"
watchdog="${DRATS_ENABLE_WATCHDOG:-OFF}"
trace="${DRATS_ENABLE_TRACE:-OFF}"

command -v arm-none-eabi-gcc >/dev/null
"${project_root}/scripts/fetch_deps.sh"

generator_args=()
if [[ ! -f "${build_dir}/CMakeCache.txt" ]] && command -v ninja >/dev/null; then
  generator_args=(-G Ninja)
fi
cmake -S "${project_root}/firmware" -B "${build_dir}" \
  "${generator_args[@]}" \
  -DCMAKE_TOOLCHAIN_FILE="${project_root}/firmware/cmake/arm-none-eabi-gcc.cmake" \
  -DDRATS_REAL_SENSOR="${real_sensor}" \
  -DDRATS_ENABLE_WATCHDOG="${watchdog}" \
  -DDRATS_ENABLE_TRACE="${trace}" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "${build_dir}"
python3 "${project_root}/scripts/check_size.py" \
  "${build_dir}/drats_firmware" \
  --output "${build_dir}/size-report.json"
python3 "${project_root}/scripts/check_target.py" \
  "${build_dir}/drats_firmware" \
  "${build_dir}/drats_firmware.bin" \
  --output "${build_dir}/target-report.json"
