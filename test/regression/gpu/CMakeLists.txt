# ============================================================================
# GPU Regression Tests CMakeLists.txt
# ============================================================================
# WHAT: Regression tests for GPU operations
# WHY:  Prevent reintroduction of fixed GPU-related bugs
# HOW:  Test specific bug scenarios and backward compatibility
# ============================================================================

# Helper macro for adding GPU regression tests
macro(add_gpu_regression_test TEST_NAME)
    add_executable(regression_gpu_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(regression_gpu_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    target_compile_features(regression_gpu_${TEST_NAME} PRIVATE cxx_std_20)
    target_compile_definitions(regression_gpu_${TEST_NAME} PRIVATE NIMCP_TESTING)
    add_test(NAME regression_gpu_${TEST_NAME}
             COMMAND regression_gpu_${TEST_NAME})
    set_tests_properties(regression_gpu_${TEST_NAME}
        PROPERTIES
            LABELS "regression;gpu"
            TIMEOUT 120
    )
endmacro()

# =============================================================================
# GPU Stub Regression Tests (no CUDA required)
# =============================================================================
# WHAT: Regression tests for GPU stub behavior on CPU-only systems
# WHY:  Prevent reintroduction of false positive throws and ensure
#        consistent error handling when CUDA is not available
# HOW:  Test stub return values, backend fallback, and error types
add_gpu_regression_test(test_gpu_stub_regression)

# =============================================================================
# GPU Recovery Validation Regression Tests (no CUDA required)
# =============================================================================
# WHAT: Regression tests for GPU recovery callback validation
# WHY:  Prevent reintroduction of unchecked recovery results
# HOW:  Test recovery config, context lifecycle, parameter correction
add_gpu_regression_test(test_gpu_recovery_validation_regression)

# =============================================================================
# GPU Visual Processing Regression Tests (no CUDA required)
# =============================================================================
# WHAT: Regression tests for GPU visual processing (V1-V5, saliency)
# WHY:  Prevent reintroduction of visual processing bugs (numerical, API)
# HOW:  Test struct layouts, function signatures, edge cases, memory safety
# NOTE: Test has CPU fallback via has_gpu_ flag — runs on CPU-only systems
add_gpu_regression_test(test_visual_processing_regression)

# =============================================================================
# GPU Inference Optimization Regression Tests (no CUDA required)
# =============================================================================
# WHAT: Regression tests for GPU inference features (freeze, dirty flag, stability)
# WHY:  Prevent regressions in freeze/unfreeze, weight upload, inference stability
# HOW:  Test frozen vs unfrozen equivalence, repeated stability, train-freeze cycles
add_gpu_regression_test(test_gpu_inference_regression)
set_tests_properties(regression_gpu_test_gpu_inference_regression
    PROPERTIES RESOURCE_LOCK "brain_heavy"
)

# Guard: CUDA toolkit required for remaining GPU regression tests
if(NOT CUDAToolkit_FOUND)
    message(STATUS "  Skipping CUDA GPU regression tests (CUDA toolkit not found)")

    # Include recovery subdirectory (may have non-CUDA tests)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/recovery/CMakeLists.txt)
        add_subdirectory(recovery)
    endif()
    return()
endif()

message(STATUS "  Building GPU regression tests (CUDA toolkit found)")

# =============================================================================
# GPU Neuron Regression Tests
# =============================================================================
add_gpu_regression_test(test_gpu_neuron_backward_compat)
add_gpu_regression_test(test_gpu_neuron_regression)

# =============================================================================
# Multi-GPU Regression Tests
# =============================================================================
add_gpu_regression_test(test_multigpu_regression)

# P2-13: Device ID bounds overflow regression test
add_gpu_regression_test(test_gpu_bounds_regression)

# =============================================================================
# GPU Spike Event Regression Tests
# =============================================================================
add_gpu_regression_test(test_gpu_spike_event_backward_compat)
set_tests_properties(regression_gpu_test_gpu_spike_event_backward_compat PROPERTIES
    RESOURCE_LOCK "brain_heavy"
)

# =============================================================================
# GPU Execution Mode Regression Tests
# =============================================================================
add_gpu_regression_test(test_gpu_execution_mode_backward_compat)

# =============================================================================
# GPU Broca Regression Tests
# =============================================================================
# WHAT: Regression tests for GPU-accelerated Broca's region
# WHY:  Prevent reintroduction of GPU Broca bugs (alignment, overflow, etc.)
# HOW:  Test specific bug scenarios and verify fixes remain stable
add_gpu_regression_test(test_broca_gpu_regression)

# =============================================================================
# GPU Substrate Regression Tests
# =============================================================================
# WHAT: Regression tests for GPU-accelerated neural substrate
# WHY:  Prevent reintroduction of substrate GPU bugs (numerical stability, memory)
# HOW:  Test numerical stability, GPU/CPU equivalence, edge cases
add_gpu_regression_test(test_substrate_gpu_regression)

# =============================================================================
# GPU Walkthrough Fixes Regression Tests
# =============================================================================
# WHAT: Regression tests for GPU P1/P2/P3 walkthrough fixes
# WHY:  Prevent reintroduction of division-by-zero, OOB, block size,
#        condition ordering, and stub behavior bugs
# HOW:  Test specific bug scenarios (P1-12 through P3) and verify fixes
add_gpu_regression_test(test_gpu_walkthrough_fixes)

# =============================================================================
# GPU Pass-3 Fixes Regression Tests
# =============================================================================
# WHAT: Regression tests for GPU/Tensor pass-3 fixes (P1-M/T/U, P2-S/T/U, P3-T)
# WHY:  Prevent reintroduction of div-by-zero guards, dtype dispatch,
#        overflow detection, realloc safety, and error message fixes
# HOW:  Test specific bug scenarios and verify fixes remain stable
add_gpu_regression_test(test_gpu_pass3_fixes)

# Include broca subdirectory if it exists
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/broca/CMakeLists.txt)
    add_subdirectory(broca)
endif()

# Include fuzzy subdirectory if it exists (controls auto-discovery)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/fuzzy/CMakeLists.txt)
    add_subdirectory(fuzzy)
endif()

# =============================================================================
# GPU Fuzzy Regression Tests
# =============================================================================
# WHAT: Regression tests for GPU-accelerated fuzzy logic operations
# WHY:  Prevent reintroduction of fuzzy GPU bugs (numerical, overflow, etc.)
# HOW:  Test specific bug scenarios, numerical stability, edge cases
# DISABLED: Test includes missing header gpu/nimcp_gpu_context.h
# TODO: Fix header path to gpu/context/nimcp_gpu_context.h
# if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_fuzzy_gpu_regression.cpp)
#     add_gpu_regression_test(test_fuzzy_gpu_regression)
# endif()

# =============================================================================
# GPU Financial Regression Tests
# =============================================================================
# WHAT: Regression tests for GPU-accelerated financial operations
# WHY:  Prevent reintroduction of financial GPU bugs (Monte Carlo, options, etc.)
# HOW:  Test specific bug scenarios, numerical stability, edge cases
# DISABLED: Test has extern "C" linkage error - CUDA templates conflict with C linkage
# TODO: Fix extern "C" block in test_financial_gpu_regression.cpp line 17
# if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_financial_gpu_regression.cpp)
#     add_gpu_regression_test(test_financial_gpu_regression)
# endif()

# Include statistics GPU regression tests subdirectory
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/statistics/CMakeLists.txt)
    add_subdirectory(statistics)
endif()

# Include recovery GPU regression tests subdirectory
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/recovery/CMakeLists.txt)
    add_subdirectory(recovery)
endif()

# Include financial GPU regression tests subdirectory
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/financial/CMakeLists.txt)
    add_subdirectory(financial)
endif()
