# Cognitive Substrate Bridges Integration Tests
# Tests integration of all 8 cognitive substrate bridges with shared neural substrate
# NOTE: ToM substrate bridge API now unified (cognitive/tom/ provides all functions)

add_executable(integration_cognitive_substrate_bridges
    test_cognitive_substrate_bridges_integration.cpp
)

target_link_libraries(integration_cognitive_substrate_bridges
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_substrate_bridges
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_cognitive_substrate_bridges PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_substrate_bridges
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;substrate"
        TIMEOUT 120
)
