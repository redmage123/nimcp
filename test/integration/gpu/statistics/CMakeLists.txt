# ============================================================================
# GPU Statistics Integration Tests CMakeLists.txt
# ============================================================================
# WHAT: Integration tests for GPU-accelerated statistics operations
# WHY:  Verify GPU statistics kernels integrate correctly with the system
# HOW:  Test GPU/CPU equivalence, backend switching, full pipeline tests
# ============================================================================

# Guard: CUDA toolkit required for GPU statistics integration tests
if(NOT CUDAToolkit_FOUND)
    message(STATUS "  Skipping GPU statistics integration tests (CUDA toolkit not found)")
    return()
endif()

message(STATUS "  Building GPU statistics integration tests (CUDA toolkit found)")

# Helper macro for adding GPU statistics integration tests
macro(add_statistics_gpu_integration_test TEST_NAME)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_NAME}.cpp)
        add_executable(integration_gpu_statistics_${TEST_NAME}
            ${TEST_NAME}.cpp
        )
        target_link_libraries(integration_gpu_statistics_${TEST_NAME}
            PRIVATE
                GTest::GTest
                GTest::Main
                nimcp
                m
                pthread
        )
        target_include_directories(integration_gpu_statistics_${TEST_NAME}
            PRIVATE
                ${PROJECT_SOURCE_DIR}/include
        )
        target_compile_features(integration_gpu_statistics_${TEST_NAME} PRIVATE cxx_std_20)
        target_compile_definitions(integration_gpu_statistics_${TEST_NAME} PRIVATE NIMCP_TESTING)
        add_test(NAME integration_gpu_statistics_${TEST_NAME}
                 COMMAND integration_gpu_statistics_${TEST_NAME})
        set_tests_properties(integration_gpu_statistics_${TEST_NAME}
            PROPERTIES
                LABELS "integration;gpu;statistics"
                TIMEOUT 180
        )
    endif()
endmacro()

# =============================================================================
# GPU Statistics Integration Tests
# =============================================================================

# DISABLED: Test needs API updates - nimcp_error_t, NIMCP_OK, nimcp_statistics_descriptive
#           not declared (should use current statistics API)
# TODO: Update test to use current statistics API
# add_statistics_gpu_integration_test(test_statistics_gpu_integration)
