# ============================================================================
# GPU Graph Recovery Integration Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for GPU graph module recovery integration tests
# WHY:  Enable end-to-end testing of GPU recovery in graph workflows
# HOW:  Link against GTest framework and nimcp library
# ============================================================================

# Find GTest
find_package(GTest QUIET)

if(GTest_FOUND AND CUDAToolkit_FOUND)
    message(STATUS "Adding GPU graph recovery integration tests (GTest)...")

    # ========================================================================
    # Test: Graph GPU Recovery Integration
    # ========================================================================
    # DISABLED: Test needs implementation - nimcp_gpu_graph_* functions not implemented
    # TODO: Implement graph GPU functions
    # add_executable(integration_gpu_graph_test_graph_gpu_recovery_integration
    #     test_graph_gpu_recovery_integration.cpp
    # )
    #
    # target_link_libraries(integration_gpu_graph_test_graph_gpu_recovery_integration
    #     PRIVATE
    #         nimcp
    #         GTest::gtest
    #         GTest::gtest_main
    #         ${CUDA_LIBRARIES}
    #         m
    #         pthread
    # )
    #
    # target_include_directories(integration_gpu_graph_test_graph_gpu_recovery_integration
    #     PRIVATE
    #         ${CMAKE_SOURCE_DIR}/include
    #         ${CMAKE_SOURCE_DIR}/src
    #         ${CUDA_INCLUDE_DIRS}
    # )
    #
    # add_test(NAME integration_gpu_graph_recovery COMMAND integration_gpu_graph_test_graph_gpu_recovery_integration)
    # set_tests_properties(integration_gpu_graph_recovery PROPERTIES
    #     TIMEOUT 300
    #     LABELS "integration;gpu;recovery;graph;knowledge"
    # )

else()
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found - skipping GPU graph recovery integration tests")
    endif()
    if(NOT CUDAToolkit_FOUND)
        message(STATUS "CUDA toolkit not found - skipping GPU graph recovery integration tests")
    endif()
endif()
