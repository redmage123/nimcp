# ============================================================================
# Memory Store Regression Tests
# ============================================================================
# WHAT: Regression tests for NIMCP persistent memory store edge cases
# WHY:  Guard against NULL embeddings, NaN vectors, concurrent flush,
#        corrupt databases, zero-capacity configs, and overlong labels
# HOW:  GTest-based tests with deliberate API abuse and boundary conditions
# ============================================================================

add_executable(regression_memory_test_memory_store
    test_memory_store_regression.cpp
)

target_include_directories(regression_memory_test_memory_store PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/test
)

target_link_libraries(regression_memory_test_memory_store
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(regression_memory_test_memory_store PRIVATE cxx_std_17)
target_compile_definitions(regression_memory_test_memory_store PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_memory_test_memory_store
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;memory;memory_store"
        TIMEOUT 180
)

# ============================================================================
# OODB Regression Tests
# ============================================================================

add_executable(regression_memory_test_oodb_regression
    test_oodb_regression.cpp
)

target_include_directories(regression_memory_test_oodb_regression PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/test
)

target_link_libraries(regression_memory_test_oodb_regression
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(regression_memory_test_oodb_regression PRIVATE cxx_std_17)
target_compile_definitions(regression_memory_test_oodb_regression PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_memory_test_oodb_regression
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;memory;oodb"
        TIMEOUT 180
)
