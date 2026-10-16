# ============================================================================
# Training GPU Integration Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for training GPU integration tests
# WHY:  Enable end-to-end testing of training GPU modules with recovery
# HOW:  Link against GTest framework and nimcp library
# ============================================================================

# Find GTest
find_package(GTest QUIET)

if(GTest_FOUND AND CUDAToolkit_FOUND)
    message(STATUS "Adding training GPU integration tests (GTest)...")

    # ========================================================================
    # Test: Training Pipeline Integration
    # ========================================================================
    # Tests the complete training pipeline including forward pass, loss
    # computation, backward pass, and optimizer step with GPU acceleration.
    # ========================================================================
    # DISABLED: Test has extern "C" linkage error - CUDA templates conflict with C linkage
    # TODO: Fix extern "C" block in test source file
    # add_executable(integration_gpu_training_test_pipeline
    #     test_training_pipeline_integration.cpp
    # )
    #
    # target_link_libraries(integration_gpu_training_test_pipeline
    #     PRIVATE
    #         nimcp
    #         GTest::gtest
    #         GTest::gtest_main
    #         ${CUDA_LIBRARIES}
    #         m
    #         pthread
    # )
    #
    # target_include_directories(integration_gpu_training_test_pipeline
    #     PRIVATE
    #         ${CMAKE_SOURCE_DIR}/include
    #         ${CMAKE_SOURCE_DIR}/src
    #         ${CUDA_INCLUDE_DIRS}
    # )
    #
    # add_test(NAME integration_gpu_training_pipeline COMMAND integration_gpu_training_test_pipeline)
    # set_tests_properties(integration_gpu_training_pipeline PROPERTIES
    #     TIMEOUT 300
    #     LABELS "integration;gpu;training;recovery"
    # )

    # ========================================================================
    # Test: Checkpoint Integration
    # ========================================================================
    # Tests gradient and activation checkpointing with full forward/backward
    # passes, memory management, and recomputation workflows.
    # ========================================================================
    # DISABLED: Test needs API updates - NIMCP_DTYPE_FLOAT32, nimcp_gpu_tensor_copy_*,
    #           nimcp_gpu_tensor_destroy signature mismatch
    # TODO: Update test to use current GPU tensor API
    # add_executable(integration_gpu_training_test_checkpoint
    #     test_checkpoint_integration.cpp
    # )
    #
    # target_link_libraries(integration_gpu_training_test_checkpoint
    #     PRIVATE
    #         nimcp
    #         GTest::gtest
    #         GTest::gtest_main
    #         ${CUDA_LIBRARIES}
    #         m
    #         pthread
    # )
    #
    # target_include_directories(integration_gpu_training_test_checkpoint
    #     PRIVATE
    #         ${CMAKE_SOURCE_DIR}/include
    #         ${CMAKE_SOURCE_DIR}/src
    #         ${CUDA_INCLUDE_DIRS}
    # )
    #
    # add_test(NAME integration_gpu_training_checkpoint COMMAND integration_gpu_training_test_checkpoint)
    # set_tests_properties(integration_gpu_training_checkpoint PROPERTIES
    #     TIMEOUT 300
    #     LABELS "integration;gpu;training;checkpoint;recovery"
    # )

else()
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found - skipping training GPU integration tests")
    endif()
    if(NOT CUDAToolkit_FOUND)
        message(STATUS "CUDA toolkit not found - skipping training GPU integration tests")
    endif()
endif()
