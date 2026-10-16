# ============================================================================
# Inference GPU Integration Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for inference GPU integration tests
# WHY:  Enable testing of inference pipelines with recovery
# HOW:  Link against GTest framework and nimcp library
# ============================================================================

# Find GTest
find_package(GTest QUIET)

if(GTest_FOUND AND CUDAToolkit_FOUND)
    message(STATUS "Adding inference GPU integration tests (GTest)...")

    # ========================================================================
    # Test: Inference GPU Integration with Recovery
    # ========================================================================
    # DISABLED: Test needs API updates - nimcp_gpu_tensor_numel/upload not declared
    # TODO: Update test to use current GPU tensor API
    # add_executable(integration_gpu_inference_recovery
    #     test_inference_gpu_integration.cpp
    # )
    #
    # target_link_libraries(integration_gpu_inference_recovery
    #     PRIVATE
    #         nimcp
    #         GTest::gtest
    #         GTest::gtest_main
    #         ${CUDA_LIBRARIES}
    #         m
    #         pthread
    # )
    #
    # target_include_directories(integration_gpu_inference_recovery
    #     PRIVATE
    #         ${CMAKE_SOURCE_DIR}/include
    #         ${CMAKE_SOURCE_DIR}/src
    #         ${CUDA_INCLUDE_DIRS}
    # )
    #
    # target_compile_definitions(integration_gpu_inference_recovery
    #     PRIVATE
    #         NIMCP_ENABLE_CUDA=1
    # )
    #
    # add_test(NAME integration_gpu_inference_recovery COMMAND integration_gpu_inference_recovery)
    # set_tests_properties(integration_gpu_inference_recovery PROPERTIES
    #     TIMEOUT 300
    #     LABELS "integration;gpu;inference;recovery"
    # )

else()
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found - skipping inference GPU integration tests")
    endif()
    if(NOT CUDAToolkit_FOUND)
        message(STATUS "CUDA toolkit not found - skipping inference GPU integration tests")
    endif()
endif()
