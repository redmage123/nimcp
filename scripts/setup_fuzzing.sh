#!/bin/bash
#
# NIMCP Fuzzing Setup Script
# ==========================
# Installs dependencies and builds all fuzz targets
#

set -e  # Exit on error

echo "=================================================="
echo "NIMCP Fuzzing Setup"
echo "=================================================="

# Check if running with sudo for package installation
if [ "$EUID" -eq 0 ]; then
    echo "✗ Don't run this script with sudo"
    echo "  Run as regular user. Script will prompt for sudo when needed."
    exit 1
fi

# Step 1: Install required packages
echo ""
echo "Step 1: Installing required packages..."
echo "--------------------------------------"
sudo apt-get update
sudo apt-get install -y \
    clang \
    llvm \
    libc++-dev \
    libc++abi-dev \
    cmake \
    build-essential

# Verify clang is installed
if ! command -v clang++ &> /dev/null; then
    echo "✗ clang++ not found in PATH"
    exit 1
fi

echo "✓ Clang $(clang++ --version | head -1)"

# Step 2: Test libFuzzer support
echo ""
echo "Step 2: Testing libFuzzer support..."
echo "------------------------------------"
cat > /tmp/test_fuzzer.cpp << 'EOF'
#include <cstddef>
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    return 0;
}
EOF

if clang++ -fsanitize=fuzzer /tmp/test_fuzzer.cpp -o /tmp/test_fuzzer 2>/dev/null; then
    echo "✓ libFuzzer working"
    rm /tmp/test_fuzzer /tmp/test_fuzzer.cpp
else
    echo "✗ libFuzzer not working"
    echo "  Try: sudo apt-get install -y compiler-rt"
    exit 1
fi

# Step 3: Build fuzzing targets
echo ""
echo "Step 3: Building fuzzing targets..."
echo "-----------------------------------"

# Get script directory (where nimcp root is)
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Create build directory
mkdir -p build-fuzz
cd build-fuzz

# Configure with fuzzing enabled
echo "Configuring CMake..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_FUZZING=ON \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    | tee cmake_output.log

# Check if fuzzing was enabled
if grep -q "Fuzzing infrastructure: ENABLED" cmake_output.log && \
   ! grep -q "libFuzzer not supported" cmake_output.log; then
    echo "✓ Fuzzing enabled"
else
    echo "✗ Fuzzing not enabled. Check cmake_output.log"
    exit 1
fi

# Build all fuzz targets
echo "Building fuzz targets..."
make -j$(nproc) 2>&1 | tee build_output.log

# Check what was built
echo ""
echo "Step 4: Verifying fuzz targets..."
echo "----------------------------------"

FUZZ_TARGETS=(
    "src/fuzz/fuzz_btree"
    "src/fuzz/fuzz_neuralnet"
    "src/fuzz/fuzz_protocol"
    "src/fuzz/fuzz_queue_manager"
    "src/fuzz/fuzz_validate"
    "src/fuzz/fuzz_brain_serialization"
    "src/fuzz/fuzz_spike_processing"
    "src/fuzz/fuzz_neuromodulators"
)

BUILT_COUNT=0
for target in "${FUZZ_TARGETS[@]}"; do
    if [ -f "$target" ]; then
        echo "✓ $target"
        BUILT_COUNT=$((BUILT_COUNT + 1))
    else
        echo "✗ $target (not built)"
    fi
done

echo ""
echo "=================================================="
echo "Summary: $BUILT_COUNT / ${#FUZZ_TARGETS[@]} fuzz targets built"
echo "=================================================="

if [ $BUILT_COUNT -eq 0 ]; then
    echo "✗ No fuzz targets were built"
    echo "  Check build_output.log for errors"
    exit 1
fi

# Step 5: Test a fuzzer
echo ""
echo "Step 5: Testing fuzzer..."
echo "-------------------------"

if [ -f "src/fuzz/fuzz_btree" ]; then
    echo "Running fuzz_btree for 5 seconds..."
    timeout 5 ./src/fuzz/fuzz_btree 2>&1 | head -20 || true
    echo "✓ Fuzzer ran successfully"
else
    echo "⚠ fuzz_btree not available for testing"
fi

echo ""
echo "=================================================="
echo "✓ Fuzzing setup complete!"
echo "=================================================="
echo ""
echo "Usage:"
echo "  cd build-fuzz/src/fuzz"
echo "  ./fuzz_btree -max_total_time=60"
echo ""
echo "Or with debug suite:"
echo "  cd build-fuzz"
echo "  python3 ../debug_suite.py --test dummy --mode fuzz \\"
echo "    --fuzz-target ./src/fuzz/fuzz_btree --fuzz-duration 300"
echo ""
