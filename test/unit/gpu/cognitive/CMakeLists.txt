# ============================================================================
# GPU Cognitive Unit Tests CMakeLists.txt
# ============================================================================
# WHAT: Unit tests for GPU-accelerated cognitive operations
# WHY:  Verify GPU cognitive kernels work correctly in isolation
# HOW:  Link against nimcp and GTest, test individual GPU cognitive functions
# ============================================================================

# Guard: CUDA toolkit required for GPU cognitive tests
if(NOT CUDAToolkit_FOUND)
    message(STATUS "  Skipping GPU cognitive unit tests (CUDA toolkit not found)")
    return()
endif()

message(STATUS "  Building GPU cognitive unit tests (CUDA toolkit found)")

# Helper macro for adding GPU cognitive unit tests
macro(add_gpu_cognitive_test TEST_NAME)
    add_executable(unit_gpu_cognitive_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_gpu_cognitive_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    target_compile_features(unit_gpu_cognitive_${TEST_NAME} PRIVATE cxx_std_20)
    target_compile_definitions(unit_gpu_cognitive_${TEST_NAME} PRIVATE NIMCP_TESTING)
    add_test(NAME unit_gpu_cognitive_${TEST_NAME}
             COMMAND unit_gpu_cognitive_${TEST_NAME})
    set_tests_properties(unit_gpu_cognitive_${TEST_NAME}
        PROPERTIES
            LABELS "unit;gpu;cognitive"
            TIMEOUT 120
    )
endmacro()

# =============================================================================
# Broca GPU Unit Tests
# =============================================================================
# WHAT: Unit tests for GPU-accelerated Broca's region operations
# WHY:  Verify GPU lexical lookup, phoneme encoding, motor command generation
# HOW:  Test individual kernel functions with GPU/CPU equivalence verification
add_gpu_cognitive_test(test_broca_gpu)

# =============================================================================
# Wernicke GPU Unit Tests
# =============================================================================
# WHAT: Unit tests for GPU-accelerated Wernicke's region operations
# WHY:  Verify GPU phoneme recognition, word recognition, semantic activation
# HOW:  Test individual kernel functions with GPU/CPU equivalence verification
add_gpu_cognitive_test(test_wernicke_gpu)

# =============================================================================
# JEPA GPU Unit Tests
# =============================================================================
# WHAT: Unit tests for GPU-accelerated JEPA predictor operations
# WHY:  Verify GPU forward prediction, inverse model, masking, loss computation
# HOW:  Test individual kernel functions with numerical stability checks
# DISABLED: Test needs API updates - nimcp_gpu_tensor_upload/download/get_dims not declared
# TODO: Update test to use current GPU tensor API (nimcp_gpu_tensor_to_host, etc.)
# add_gpu_cognitive_test(test_jepa_gpu)

# =============================================================================
# Omni GPU Unit Tests
# =============================================================================
# WHAT: Unit tests for GPU-accelerated Omnidirectional inference operations
# WHY:  Verify GPU bidirectional prediction, Hopfield, hierarchy, replay
# HOW:  Test individual kernel functions with multi-component integration
# DISABLED: Test needs API updates - nimcp_gpu_tensor_upload/download not declared
# TODO: Update test to use current GPU tensor API (nimcp_gpu_tensor_to_host, etc.)
# add_gpu_cognitive_test(test_omni_gpu)

# =============================================================================
# Occipital GPU Bridge Unit Tests
# =============================================================================
# WHAT: Unit tests for GPU-accelerated Occipital cortex bridge operations
# WHY:  Verify GPU V1-V5 visual processing, saliency computation
# HOW:  Test individual kernel functions with CPU fallback verification
# DISABLED: Test needs API updates - visual_input_t missing stride field,
#           visual_processing_result_t missing num_features field
# TODO: Update test to use current visual processing API
# add_gpu_cognitive_test(test_occipital_gpu_bridge)
