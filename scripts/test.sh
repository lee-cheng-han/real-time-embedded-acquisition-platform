#!/usr/bin/env bash
set -euo pipefail

project_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="${project_root}/build/host"

generator_args=()
if [[ ! -f "${build_dir}/CMakeCache.txt" ]] && command -v ninja >/dev/null 2>&1; then
  generator_args=(-G Ninja)
fi

cmake -S "${project_root}" -B "${build_dir}" "${generator_args[@]}" \
  -DCMAKE_BUILD_TYPE=Debug -DDRATS_ENABLE_SANITIZERS=ON
cmake --build "${build_dir}"
ctest --test-dir "${build_dir}" --output-on-failure \
  --output-junit "${build_dir}/test-results.xml"
