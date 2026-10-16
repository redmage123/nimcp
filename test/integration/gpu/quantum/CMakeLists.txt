# ============================================================================
# GPU Quantum Recovery Integration Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for GPU quantum module recovery integration tests
# WHY:  Enable end-to-end testing of GPU recovery in quantum workflows
# HOW:  Link against GTest framework and nimcp library
# ============================================================================

# Find GTest
find_package(GTest QUIET)

if(GTest_FOUND AND CUDAToolkit_FOUND)
    message(STATUS "Adding GPU quantum recovery integration tests (GTest)...")

    # ========================================================================
    # Test: Quantum GPU Recovery Integration
    # ========================================================================
    # DISABLED: Test needs API updates - nimcp_qmc_*, nimcp_mcts_*, nimcp_quantum_state_*
    #           functions not declared
    # TODO: Update test to use current quantum and MCTS GPU API
    # add_executable(integration_gpu_quantum_test_quantum_gpu_recovery_integration
    #     test_quantum_gpu_recovery_integration.cpp
    # )
    #
    # target_link_libraries(integration_gpu_quantum_test_quantum_gpu_recovery_integration
    #     PRIVATE
    #         nimcp
    #         GTest::gtest
    #         GTest::gtest_main
    #         ${CUDA_LIBRARIES}
    #         m
    #         pthread
    # )
    #
    # target_include_directories(integration_gpu_quantum_test_quantum_gpu_recovery_integration
    #     PRIVATE
    #         ${CMAKE_SOURCE_DIR}/include
    #         ${CMAKE_SOURCE_DIR}/src
    #         ${CUDA_INCLUDE_DIRS}
    # )
    #
    # add_test(NAME integration_gpu_quantum_recovery COMMAND integration_gpu_quantum_test_quantum_gpu_recovery_integration)
    # set_tests_properties(integration_gpu_quantum_recovery PROPERTIES
    #     TIMEOUT 300
    #     LABELS "integration;gpu;recovery;quantum"
    # )

else()
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found - skipping GPU quantum recovery integration tests")
    endif()
    if(NOT CUDAToolkit_FOUND)
        message(STATUS "CUDA toolkit not found - skipping GPU quantum recovery integration tests")
    endif()
endif()
