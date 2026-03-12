#!/usr/bin/env bash
set -euo pipefail

REPO="${REPO:-MuyleangIng/quickqc}"
INSTALL_DIR="${INSTALL_DIR:-$HOME/.local/bin}"

ARCH="$(uname -m)"
case "${ARCH}" in
  x86_64|amd64)
    ASSET="quickqc-linux-ubuntu-amd64.tar.gz"
    ;;
  aarch64|arm64)
    ASSET="quickqc-linux-ubuntu-arm64.tar.gz"
    ;;
  *)
    echo "Unsupported architecture: ${ARCH}" >&2
    exit 1
    ;;
esac

if command -v apt-get >/dev/null 2>&1; then
  sudo apt-get update
  sudo apt-get install -y \
    libqt6core6 \
    libqt6gui6 \
    libqt6sql6 \
    libqt6widgets6 \
    libqt6openglwidgets6
fi

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TMP_DIR}"' EXIT

curl -fsSL "https://github.com/${REPO}/releases/latest/download/${ASSET}" -o "${TMP_DIR}/quickqc.tar.gz"
tar -xzf "${TMP_DIR}/quickqc.tar.gz" -C "${TMP_DIR}"

if [[ ! -f "${TMP_DIR}/quickqc" ]]; then
  echo "quickqc binary not found in release artifact: ${ASSET}" >&2
  exit 1
fi

mkdir -p "${INSTALL_DIR}"
install -m 0755 "${TMP_DIR}/quickqc" "${INSTALL_DIR}/quickqc"

echo "QuickQC installed at ${INSTALL_DIR}/quickqc"
if [[ ":${PATH}:" != *":${INSTALL_DIR}:"* ]]; then
  echo "Add ${INSTALL_DIR} to PATH to run with: quickqc"
fi
