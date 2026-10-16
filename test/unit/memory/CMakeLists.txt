# ============================================================================
# Memory Store Unit Tests
# ============================================================================
# WHAT: Unit tests for NIMCP persistent memory store (SQLite + FTS5 + vector)
# WHY:  Verify CRUD, text search, vector search, bloom filter, consolidation,
#        GC, and statistics in isolation
# HOW:  GTest-based tests with per-test temp SQLite databases
# ============================================================================

add_executable(unit_memory_test_memory_store
    test_memory_store.cpp
)

target_include_directories(unit_memory_test_memory_store PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/test
)

target_link_libraries(unit_memory_test_memory_store
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_memory_test_memory_store PRIVATE cxx_std_17)
target_compile_definitions(unit_memory_test_memory_store PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_memory_test_memory_store
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;memory;memory_store"
        TIMEOUT 120
)

# ============================================================================
# OODB (Object-Oriented Database) Cache Unit Tests
# ============================================================================

add_executable(unit_memory_test_memory_oodb
    test_memory_oodb.cpp
)

target_include_directories(unit_memory_test_memory_oodb PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/test
)

target_link_libraries(unit_memory_test_memory_oodb
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_memory_test_memory_oodb PRIVATE cxx_std_17)
target_compile_definitions(unit_memory_test_memory_oodb PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_memory_test_memory_oodb
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;memory;oodb"
        TIMEOUT 120
)

# ============================================================================
# Metadata Catalog Unit Tests
# ============================================================================

add_executable(unit_memory_test_metadata_catalog
    test_metadata_catalog.cpp
)

target_include_directories(unit_memory_test_metadata_catalog PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/test
)

target_link_libraries(unit_memory_test_metadata_catalog
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_memory_test_metadata_catalog PRIVATE cxx_std_17)
target_compile_definitions(unit_memory_test_metadata_catalog PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_memory_test_metadata_catalog
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;memory;metadata_catalog"
        TIMEOUT 120
)

# ============================================================================
# Remote Sensory Stream Unit Tests
# ============================================================================

add_executable(unit_memory_test_remote_stream
    test_remote_stream.cpp
)

target_include_directories(unit_memory_test_remote_stream PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/test
)

target_link_libraries(unit_memory_test_remote_stream
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_memory_test_remote_stream PRIVATE cxx_std_17)
target_compile_definitions(unit_memory_test_remote_stream PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_memory_test_remote_stream
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;memory;remote_stream"
        TIMEOUT 120
)
