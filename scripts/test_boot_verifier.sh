#!/usr/bin/env bash
set -euo pipefail

project_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
package=${1:?usage: test_boot_verifier.sh PACKAGE}
report=${2:-"${project_root}/build/host/boot-verifier-report.json"}
mbedtls="${project_root}/build/deps/mbedtls"
output="${project_root}/build/host/test_boot_verifier"

cc -std=c17 -O2 -Wall -Wextra -Wpedantic -Werror \
  -DMBEDTLS_CONFIG_FILE='"drats_mbedtls_config.h"' \
  -I"${project_root}/build/generated" \
  -I"${project_root}/bootloader" \
  -I"${project_root}/common/include" \
  -I"${mbedtls}/include" \
  -I"${mbedtls}/library" \
  "${project_root}/tests/integration/test_boot_verifier.c" \
  "${project_root}/bootloader/image_verify.c" \
  "${project_root}/common/src/image.c" \
  "${mbedtls}/library/asn1parse.c" \
  "${mbedtls}/library/asn1write.c" \
  "${mbedtls}/library/bignum.c" \
  "${mbedtls}/library/bignum_core.c" \
  "${mbedtls}/library/constant_time.c" \
  "${mbedtls}/library/ecp.c" \
  "${mbedtls}/library/ecp_curves.c" \
  "${mbedtls}/library/ecdsa.c" \
  "${mbedtls}/library/platform_util.c" \
  "${mbedtls}/library/sha256.c" \
  -o "${output}"
"${output}" "${package}"
python3 - "${package}" "${report}" <<'PY'
import hashlib
import json
from pathlib import Path
import sys

package = Path(sys.argv[1])
report = Path(sys.argv[2])
result = {
    "checks": {
        "accepts_valid_package": True,
        "rejects_image_tampering": True,
        "rejects_signature_tampering": True,
        "rejects_rollback_counter": True,
    },
    "package": str(package),
    "package_sha256": hashlib.sha256(package.read_bytes()).hexdigest(),
    "pass": True,
}
report.parent.mkdir(parents=True, exist_ok=True)
report.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n",
                  encoding="utf-8")
PY
