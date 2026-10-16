# =============================================================================
# Middleware Exception Integration Tests
# =============================================================================

# Middleware Exception Integration Tests
add_executable(integration_middleware_exception_test_middleware_exception_integration
    test_middleware_exception_integration.cpp
)

target_link_libraries(integration_middleware_exception_test_middleware_exception_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        pthread
        m
)

target_compile_features(integration_middleware_exception_test_middleware_exception_integration PRIVATE cxx_std_17)

add_test(
    NAME integration_middleware_exception_test_middleware_exception_integration
    COMMAND integration_middleware_exception_test_middleware_exception_integration
)

set_tests_properties(integration_middleware_exception_test_middleware_exception_integration
    PROPERTIES
    LABELS "integration;middleware;exception"
    TIMEOUT 120
)
