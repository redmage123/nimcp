# Positional Encoding Regression Tests
# Stability and correctness regression tests for positional encoding module

# WHAT: Build positional encoding regression test
# WHY:  Ensure PE core implementation remains stable
# HOW:  Compile test with full nimcp linkage and extended timeout

add_executable(regression_utils_encoding_pe_stability
    test_pe_stability.cpp
)

target_link_libraries(regression_utils_encoding_pe_stability
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(regression_utils_encoding_pe_stability PRIVATE cxx_std_17)

# Optimize for performance testing while maintaining accuracy
target_compile_options(regression_utils_encoding_pe_stability
    PRIVATE -O2)

gtest_discover_tests(regression_utils_encoding_pe_stability
    PROPERTIES
        TIMEOUT 600
        LABELS "regression;utils;encoding;positional_encoding;stability"
)
