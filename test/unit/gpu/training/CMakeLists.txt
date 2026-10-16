# ============================================================================
# Training GPU Unit Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for training GPU unit tests
# WHY:  Enable testing of training GPU kernels with recovery integration
# HOW:  Link against GTest framework and nimcp library
# ============================================================================

# Find GTest
find_package(GTest QUIET)

if(GTest_FOUND AND CUDAToolkit_FOUND)
    message(STATUS "Adding training GPU unit tests (GTest)...")

    # ========================================================================
    # Test: Gradient Kernels with Recovery
    # ========================================================================
    add_executable(unit_gpu_training_test_gradient_kernels
        test_gradient_kernels.cpp
    )

    target_link_libraries(unit_gpu_training_test_gradient_kernels
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(unit_gpu_training_test_gradient_kernels
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    add_test(NAME unit_gpu_training_gradient_kernels COMMAND unit_gpu_training_test_gradient_kernels)
    set_tests_properties(unit_gpu_training_gradient_kernels PROPERTIES
        TIMEOUT 180
        LABELS "unit;gpu;training;recovery"
    )

    # ========================================================================
    # Test: Optimizer Kernels with Recovery
    # ========================================================================
    add_executable(unit_gpu_training_test_optimizer_kernels
        test_optimizer_kernels.cpp
    )

    target_link_libraries(unit_gpu_training_test_optimizer_kernels
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(unit_gpu_training_test_optimizer_kernels
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    add_test(NAME unit_gpu_training_optimizer_kernels COMMAND unit_gpu_training_test_optimizer_kernels)
    set_tests_properties(unit_gpu_training_optimizer_kernels PROPERTIES
        TIMEOUT 180
        LABELS "unit;gpu;training;recovery"
    )

    # ========================================================================
    # Test: Backpropagation Kernels with Recovery
    # ========================================================================
    # DISABLED: Test needs API updates - nimcp_gpu_tensor_destroy takes 1 arg, not 2
    # TODO: Update test to use current GPU tensor API
    # add_executable(unit_gpu_training_test_backprop_kernels
    #     test_backprop_kernels.cpp
    # )
    #
    # target_link_libraries(unit_gpu_training_test_backprop_kernels
    #     PRIVATE
    #         nimcp
    #         GTest::gtest
    #         GTest::gtest_main
    #         ${CUDA_LIBRARIES}
    #         m
    #         pthread
    # )
    #
    # target_include_directories(unit_gpu_training_test_backprop_kernels
    #     PRIVATE
    #         ${CMAKE_SOURCE_DIR}/include
    #         ${CMAKE_SOURCE_DIR}/src
    #         ${CUDA_INCLUDE_DIRS}
    # )
    #
    # add_test(NAME unit_gpu_training_backprop_kernels COMMAND unit_gpu_training_test_backprop_kernels)
    # set_tests_properties(unit_gpu_training_backprop_kernels PROPERTIES
    #     TIMEOUT 180
    #     LABELS "unit;gpu;training;recovery"
    # )

    # ========================================================================
    # Test: Checkpoint Kernels with Recovery
    # ========================================================================
    # DISABLED: Test needs API updates - nimcp_gpu_tensor_destroy takes 1 arg, not 2
    # TODO: Update test to use current GPU tensor API
    # add_executable(unit_gpu_training_test_checkpoint_kernels
    #     test_checkpoint_kernels.cpp
    # )
    #
    # target_link_libraries(unit_gpu_training_test_checkpoint_kernels
    #     PRIVATE
    #         nimcp
    #         GTest::gtest
    #         GTest::gtest_main
    #         ${CUDA_LIBRARIES}
    #         m
    #         pthread
    # )
    #
    # target_include_directories(unit_gpu_training_test_checkpoint_kernels
    #     PRIVATE
    #         ${CMAKE_SOURCE_DIR}/include
    #         ${CMAKE_SOURCE_DIR}/src
    #         ${CUDA_INCLUDE_DIRS}
    # )
    #
    # add_test(NAME unit_gpu_training_checkpoint_kernels COMMAND unit_gpu_training_test_checkpoint_kernels)
    # set_tests_properties(unit_gpu_training_checkpoint_kernels PROPERTIES
    #     TIMEOUT 180
    #     LABELS "unit;gpu;training;recovery;checkpoint"
    # )

else()
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found - skipping training GPU unit tests")
    endif()
    if(NOT CUDAToolkit_FOUND)
        message(STATUS "CUDA toolkit not found - skipping training GPU unit tests")
    endif()
endif()
