# ============================================================================
# GPU Statistics Unit Tests CMakeLists.txt
# ============================================================================
# WHAT: Unit tests for GPU-accelerated statistics operations
# WHY:  Verify GPU statistics kernels produce correct results
# HOW:  Test individual GPU statistics functions, compare with CPU reference
# ============================================================================

# Guard: CUDA toolkit required for GPU statistics unit tests
if(NOT CUDAToolkit_FOUND)
    message(STATUS "  Skipping GPU statistics unit tests (CUDA toolkit not found)")
    return()
endif()

message(STATUS "  Building GPU statistics unit tests (CUDA toolkit found)")

# Helper macro for adding GPU statistics unit tests
macro(add_statistics_gpu_test TEST_NAME)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_NAME}.cpp)
        add_executable(unit_gpu_statistics_${TEST_NAME}
            ${TEST_NAME}.cpp
        )
        target_link_libraries(unit_gpu_statistics_${TEST_NAME}
            PRIVATE
                GTest::GTest
                GTest::Main
                nimcp
                m
                pthread
        )
        target_include_directories(unit_gpu_statistics_${TEST_NAME}
            PRIVATE
                ${PROJECT_SOURCE_DIR}/include
        )
        target_compile_features(unit_gpu_statistics_${TEST_NAME} PRIVATE cxx_std_17)
        target_compile_definitions(unit_gpu_statistics_${TEST_NAME} PRIVATE NIMCP_TESTING)
        add_test(NAME unit_gpu_statistics_${TEST_NAME}
                 COMMAND unit_gpu_statistics_${TEST_NAME})
        set_tests_properties(unit_gpu_statistics_${TEST_NAME}
            PROPERTIES
                LABELS "unit;gpu;statistics"
                TIMEOUT 120
        )
    endif()
endmacro()

# =============================================================================
# GPU Statistics Kernel Unit Tests
# =============================================================================

# Core statistics GPU tests
add_statistics_gpu_test(test_statistics_gpu)
