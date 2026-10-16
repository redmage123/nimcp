# ============================================================================
# Memory Store Integration Tests
# ============================================================================
# WHAT: Integration tests for NIMCP persistent memory store
# WHY:  Verify multi-table workflows, concurrency, vector accuracy,
#        graph traversal, and persistence across store restarts
# HOW:  GTest-based tests with realistic data volumes and multi-threading
# ============================================================================

add_executable(integration_memory_test_memory_store
    test_memory_store_integration.cpp
)

target_include_directories(integration_memory_test_memory_store PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/test
)

target_link_libraries(integration_memory_test_memory_store
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(integration_memory_test_memory_store PRIVATE cxx_std_17)
target_compile_definitions(integration_memory_test_memory_store PRIVATE NIMCP_TESTING)

gtest_discover_tests(integration_memory_test_memory_store
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;memory;memory_store"
        TIMEOUT 300
)

# ============================================================================
# OODB + SQLite Store Integration Tests
# ============================================================================

add_executable(integration_memory_test_oodb_integration
    test_oodb_integration.cpp
)

target_include_directories(integration_memory_test_oodb_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/test
)

target_link_libraries(integration_memory_test_oodb_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(integration_memory_test_oodb_integration PRIVATE cxx_std_17)
target_compile_definitions(integration_memory_test_oodb_integration PRIVATE NIMCP_TESTING)

gtest_discover_tests(integration_memory_test_oodb_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;memory;oodb"
        TIMEOUT 300
)
