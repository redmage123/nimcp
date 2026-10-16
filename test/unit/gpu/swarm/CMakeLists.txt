# ============================================================================
# GPU Swarm Unit Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for GPU swarm recovery unit tests
# WHY:  Enable testing of GPU recovery in swarm operations
# HOW:  Link against GTest framework and nimcp library
# ============================================================================

# Find GTest
find_package(GTest QUIET)

if(GTest_FOUND AND CUDAToolkit_FOUND)
    message(STATUS "Adding GPU swarm recovery unit tests (GTest)...")

    # ========================================================================
    # Test: Swarm GPU Recovery
    # ========================================================================
    # DISABLED: Test needs API updates - nimcp_gpu_tensor_upload not declared
    # TODO: Update test to use current GPU tensor API
    # add_executable(unit_gpu_swarm_test_swarm_gpu_recovery
    #     test_swarm_gpu_recovery.cpp
    # )
    #
    # target_link_libraries(unit_gpu_swarm_test_swarm_gpu_recovery
    #     PRIVATE
    #         nimcp
    #         GTest::gtest
    #         GTest::gtest_main
    #         ${CUDA_LIBRARIES}
    #         m
    #         pthread
    # )
    #
    # target_include_directories(unit_gpu_swarm_test_swarm_gpu_recovery
    #     PRIVATE
    #         ${CMAKE_SOURCE_DIR}/include
    #         ${CMAKE_SOURCE_DIR}/src
    #         ${CUDA_INCLUDE_DIRS}
    # )
    #
    # target_compile_definitions(unit_gpu_swarm_test_swarm_gpu_recovery
    #     PRIVATE
    #         NIMCP_ENABLE_CUDA
    # )
    #
    # add_test(NAME unit_gpu_swarm_recovery COMMAND unit_gpu_swarm_test_swarm_gpu_recovery)
    # set_tests_properties(unit_gpu_swarm_recovery PROPERTIES
    #     TIMEOUT 300
    #     LABELS "unit;gpu;swarm;recovery"
    # )

else()
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found - skipping GPU swarm unit tests")
    endif()
    if(NOT CUDAToolkit_FOUND)
        message(STATUS "CUDA toolkit not found - skipping GPU swarm unit tests")
    endif()
endif()
