# =============================================================================
# Ternary Integration Tests CMakeLists.txt
# =============================================================================
#
# Integration tests for ternary representation across NIMCP modules.
#
# Tests:
# - test_neuralnet_ternary_integration: Ternary weights with neural network
# - test_sparse_ternary_integration: Ternary sparse synapses
# - test_lnn_ternary_integration: LNN with ternary recurrent weights
# - test_attention_ternary_integration: Ternary attention mechanisms
# - test_training_ternary_integration: QAT with ternary mode
# - test_swarm_ternary_integration: Swarm consensus with ternary voting
#
# Author: NIMCP Development Team
# Date: 2025-12-31
# =============================================================================

message(STATUS "Configuring ternary integration tests...")

# =============================================================================
# Macro for adding ternary integration tests
# =============================================================================
macro(add_ternary_integration_test TEST_NAME)
    set(TARGET_NAME integration_ternary_${TEST_NAME})

    add_executable(${TARGET_NAME}
        ${TEST_NAME}.cpp
    )

    target_link_libraries(${TARGET_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )

    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)

    # Add include directories for ternary headers
    target_include_directories(${TARGET_NAME}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/include/utils/ternary
    )

    # Register test with CTest
    add_test(NAME ${TARGET_NAME}
             COMMAND ${TARGET_NAME})

    # Set test properties
    set_tests_properties(${TARGET_NAME}
        PROPERTIES
            LABELS "integration;ternary"
            TIMEOUT 120
    )

    message(STATUS "  Added ternary integration test: ${TARGET_NAME}")
endmacro()

# =============================================================================
# Ternary Integration Tests
# =============================================================================

# Test ternary weights with full neural network forward pass
# Tests mixed float/ternary layer configurations and weight conversion
add_ternary_integration_test(test_neuralnet_ternary_integration)

# Test ternary sparse synapses with neuralnet module
# Tests sparse operations with cortical columns
add_ternary_integration_test(test_sparse_ternary_integration)

# Test LNN with ternary recurrent weights
# Tests ODE solver with ternary matrices and wiring patterns
add_ternary_integration_test(test_lnn_ternary_integration)

# Test ternary attention with cortical columns
# Tests attention gate with sparse coding and hard attention
add_ternary_integration_test(test_attention_ternary_integration)

# Test quantization-aware training with ternary mode
# Tests mixed precision and plasticity with ternary weights
add_ternary_integration_test(test_training_ternary_integration)

# Test swarm memory with ternary confidence
# Tests swarm voting with ternary consensus
# SKIP: Uses swarm_memory_*_ternary_confidence APIs that aren't implemented yet
# add_ternary_integration_test(test_swarm_ternary_integration)
message(STATUS "  [SKIP] test_swarm_ternary_integration - ternary confidence API not implemented")

# =============================================================================
# Summary
# =============================================================================
message(STATUS "Ternary integration tests configured:")
message(STATUS "  - integration_ternary_test_neuralnet_ternary_integration")
message(STATUS "  - integration_ternary_test_sparse_ternary_integration")
message(STATUS "  - integration_ternary_test_lnn_ternary_integration")
message(STATUS "  - integration_ternary_test_attention_ternary_integration")
message(STATUS "  - integration_ternary_test_training_ternary_integration")
message(STATUS "  - [SKIP] integration_ternary_test_swarm_ternary_integration")
