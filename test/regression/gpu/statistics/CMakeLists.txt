# ============================================================================
# GPU Statistics Regression Tests CMakeLists.txt
# ============================================================================
# WHAT: Regression tests for GPU-accelerated statistics operations
# WHY:  Prevent reintroduction of fixed GPU statistics bugs
# HOW:  Test specific bug scenarios, numerical stability, edge cases
# ============================================================================

# Guard: CUDA toolkit required for GPU statistics regression tests
if(NOT CUDAToolkit_FOUND)
    message(STATUS "  Skipping GPU statistics regression tests (CUDA toolkit not found)")
    return()
endif()

message(STATUS "  Building GPU statistics regression tests (CUDA toolkit found)")

# Helper macro for adding GPU statistics regression tests
macro(add_statistics_gpu_regression_test TEST_NAME)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_NAME}.cpp)
        add_executable(regression_gpu_statistics_${TEST_NAME}
            ${TEST_NAME}.cpp
        )
        target_link_libraries(regression_gpu_statistics_${TEST_NAME}
            PRIVATE
                GTest::GTest
                GTest::Main
                nimcp
                m
                pthread
        )
        target_include_directories(regression_gpu_statistics_${TEST_NAME}
            PRIVATE
                ${PROJECT_SOURCE_DIR}/include
        )
        target_compile_features(regression_gpu_statistics_${TEST_NAME} PRIVATE cxx_std_20)
        target_compile_definitions(regression_gpu_statistics_${TEST_NAME} PRIVATE NIMCP_TESTING)
        add_test(NAME regression_gpu_statistics_${TEST_NAME}
                 COMMAND regression_gpu_statistics_${TEST_NAME})
        set_tests_properties(regression_gpu_statistics_${TEST_NAME}
            PROPERTIES
                LABELS "regression;gpu;statistics"
                TIMEOUT 120
        )
    endif()
endmacro()

# =============================================================================
# GPU Statistics Regression Tests
# =============================================================================

# GPU statistics regression tests (numerical stability, edge cases)
add_statistics_gpu_regression_test(test_statistics_gpu_regression)
