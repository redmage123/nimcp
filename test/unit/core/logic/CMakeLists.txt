# ============================================================================
# Neural Logic Unit Tests
# ============================================================================

# Test 1: Neural Logic Brain Integration
add_test_binary(
    test_neural_logic_brain_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_neural_logic_brain_integration.cpp
    unit
)

# Test 2: Neural Logic Attachment - DISABLED: Uses deprecated brain_get_neural_logic API
#add_test_binary(
#    test_neural_logic_attachment
#    ${CMAKE_CURRENT_SOURCE_DIR}/test_neural_logic_attachment.cpp
#    unit
#)

# Test 3: Neural Logic Circuit Builder
add_test_binary(
    test_neural_logic_circuit_builder
    ${CMAKE_CURRENT_SOURCE_DIR}/test_neural_logic_circuit_builder.cpp
    unit
)

# Test 4: Neural Logic Evaluation - DISABLED: Uses deprecated brain_get_neural_logic API
#add_test_binary(
#    test_neural_logic_evaluation
#    ${CMAKE_CURRENT_SOURCE_DIR}/test_neural_logic_evaluation.cpp
#    unit
#)

# Test 5: Neural Logic Factory - DISABLED: Uses deprecated brain_get_neural_logic API
#add_test_binary(
#    test_neural_logic_factory
#    ${CMAKE_CURRENT_SOURCE_DIR}/test_neural_logic_factory.cpp
#    unit
#)

# Test 6: Neural Logic Neuromodulation - DISABLED: Uses deprecated brain_get_neural_logic API
#add_test_binary(
#    test_neural_logic_neuromodulation
#    ${CMAKE_CURRENT_SOURCE_DIR}/test_neural_logic_neuromodulation.cpp
#    unit
#)

# Test 7: Neural Logic Quantum Bridge
add_test_binary(
    test_neural_logic_quantum_bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/test_neural_logic_quantum_bridge.cpp
    unit
)
