# ============================================================================
# Cognitive Exception Handling Integration Tests
# ============================================================================
# WHAT: Integration tests for cognitive module exception handling with immune system
# WHY:  Verify complete exception flow from cognitive modules through immune response
# HOW:  Test exception creation, immune presentation, recovery execution
# ============================================================================

# Cognitive Exception Integration Tests
add_executable(integration_cognitive_exception_integration
    test_cognitive_exception_integration.cpp
)

target_link_libraries(integration_cognitive_exception_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_exception_integration
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_cognitive_exception_integration PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_exception_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;exception;immune"
        TIMEOUT 180
)
