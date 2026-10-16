# ============================================================================
# Brain Processing Unit Tests
# ============================================================================

# Test 1: Cognitive Logic Circuits
add_test_binary(
    test_cognitive_logic_circuits
    ${CMAKE_CURRENT_SOURCE_DIR}/test_cognitive_logic_circuits.cpp
    unit
)

# Test 2: Cognitive Processor
add_test_binary(
    test_cognitive_processor
    ${CMAKE_CURRENT_SOURCE_DIR}/test_cognitive_processor.cpp
    unit
)

# Test 3: Multimodal Integrator
add_test_binary(
    test_multimodal_integrator
    ${CMAKE_CURRENT_SOURCE_DIR}/test_multimodal_integrator.cpp
    unit
)
