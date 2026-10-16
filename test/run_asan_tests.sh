#!/bin/bash
# Run tests with ASAN leak suppressions for third-party libraries
#
# Usage: ./run_asan_tests.sh [test_executable] [args...]
#
# If no test is specified, runs all E2E tests

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SUPPRESSIONS_FILE="${SCRIPT_DIR}/asan_suppressions.txt"

# Set LSAN options to use our suppression file
export LSAN_OPTIONS="suppressions=${SUPPRESSIONS_FILE}:print_suppressions=1"

# Also set ASAN options for better output
export ASAN_OPTIONS="detect_leaks=1:halt_on_error=0:print_legend=1"

if [ $# -eq 0 ]; then
    echo "Running all E2E tests with ASAN suppressions..."
    echo "Suppression file: ${SUPPRESSIONS_FILE}"
    echo ""

    # Find build directory (prefer build-asan)
    if [ -d "${SCRIPT_DIR}/../build-asan/test/e2e" ]; then
        BUILD_DIR="${SCRIPT_DIR}/../build-asan"
    elif [ -d "${SCRIPT_DIR}/../build/test/e2e" ]; then
        BUILD_DIR="${SCRIPT_DIR}/../build"
    else
        echo "Error: No build directory found"
        exit 1
    fi

    E2E_DIR="${BUILD_DIR}/test/e2e"

    # Run all E2E tests
    FAILED=0
    for test in "${E2E_DIR}"/e2e_test_*; do
        if [ -x "$test" ]; then
            echo "=========================================="
            echo "Running: $(basename $test)"
            echo "=========================================="
            timeout 120 "$test"
            if [ $? -ne 0 ]; then
                FAILED=$((FAILED + 1))
            fi
            echo ""
        fi
    done

    if [ $FAILED -eq 0 ]; then
        echo "All E2E tests passed!"
    else
        echo "WARNING: $FAILED test(s) had issues"
    fi
else
    # Run specified test
    exec "$@"
fi
