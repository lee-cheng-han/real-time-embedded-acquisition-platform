#!/usr/bin/env bash
set -euo pipefail

project_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
"${project_root}/scripts/test.sh"
"${project_root}/scripts/build_target.sh"
