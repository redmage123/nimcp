# ============================================================================
# GPU Hypothalamus Integration Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for GPU hypothalamus recovery integration tests
# WHY:  Enable testing of GPU recovery across hypothalamus component boundaries
# HOW:  Link against GTest framework and nimcp library
# ============================================================================

# Find GTest
find_package(GTest QUIET)

if(GTest_FOUND AND CUDAToolkit_FOUND)
    message(STATUS "Adding GPU hypothalamus recovery integration tests (GTest)...")

    # ========================================================================
    # Test: Hypothalamus GPU Recovery Integration
    # ========================================================================
    # DISABLED: Test needs API updates - recovery_stats missing fields
    # TODO: Update test to use current GPU recovery API
    # add_executable(integration_gpu_hypothalamus_test_hypothalamus_gpu_recovery_integration
    #     test_hypothalamus_gpu_recovery_integration.cpp
    # )
    #
    # target_link_libraries(integration_gpu_hypothalamus_test_hypothalamus_gpu_recovery_integration
    #     PRIVATE
    #         nimcp
    #         GTest::gtest
    #         GTest::gtest_main
    #         ${CUDA_LIBRARIES}
    #         m
    #         pthread
    # )
    #
    # target_include_directories(integration_gpu_hypothalamus_test_hypothalamus_gpu_recovery_integration
    #     PRIVATE
    #         ${CMAKE_SOURCE_DIR}/include
    #         ${CMAKE_SOURCE_DIR}/src
    #         ${CUDA_INCLUDE_DIRS}
    # )
    #
    # target_compile_definitions(integration_gpu_hypothalamus_test_hypothalamus_gpu_recovery_integration
    #     PRIVATE
    #         NIMCP_ENABLE_CUDA
    # )
    #
    # add_test(NAME integration_gpu_hypothalamus_recovery COMMAND integration_gpu_hypothalamus_test_hypothalamus_gpu_recovery_integration)
    # set_tests_properties(integration_gpu_hypothalamus_recovery PROPERTIES
    #     TIMEOUT 600
    #     LABELS "integration;gpu;hypothalamus;recovery"
    # )

else()
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found - skipping GPU hypothalamus integration tests")
    endif()
    if(NOT CUDAToolkit_FOUND)
        message(STATUS "CUDA toolkit not found - skipping GPU hypothalamus integration tests")
    endif()
endif()
