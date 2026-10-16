# ============================================================================
# Imagination FEP Regression Tests
# ============================================================================
# Performance, memory, numerical stability, and consistency regression tests
# for the Imagination + FEP integration
# ============================================================================

# ----------------------------------------------------------------------------
# Regression Test: Imagination FEP Bridge Regression
# ----------------------------------------------------------------------------
add_executable(regression_imagination_fep
    test_imagination_fep_regression.cpp
)

target_link_libraries(regression_imagination_fep
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_imagination_fep
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(regression_imagination_fep PRIVATE cxx_std_17)
target_compile_options(regression_imagination_fep PRIVATE -O2)

gtest_discover_tests(regression_imagination_fep
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;imagination;fep;performance;memory"
        TIMEOUT 600
)
