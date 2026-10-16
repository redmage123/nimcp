# =============================================================================
# Bridges Exception Unit Tests
# =============================================================================

# Bridges Exception Handling Unit Tests
add_executable(unit_bridges_exception_test_bridges_exception_handling
    test_bridges_exception_handling.cpp
)

target_link_libraries(unit_bridges_exception_test_bridges_exception_handling
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        pthread
        m
)

target_compile_features(unit_bridges_exception_test_bridges_exception_handling PRIVATE cxx_std_17)

add_test(
    NAME unit_bridges_exception_test_bridges_exception_handling
    COMMAND unit_bridges_exception_test_bridges_exception_handling
)

set_tests_properties(unit_bridges_exception_test_bridges_exception_handling
    PROPERTIES
    LABELS "unit;bridges;exception"
    TIMEOUT 60
)
