# =============================================================================
# Middleware Exception Unit Tests
# =============================================================================

# Middleware Exception Handling Unit Tests
add_executable(unit_middleware_exception_test_middleware_exception_handling
    test_middleware_exception_handling.cpp
)

target_link_libraries(unit_middleware_exception_test_middleware_exception_handling
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        pthread
        m
)

target_compile_features(unit_middleware_exception_test_middleware_exception_handling PRIVATE cxx_std_17)

add_test(
    NAME unit_middleware_exception_test_middleware_exception_handling
    COMMAND unit_middleware_exception_test_middleware_exception_handling
)

set_tests_properties(unit_middleware_exception_test_middleware_exception_handling
    PROPERTIES
    LABELS "unit;middleware;exception"
    TIMEOUT 60
)
