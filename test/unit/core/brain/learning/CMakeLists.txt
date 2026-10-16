# ============================================================================
# Brain Learning Unit Tests
# ============================================================================

# Test 1: Circuit Compilation
add_test_binary(
    test_circuit_compilation
    ${CMAKE_CURRENT_SOURCE_DIR}/test_circuit_compilation.cpp
    unit
)

# Test 2: Brain Learning Bio-Async
add_test_binary(
    test_brain_learning_bio_async
    ${CMAKE_CURRENT_SOURCE_DIR}/test_brain_learning_bio_async.cpp
    unit
)

# Test 3: Brain Learning Security
add_test_binary(
    test_brain_learning_security
    ${CMAKE_CURRENT_SOURCE_DIR}/test_brain_learning_security.cpp
    unit
)
