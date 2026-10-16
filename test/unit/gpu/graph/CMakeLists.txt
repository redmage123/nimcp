# ============================================================================
# GPU Graph Recovery Unit Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for GPU graph module recovery unit tests
# WHY:  Enable testing of GPU recovery in graph operations
# HOW:  Link against GTest framework and nimcp library
# ============================================================================

# Find GTest
find_package(GTest QUIET)

if(GTest_FOUND AND CUDAToolkit_FOUND)
    message(STATUS "Adding GPU graph recovery unit tests (GTest)...")

    # ========================================================================
    # Test: Graph GPU Recovery
    # ========================================================================
    # DISABLED: Test needs implementation - nimcp_gpu_graph_count_triangles,
    #           nimcp_gpu_graph_is_connected, nimcp_gpu_graph_bfs_multi_source
    #           not implemented
    # TODO: Implement graph GPU functions
    # add_executable(unit_gpu_graph_test_graph_gpu_recovery
    #     test_graph_gpu_recovery.cpp
    # )
    #
    # target_link_libraries(unit_gpu_graph_test_graph_gpu_recovery
    #     PRIVATE
    #         nimcp
    #         GTest::gtest
    #         GTest::gtest_main
    #         ${CUDA_LIBRARIES}
    #         m
    #         pthread
    # )
    #
    # target_include_directories(unit_gpu_graph_test_graph_gpu_recovery
    #     PRIVATE
    #         ${CMAKE_SOURCE_DIR}/include
    #         ${CMAKE_SOURCE_DIR}/src
    #         ${CUDA_INCLUDE_DIRS}
    # )
    #
    # add_test(NAME unit_gpu_graph_recovery COMMAND unit_gpu_graph_test_graph_gpu_recovery)
    # set_tests_properties(unit_gpu_graph_recovery PROPERTIES
    #     TIMEOUT 180
    #     LABELS "unit;gpu;recovery;graph"
    # )

    # ========================================================================
    # Test: Knowledge Graph GPU Recovery
    # ========================================================================
    add_executable(unit_gpu_graph_test_knowledge_graph_gpu_recovery
        test_knowledge_graph_gpu_recovery.cpp
    )

    target_link_libraries(unit_gpu_graph_test_knowledge_graph_gpu_recovery
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(unit_gpu_graph_test_knowledge_graph_gpu_recovery
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    add_test(NAME unit_gpu_knowledge_graph_recovery COMMAND unit_gpu_graph_test_knowledge_graph_gpu_recovery)
    set_tests_properties(unit_gpu_knowledge_graph_recovery PROPERTIES
        TIMEOUT 180
        LABELS "unit;gpu;recovery;knowledge;graph"
    )

else()
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found - skipping GPU graph recovery unit tests")
    endif()
    if(NOT CUDAToolkit_FOUND)
        message(STATUS "CUDA toolkit not found - skipping GPU graph recovery unit tests")
    endif()
endif()
