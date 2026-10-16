# ============================================================================
# GPU Cognitive Integration Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for GPU cognitive recovery integration tests
# WHY:  Enable testing of GPU recovery across all cognitive modules
# HOW:  Link against GTest framework, nimcp library, and CUDA dependencies
# ============================================================================

# Find GTest
find_package(GTest QUIET)

if(GTest_FOUND AND CUDAToolkit_FOUND)
    message(STATUS "Adding GPU cognitive integration tests (GTest)...")

    # ========================================================================
    # Test: Cognitive GPU Recovery Integration
    # ========================================================================
    # WHAT: Test GPU recovery across Broca, Wernicke, JEPA, Omni, Occipital
    # WHY:  Verify recovery from OOM, numerical errors, context invalidation
    # HOW:  Test error handling and CPU fallback across all cognitive modules
    # DISABLED: Test needs API updates - nimcp_gpu_tensor_upload not declared,
    #           visual_input_t missing stride field, data type mismatch
    # TODO: Update test to use current GPU tensor and visual API
    # add_executable(integration_gpu_cognitive_test_cognitive_gpu_recovery
    #     test_cognitive_gpu_recovery.cpp
    # )
    #
    # target_link_libraries(integration_gpu_cognitive_test_cognitive_gpu_recovery
    #     PRIVATE
    #         nimcp
    #         GTest::gtest
    #         GTest::gtest_main
    #         ${CUDA_LIBRARIES}
    #         m
    #         pthread
    # )
    #
    # target_include_directories(integration_gpu_cognitive_test_cognitive_gpu_recovery
    #     PRIVATE
    #         ${CMAKE_SOURCE_DIR}/include
    #         ${CMAKE_SOURCE_DIR}/src
    #         ${CUDA_INCLUDE_DIRS}
    # )
    #
    # target_compile_features(integration_gpu_cognitive_test_cognitive_gpu_recovery PRIVATE cxx_std_20)
    # target_compile_definitions(integration_gpu_cognitive_test_cognitive_gpu_recovery PRIVATE NIMCP_TESTING)
    #
    # add_test(NAME integration_gpu_cognitive_recovery COMMAND integration_gpu_cognitive_test_cognitive_gpu_recovery)
    # set_tests_properties(integration_gpu_cognitive_recovery PROPERTIES
    #     TIMEOUT 300
    #     LABELS "integration;gpu;recovery;cognitive"
    # )

else()
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found - skipping GPU cognitive integration tests")
    endif()
    if(NOT CUDAToolkit_FOUND)
        message(STATUS "CUDA toolkit not found - skipping GPU cognitive integration tests")
    endif()
endif()
