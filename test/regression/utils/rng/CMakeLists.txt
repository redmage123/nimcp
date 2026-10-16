# ============================================================================
# RNG Thread Safety Regression Tests
# ============================================================================
# WHAT: Regression tests for thread-safe RNG implementation
# WHY:  Prevent reintroduction of unsafe rand() usage
# HOW:  Multi-threaded stress tests, statistical validation, determinism checks
#
# BUG HISTORY:
# - Bug #1: Unsafe stdlib rand() in multithreaded code
# - Bug #2: Box-Muller cache corruption under concurrent access
# - Bug #3: Non-atomic statistics tracking
#
# THREADSANITIZER: These tests are designed to be TSan-friendly
# Run with: cmake -DNIMCP_TSAN=ON .. && make && ctest -L regression
# ============================================================================

# ----------------------------------------------------------------------------
# Thread Safety Regression Test
# ----------------------------------------------------------------------------
# WHAT: Comprehensive thread safety tests for nimcp_rand module
# WHY:  Verify no data races, correct statistics, deterministic reproducibility
# HOW:  8 threads, 10000 ops/thread, statistical validation

add_executable(regression_utils_rng_test_rand_thread_safety
    test_rand_thread_safety_regression.cpp
)

target_link_libraries(regression_utils_rng_test_rand_thread_safety
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)

target_compile_features(regression_utils_rng_test_rand_thread_safety
    PRIVATE cxx_std_17
)

# Enable position-independent code for better TSan compatibility
set_target_properties(regression_utils_rng_test_rand_thread_safety
    PROPERTIES POSITION_INDEPENDENT_CODE ON
)

# Register test with CTest
add_test(
    NAME regression_utils_rng_test_rand_thread_safety
    COMMAND regression_utils_rng_test_rand_thread_safety
)

# Set test properties
set_tests_properties(regression_utils_rng_test_rand_thread_safety
    PROPERTIES
        LABELS "regression;utils;rng;thread_safety"
        TIMEOUT 120
)

# ----------------------------------------------------------------------------
# ThreadSanitizer-specific configuration
# ----------------------------------------------------------------------------
# Note: TSan flags are typically set globally via -DNIMCP_TSAN=ON
# Additional TSan options can be passed via environment variable:
# TSAN_OPTIONS="history_size=7 second_deadlock_stack=1"
