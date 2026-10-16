#!/bin/bash
# Verified build script — always runs from the correct directory
set -e

BUILD_DIR=/home/bbrelin/nimcp/build
SITE_PACKAGES=/home/bbrelin/.local/lib/python3.12/site-packages

echo "=== 1. Delete stale installs ==="
rm -f "$SITE_PACKAGES"/nimcp*.so

echo "=== 2. Touch modified sources ==="
for f in "$@"; do
    if [ -f "$f" ]; then
        touch "$f"
        echo "  touched: $(basename $f)"
    fi
done

echo "=== 3. Build libnimcp ==="
cd "$BUILD_DIR"
make nimcp -j4 2>&1 | grep -E "Building C|Building CUDA|Linking|error:" | tail -8

echo "=== 4. Build Python module ==="
make nimcp_python -j4 2>&1 | grep -E "Building C|Linking|error:" | tail -4

echo "=== 5. Verify timestamps ==="
stat --format='  libnimcp: %y' "$BUILD_DIR/lib/libnimcp.so.2.6.3"
stat --format='  python:   %y' "$BUILD_DIR/lib/python/nimcp.so"

echo "=== 6. Install ==="
cp "$BUILD_DIR/lib/python/nimcp.so" "$SITE_PACKAGES/nimcp.cpython-312-x86_64-linux-gnu.so"
cp "$BUILD_DIR/lib/python/nimcp.so" "$SITE_PACKAGES/nimcp.so"

echo "=== 7. Verify checksums ==="
UNIQUE=$(md5sum "$BUILD_DIR/lib/python/nimcp.so" "$SITE_PACKAGES/nimcp.so" | awk '{print $1}' | sort -u | wc -l)
if [ "$UNIQUE" -eq 1 ]; then
    echo "  CHECKSUM MATCH"
else
    echo "  CHECKSUM MISMATCH!"
    exit 1
fi

echo "=== BUILD OK ==="
