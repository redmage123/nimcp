# ============================================================================
# Reasoning Integration Tests
# ============================================================================

# Test 1: End-to-end reasoning
add_test_binary(
    test_end_to_end_reasoning
    ${CMAKE_CURRENT_SOURCE_DIR}/test_end_to_end_reasoning.cpp
    integration
)
# Test 2: Neural-symbolic bridge
add_test_binary(
    test_neural_symbolic_bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/test_neural_symbolic_bridge.cpp
    integration
)
# Test 3: Brain reasoning API
add_test_binary(
    test_brain_reasoning_api
    ${CMAKE_CURRENT_SOURCE_DIR}/test_brain_reasoning_api.cpp
    integration
)
# Test 4: Cognitive event flow
# DISABLED: Uses unimplemented API (get_learned_rule_count)
# add_test_binary(
#     test_cognitive_event_flow
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_cognitive_event_flow.cpp
#     integration
# )
# Test 5: Working memory reasoning
add_test_binary(
    test_working_memory_reasoning
    ${CMAKE_CURRENT_SOURCE_DIR}/test_working_memory_reasoning.cpp
    integration
)
# Test 6: Reasoning integration
# DISABLED: Uses unimplemented APIs (brain_learn_logical_rule, etc.)
# add_test_binary(
#     test_reasoning_integration
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_reasoning_integration.cpp
#     integration
# )

# Reasoning SNN-Plasticity Integration Tests
add_executable(integration_cognitive_reasoning_snn_plasticity
    test_reasoning_snn_plasticity_integration.cpp
)
target_link_libraries(integration_cognitive_reasoning_snn_plasticity
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(integration_cognitive_reasoning_snn_plasticity
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(integration_cognitive_reasoning_snn_plasticity PRIVATE cxx_std_17)
gtest_discover_tests(integration_cognitive_reasoning_snn_plasticity
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;reasoning;snn;plasticity"
        TIMEOUT 180
)
