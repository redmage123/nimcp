# ============================================================================
# Core Neuralnet Unit Tests
# ============================================================================

# Test 1: Sparse Synapse Allocation
add_test_binary(
    test_sparse_synapse
    ${CMAKE_CURRENT_SOURCE_DIR}/test_sparse_synapse.cpp
    unit
)

# Test 2: Homeostasis-Immune Integration
add_test_binary(
    test_homeostasis_immune_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_homeostasis_immune_integration.cpp
    unit
)

# Test 3: Spike Ring Buffer
add_test_binary(
    test_spike_ring_buffer
    ${CMAKE_CURRENT_SOURCE_DIR}/test_spike_ring_buffer.cpp
    unit
)

# Test 4: Backprop Activation Reconstruction (Bug Fix Validation)
add_test_binary(
    test_backprop_activation_reconstruction
    ${CMAKE_CURRENT_SOURCE_DIR}/test_backprop_activation_reconstruction.cpp
    unit
)

# Test 5: Learning Rules with Sparse Synapse Storage (Bug Fix Validation)
add_test_binary(
    test_learning_sparse_synapse
    ${CMAKE_CURRENT_SOURCE_DIR}/test_learning_sparse_synapse.cpp
    unit
)

# Test 6: Struct Consistency (canary for stale struct copies)
add_test_binary(
    test_struct_consistency
    ${CMAKE_CURRENT_SOURCE_DIR}/test_struct_consistency.cpp
    unit
)

# Test 7: Synapse Hot/Cold Split (NIMCP 2.11)
add_test_binary(
    test_synapse_hot_cold
    ${CMAKE_CURRENT_SOURCE_DIR}/test_synapse_hot_cold.cpp
    unit
)

# Test 8: Synapse Lifecycle Manager (pruning/growth/GC)
add_test_binary(
    test_synapse_lifecycle
    ${CMAKE_CURRENT_SOURCE_DIR}/test_synapse_lifecycle.cpp
    unit
)
