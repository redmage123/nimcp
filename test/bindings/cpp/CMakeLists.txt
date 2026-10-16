# ============================================================================
# NIMCP C++ Bindings Test Suite
# ============================================================================

add_executable(cpp_bindings_test
    test_cpp_bindings.cpp
)

target_link_libraries(cpp_bindings_test PRIVATE
    nimcp_cpp
    GTest::gtest_main
    pthread
    m
)

gtest_discover_tests(cpp_bindings_test
    PROPERTIES
        TIMEOUT 120
        LABELS "bindings;cpp"
)
