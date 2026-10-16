# ============================================================================
# Memory Store End-to-End Tests
# ============================================================================
# WHAT: E2E tests for NIMCP persistent memory store under realistic workloads
# WHY:  Verify 10K-step training simulations, persistence across restarts,
#        consolidation pipelines, edge-device constraints, and search precision
# HOW:  GTest-based tests with large data volumes and timed assertions
# ============================================================================

add_executable(e2e_memory_test_memory_store
    test_memory_store_e2e.cpp
)

target_include_directories(e2e_memory_test_memory_store PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/test
)

target_link_libraries(e2e_memory_test_memory_store
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(e2e_memory_test_memory_store PRIVATE cxx_std_17)
target_compile_definitions(e2e_memory_test_memory_store PRIVATE NIMCP_TESTING)

gtest_discover_tests(e2e_memory_test_memory_store
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "e2e;memory;memory_store"
        TIMEOUT 600
)
