# ============================================================================
# Inference GPU Unit Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for inference GPU unit tests
# WHY:  Enable testing of inference GPU kernels with recovery
# HOW:  Link against GTest framework and nimcp library
# ============================================================================

# Find GTest
find_package(GTest QUIET)

if(GTest_FOUND AND CUDAToolkit_FOUND)
    message(STATUS "Adding inference GPU unit tests (GTest)...")

    # ========================================================================
    # Test: Inference GPU Recovery
    # ========================================================================
    # DISABLED: Test needs API updates - nimcp_gpu_tensor_numel, nimcp_gpu_tensor_upload
    #           not declared (should use current GPU tensor API)
    # TODO: Update test to use current GPU tensor API
    # add_executable(unit_gpu_inference_recovery
    #     test_inference_gpu_recovery.cpp
    # )
    #
    # target_link_libraries(unit_gpu_inference_recovery
    #     PRIVATE
    #         nimcp
    #         GTest::gtest
    #         GTest::gtest_main
    #         ${CUDA_LIBRARIES}
    #         m
    #         pthread
    # )
    #
    # target_include_directories(unit_gpu_inference_recovery
    #     PRIVATE
    #         ${CMAKE_SOURCE_DIR}/include
    #         ${CMAKE_SOURCE_DIR}/src
    #         ${CUDA_INCLUDE_DIRS}
    # )
    #
    # target_compile_definitions(unit_gpu_inference_recovery
    #     PRIVATE
    #         NIMCP_ENABLE_CUDA=1
    # )
    #
    # add_test(NAME unit_gpu_inference_recovery COMMAND unit_gpu_inference_recovery)
    # set_tests_properties(unit_gpu_inference_recovery PROPERTIES
    #     TIMEOUT 180
    #     LABELS "unit;gpu;inference;recovery"
    # )

else()
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found - skipping inference GPU unit tests")
    endif()
    if(NOT CUDAToolkit_FOUND)
        message(STATUS "CUDA toolkit not found - skipping inference GPU unit tests")
    endif()
endif()
