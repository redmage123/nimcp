# ============================================================================
# Knowledge Graph Integration Tests
# ============================================================================

# KG + Reasoning Integration Tests
add_executable(integration_cognitive_kg_reasoning
    test_kg_reasoning_integration.cpp
)

target_link_libraries(integration_cognitive_kg_reasoning
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_kg_reasoning
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_cognitive_kg_reasoning PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_kg_reasoning
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;knowledge;reasoning"
        TIMEOUT 300
)
