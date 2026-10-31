#!/usr/bin/env bash
#
# Builds the PSP port of Tanks (EBOOT.PBP + assets/) inside WSL.
#
# Requirements:
#   - pspdev SDK installed at $PSPDEV (default: ~/pspdev). See
#     https://pspdev.github.io for the Windows/WSL install guide.
#   - cmake and make available.
#
# Run via build_psp.ps1 from Windows, or directly:
#   ./build_psp.sh
#
# Output: build-psp/ containing EBOOT.PBP and assets/. Copy the whole
# folder contents to ms0:/PSP/GAME/TANKS/ on the memory stick.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

export PSPDEV="${PSPDEV:-$HOME/pspdev}"
export PATH="$PATH:$PSPDEV/bin"

if [ ! -f "$PSPDEV/psp/share/pspdev.cmake" ]; then
    echo "error: pspdev SDK not found at $PSPDEV (pspdev.cmake missing)" >&2
    exit 1
fi

BUILD_DIR="$SCRIPT_DIR/build-psp"

cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$PSPDEV/psp/share/pspdev.cmake" \
    -DCMAKE_BUILD_TYPE=Release

cmake --build "$BUILD_DIR" --parallel "$(nproc)"

echo
echo "Build complete: $BUILD_DIR"
echo "  EBOOT.PBP + assets/ are ready."
echo "  Copy the contents of build-psp/ to ms0:/PSP/GAME/TANKS/ on your memory stick."
