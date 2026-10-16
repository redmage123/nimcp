# ============================================================================
# Neural GPU Integration Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for neural network GPU integration tests
# WHY:  Enable testing of cross-module recovery (SNN, LNN, CNN)
# HOW:  Link against GTest framework and nimcp library
# ============================================================================

# Find GTest
find_package(GTest QUIET)

if(GTest_FOUND AND CUDAToolkit_FOUND)
    message(STATUS "Adding neural GPU integration tests (GTest)...")

    # ========================================================================
    # Test: Neural GPU Recovery Integration
    # ========================================================================
    add_executable(integration_gpu_neural_recovery
        test_neural_gpu_recovery_integration.cpp
    )

    target_link_libraries(integration_gpu_neural_recovery
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(integration_gpu_neural_recovery
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    target_compile_definitions(integration_gpu_neural_recovery
        PRIVATE
            $<$<BOOL:${CUDAToolkit_FOUND}>:NIMCP_ENABLE_CUDA>
    )

    add_test(NAME integration_gpu_neural_recovery COMMAND integration_gpu_neural_recovery)
    set_tests_properties(integration_gpu_neural_recovery PROPERTIES
        TIMEOUT 300
        LABELS "integration;gpu;neural;recovery"
    )

else()
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found - skipping neural GPU integration tests")
    endif()
    if(NOT CUDAToolkit_FOUND)
        message(STATUS "CUDA toolkit not found - skipping neural GPU integration tests")
    endif()
endif()
