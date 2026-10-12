#!/usr/bin/env bash
set -euo pipefail

project_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
"${project_root}/scripts/test.sh"
"${project_root}/scripts/build_target.sh"
DRATS_BUILD_DIR="${project_root}/build/target-sensor" DRATS_REAL_SENSOR=ON \
  DRATS_ENABLE_WATCHDOG=ON DRATS_ENABLE_TRACE=ON \
  "${project_root}/scripts/build_target.sh"
"${project_root}/scripts/build_bootloader.sh"
python3 "${project_root}/scripts/make_factory_image.py" \
  "${project_root}/build/bootloader/drats_development_bootloader.bin" \
  "${project_root}/build/target/drats_firmware.bin" \
  "${project_root}/build/factory/drats_factory.bin" \
  --report "${project_root}/build/factory/factory-report.json" \
  --package "${project_root}/build/factory/drats-1.0.0.drat" \
  --private-key "${project_root}/build/keys/development-private.pem" \
  --public-key "${project_root}/build/keys/development-public.pem"
"${project_root}/scripts/test_boot_verifier.sh" \
  "${project_root}/build/factory/drats-1.0.0.drat"
python3 "${project_root}/scripts/make_factory_image.py" \
  "${project_root}/build/bootloader/drats_development_bootloader.bin" \
  "${project_root}/build/target-sensor/drats_firmware.bin" \
  "${project_root}/build/factory/drats_sensor_factory.bin" \
  --report "${project_root}/build/factory/sensor-factory-report.json" \
  --package "${project_root}/build/factory/drats-sensor-1.0.0.drat" \
  --private-key "${project_root}/build/keys/development-private.pem" \
  --public-key "${project_root}/build/keys/development-public.pem"
"${project_root}/scripts/test_boot_verifier.sh" \
  "${project_root}/build/factory/drats-sensor-1.0.0.drat" \
  "${project_root}/build/host/sensor-boot-verifier-report.json"
