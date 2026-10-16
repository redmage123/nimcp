# ============================================================================
# GPU Swarm Integration Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for GPU swarm recovery integration tests
# WHY:  Enable testing of GPU recovery across swarm component boundaries
# HOW:  Link against GTest framework and nimcp library
# ============================================================================

# Find GTest
find_package(GTest QUIET)

if(GTest_FOUND AND CUDAToolkit_FOUND)
    message(STATUS "Adding GPU swarm recovery integration tests (GTest)...")

    # ========================================================================
    # Test: Swarm GPU Recovery Integration
    # ========================================================================
    # DISABLED: Test needs API updates - recovery_stats missing fields,
    #           consensus params missing convergence_threshold
    # TODO: Update test to use current swarm and recovery API
    # add_executable(integration_gpu_swarm_test_swarm_gpu_recovery_integration
    #     test_swarm_gpu_recovery_integration.cpp
    # )
    #
    # target_link_libraries(integration_gpu_swarm_test_swarm_gpu_recovery_integration
    #     PRIVATE
    #         nimcp
    #         GTest::gtest
    #         GTest::gtest_main
    #         ${CUDA_LIBRARIES}
    #         m
    #         pthread
    # )
    #
    # target_include_directories(integration_gpu_swarm_test_swarm_gpu_recovery_integration
    #     PRIVATE
    #         ${CMAKE_SOURCE_DIR}/include
    #         ${CMAKE_SOURCE_DIR}/src
    #         ${CUDA_INCLUDE_DIRS}
    # )
    #
    # target_compile_definitions(integration_gpu_swarm_test_swarm_gpu_recovery_integration
    #     PRIVATE
    #         NIMCP_ENABLE_CUDA
    # )
    #
    # add_test(NAME integration_gpu_swarm_recovery COMMAND integration_gpu_swarm_test_swarm_gpu_recovery_integration)
    # set_tests_properties(integration_gpu_swarm_recovery PROPERTIES
    #     TIMEOUT 600
    #     LABELS "integration;gpu;swarm;recovery"
    # )

else()
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found - skipping GPU swarm integration tests")
    endif()
    if(NOT CUDAToolkit_FOUND)
        message(STATUS "CUDA toolkit not found - skipping GPU swarm integration tests")
    endif()
endif()
