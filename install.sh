#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
INSTALL_DIR="${HOME}/.local/bin"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}"

mkdir -p "${INSTALL_DIR}"
install -m 0755 "${BUILD_DIR}/fpc" "${INSTALL_DIR}/fpc"

printf 'Installed fpc to %s/fpc\n' "${INSTALL_DIR}"
case ":${PATH}:" in
  *":${INSTALL_DIR}:"*) ;;
  *) printf 'Note: %s is not currently in PATH. Add it to your shell profile.\n' "${INSTALL_DIR}" ;;
esac
