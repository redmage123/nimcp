# ============================================================================
# GPU Integration Tests CMakeLists.txt
# ============================================================================
# WHAT: Integration tests for GPU/CPU equivalence, backend switching, precision
# WHY:  Verify GPU operations match CPU results and backend system works correctly
# HOW:  Link against nimcp and GTest, run tests for kernel equivalence
# ============================================================================

# Helper macro for adding GPU integration tests
macro(add_gpu_integration_test TEST_NAME)
    add_executable(integration_gpu_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(integration_gpu_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    target_compile_features(integration_gpu_${TEST_NAME} PRIVATE cxx_std_20)
    target_compile_definitions(integration_gpu_${TEST_NAME} PRIVATE NIMCP_TESTING)
    add_test(NAME integration_gpu_${TEST_NAME}
             COMMAND integration_gpu_${TEST_NAME})
    set_tests_properties(integration_gpu_${TEST_NAME}
        PROPERTIES LABELS "integration;gpu")
endmacro()

# =============================================================================
# CUDA Integration Tests - only built when CUDA toolkit is available
# =============================================================================
if(CUDAToolkit_FOUND)
    message(STATUS "  Building CUDA integration tests (CUDA toolkit found)")

    # =============================================================================
    # GPU/CPU Equivalence Tests
    # =============================================================================
    # WHAT: Verify GPU and CPU kernel implementations produce equivalent results
    # WHY:  Ensure computation correctness regardless of backend selection
    # HOW:  Run identical operations on both backends, compare outputs within tolerance
    add_gpu_integration_test(test_gpu_cpu_equivalence)

    # =============================================================================
    # Kernel Backend Switching Tests
    # =============================================================================
    # WHAT: Test backend initialization, switching, and operation dispatch
    # WHY:  Ensure seamless transitions between CPU and GPU compute backends
    # HOW:  Test backend lifecycle, switching, and verify operations work after switch
    add_gpu_integration_test(test_kernel_backend_switching)

    # =============================================================================
    # Mixed Precision Tests
    # =============================================================================
    # WHAT: Test FP32/FP16/INT8 precision conversions and quantization
    # WHY:  Mixed precision enables faster inference with acceptable accuracy
    # HOW:  Test precision conversions, quantization, and calibration functions
    add_gpu_integration_test(test_mixed_precision)

    # =============================================================================
    # Cognitive GPU Tests
    # =============================================================================
    # WHAT: Test GPU-accelerated cognitive operations (FEP, attention, working memory)
    # WHY:  Verify cognitive modules work correctly on both GPU and CPU backends
    # HOW:  Compare GPU and CPU results within tolerance, test fallback behavior
    add_gpu_integration_test(test_cognitive_gpu)

    # =============================================================================
    # Logic GPU Tests
    # =============================================================================
    # WHAT: Test GPU-accelerated neural logic operations (gates, neuromodulation)
    # WHY:  Verify neural logic gates work correctly on both GPU and CPU backends
    # HOW:  Batch evaluation, neuromodulation effects, expression evaluation
    add_gpu_integration_test(test_logic_gpu)

    # =============================================================================
    # Broca GPU Integration Tests
    # =============================================================================
    # WHAT: Test Broca's region works correctly with GPU-enabled brain
    # WHY:  Verify GPU acceleration doesn't interfere with language production
    # HOW:  Create brain with both GPU and Broca enabled, run production tests
    add_gpu_integration_test(test_broca_gpu_integration)

    # =============================================================================
    # Broca GPU Kernel Integration Tests
    # =============================================================================
    # WHAT: Test GPU Broca kernels with other GPU systems and full pipelines
    # WHY:  Verify GPU Broca works correctly with GPU context, large batches, etc.
    # HOW:  Test shared contexts, large batches, GPU/CPU equivalence, stress tests
    add_gpu_integration_test(test_broca_gpu_kernel_integration)

    # =============================================================================
    # Substrate GPU Integration Tests
    # =============================================================================
    # WHAT: Test GPU neural substrate integration with brain factory and kernel backend
    # WHY:  Verify substrate GPU operations work correctly in full system context
    # HOW:  Test brain creation, kernel dispatch, subsystem initialization and stepping
    add_gpu_integration_test(test_substrate_gpu_integration)

    # =============================================================================
    # Visual Processing GPU Integration Tests
    # =============================================================================
    # WHAT: Test GPU visual processing (V1-V5, saliency, occipital bridge)
    # WHY:  Verify GPU visual kernels produce correct results matching CPU reference
    # HOW:  Run identical operations on both backends, compare outputs within tolerance
    add_gpu_integration_test(test_visual_processing_gpu)

    # =============================================================================
    # Timeout Configuration for CUDA tests
    # =============================================================================
    # GPU tests may take longer on systems without GPU acceleration
    set_tests_properties(
        integration_gpu_test_gpu_cpu_equivalence
        integration_gpu_test_mixed_precision
        integration_gpu_test_cognitive_gpu
        integration_gpu_test_logic_gpu
        integration_gpu_test_broca_gpu_integration
        integration_gpu_test_broca_gpu_kernel_integration
        integration_gpu_test_substrate_gpu_integration
        integration_gpu_test_visual_processing_gpu
        PROPERTIES TIMEOUT 180
    )

    set_tests_properties(
        integration_gpu_test_kernel_backend_switching
        PROPERTIES TIMEOUT 90
    )
else()
    message(STATUS "  Skipping CUDA integration tests (CUDA toolkit not found)")
endif()

# =============================================================================
# GPU Inference Optimization Integration Tests
# =============================================================================
# WHAT: Integration tests for GPU inference pipeline (freeze, batch, determinism)
# WHY:  Verify GPU inference features work end-to-end with brain API
# HOW:  Create brains, train, freeze, predict — no CUDA required (GTEST_SKIP)
add_gpu_integration_test(test_gpu_inference_integration)
set_tests_properties(integration_gpu_test_gpu_inference_integration
    PROPERTIES
        TIMEOUT 600
        RESOURCE_LOCK "brain_heavy"
)

# =============================================================================
# Existing GPU Integration Tests
# =============================================================================
# These tests were already present in the directory
add_gpu_integration_test(test_gpu_integration)
add_gpu_integration_test(test_gpu_execution_mode_integration)
add_gpu_integration_test(test_gpu_neuron_integration)
add_gpu_integration_test(test_gpu_spike_event_integration)

# =============================================================================
# Subdirectory Integration Tests
# =============================================================================
# Include exception GPU integration tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/exception/CMakeLists.txt)
    add_subdirectory(exception)
endif()

# Include fuzzy GPU integration tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/fuzzy/CMakeLists.txt)
    add_subdirectory(fuzzy)
endif()

# Include financial GPU integration tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/financial/CMakeLists.txt)
    add_subdirectory(financial)
endif()

# Include statistics GPU integration tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/statistics/CMakeLists.txt)
    add_subdirectory(statistics)
endif()

# Include recovery GPU integration tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/recovery/CMakeLists.txt)
    add_subdirectory(recovery)
endif()

# Include cognitive GPU integration tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cognitive/CMakeLists.txt)
    add_subdirectory(cognitive)
endif()

# Include perception GPU integration tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/perception/CMakeLists.txt)
    add_subdirectory(perception)
endif()

# Include inference GPU integration tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/inference/CMakeLists.txt)
    add_subdirectory(inference)
endif()

# Include specialized GPU integration tests (dragonfly, portia, sparse, ternary, topology)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/specialized/CMakeLists.txt)
    add_subdirectory(specialized)
endif()

# Include neural GPU integration tests (SNN, LNN, CNN recovery)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/neural/CMakeLists.txt)
    add_subdirectory(neural)
endif()

# Include swarm GPU integration tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/swarm/CMakeLists.txt)
    add_subdirectory(swarm)
endif()

# Include hypothalamus GPU integration tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/hypothalamus/CMakeLists.txt)
    add_subdirectory(hypothalamus)
endif()

# Include quantum GPU recovery integration tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/quantum/CMakeLists.txt)
    add_subdirectory(quantum)
endif()

# Include graph GPU recovery integration tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/graph/CMakeLists.txt)
    add_subdirectory(graph)
endif()

# Include training GPU integration tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/training/CMakeLists.txt)
    add_subdirectory(training)
endif()
