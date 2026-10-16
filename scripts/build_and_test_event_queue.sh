#!/bin/bash

echo "=== Building Event Queue Test ==="
cd /home/bbrelin/nimcp/build || exit 1

# Reconfigure CMake
echo "Reconfiguring CMake..."
cmake .. || exit 1

# Build the middleware library
echo "Building middleware library..."
make nimcp_middleware -j$(nproc) || exit 1

# Build the test
echo "Building event queue test..."
make unit_middleware_events_event_queue -j$(nproc) || exit 1

# Run the test
echo "=== Running Event Queue Test ==="
cd /home/bbrelin/nimcp
./test/unit_middleware_events_test_event_queue || exit 1

echo "=== Test Completed Successfully ==="
