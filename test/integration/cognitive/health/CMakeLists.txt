# ============================================================================
# Phase 8 Health Cognitive Integration Tests
# ============================================================================

# Health Cognitive Integration Test
add_executable(integration_cognitive_health_cognitive_integration
    test_health_cognitive_integration.cpp
)

target_link_libraries(integration_cognitive_health_cognitive_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_health_cognitive_integration
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_cognitive_health_cognitive_integration PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_health_cognitive_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;health;phase8"
        TIMEOUT 180
)

# Health Cognitive E2E Test
add_executable(integration_cognitive_health_cognitive_e2e
    test_health_cognitive_e2e.cpp
)

target_link_libraries(integration_cognitive_health_cognitive_e2e
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_health_cognitive_e2e
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_cognitive_health_cognitive_e2e PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_health_cognitive_e2e
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;health;e2e;phase8"
        TIMEOUT 300
)
