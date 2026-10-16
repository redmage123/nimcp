# ============================================================================
# Motor Cortex Regression Tests
# ============================================================================
# WHAT: Regression tests for motor cortex callback thread safety
# WHY:  Prevent race conditions and deadlocks in callback handling
# HOW:  Stress test concurrent callback invocation
# ============================================================================

# Motor Callback Race Regression Test
add_test_binary(
    regression_motor_callback_regression
    ${CMAKE_CURRENT_SOURCE_DIR}/test_motor_callback_regression.cpp
    regression
)

# Enable threading support
if(TARGET regression_motor_callback_regression)
    target_link_libraries(regression_motor_callback_regression PRIVATE Threads::Threads)
endif()
