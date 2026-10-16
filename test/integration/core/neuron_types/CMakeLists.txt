# ============================================================================
# Neuron Types Module Bio-Async Integration Tests
# ============================================================================
#
# WHAT: Integration tests for neuron_types and neural_logic bio-async communication
# WHY:  Verify logic gates, spike events, and variable binding via bio-async
# HOW:  Create logic gates, test message passing, verify circuit evaluation
#

# Test 1: Neuron Types Bio-Async Integration
add_executable(integration_neuron_types_bio_async_test
    test_neuron_types_integration.cpp
)

target_link_libraries(integration_neuron_types_bio_async_test
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)

target_include_directories(integration_neuron_types_bio_async_test
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)

gtest_discover_tests(integration_neuron_types_bio_async_test)
