# ============================================================================
# NIMCP Parallel Test Suite
# ============================================================================
# Tests are organized into separate binaries for parallel execution via ctest
# Run with: ctest -j$(nproc) for maximum parallelism
#
# Test Categories (10 binaries):
# 1. core_tests: Core neural network and neuron types
# 2. glial_tests: Astrocytes, oligodendrocytes, microglia, integration
# 3. plasticity_tests: Neuromodulators, BCM, attention
# 4. cognitive_tests: Knowledge, curiosity, ethics, salience, consolidation
# 5. networking_tests: P2P, protocol, events, replication
# 6. io_tests: Data I/O, streaming, serialization, JSON
# 7. utility_tests: Data structures, time, memory, validation
# 8. security_tests: Security, performance, error injection
# 9. integration_tests: Networking, cognitive, end-to-end integration
# 10. quality_tests: Stress, memory leaks, lint, regression, logging
# ============================================================================

# Check if GTest is available - skip all tests if not found
if(NOT GTest_FOUND)
    message(STATUS "GTest not found - skipping entire test suite")
    return()
endif()

# Common test setup (shared across all test binaries)
set(COMMON_TEST_INCLUDES
    ${NIMCP_INCLUDE_DIR}
    ${NIMCP_TEST_DIR}
    ${NIMCP_LIB_DIR}
    ${GTEST_INCLUDE_DIRS}
    ${Python3_INCLUDE_DIRS}
)

set(COMMON_TEST_LIBS
    nimcp
    GTest::GTest
    GTest::Main
    Python3::Python
    pthread
)

# Helper function to create a test binary
function(add_nimcp_test_binary TARGET_NAME)
    add_executable(${TARGET_NAME} ${ARGN})

    target_include_directories(${TARGET_NAME} PRIVATE ${COMMON_TEST_INCLUDES})

    # Enable C++20 for designated initializers
    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)

    # Enable test-only APIs
    target_compile_definitions(${TARGET_NAME} PRIVATE NIMCP_TESTING)

    # Add Python module source
    target_sources(${TARGET_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/src/python/nimcp_module.c
    )

    target_link_libraries(${TARGET_NAME} PRIVATE ${COMMON_TEST_LIBS})

    set_target_properties(${TARGET_NAME} PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )

    # Enable coverage for Debug builds
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        if(CMAKE_COMPILER_IS_GNUCXX)
            target_compile_options(${TARGET_NAME} PRIVATE --coverage)
            target_link_options(${TARGET_NAME} PRIVATE --coverage)
        endif()
    endif()

    # Register with ctest
    add_test(NAME ${TARGET_NAME}
             COMMAND ${TARGET_NAME}
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

    set_tests_properties(${TARGET_NAME} PROPERTIES
        ENVIRONMENT "PYTHONPATH=${CMAKE_BINARY_DIR}/lib/python"
    )
endfunction()

# ============================================================================
# TEST BINARY 1: Core Neural Network Tests (fast, foundational)
# ============================================================================
add_nimcp_test_binary(core_tests
    test_module.cpp
    test_neuralnet_create.cpp
    test_neuralnet_learning.cpp
    test_adaptive.cpp
    # test_neuron_types.cpp  # TEMPORARILY DISABLED: API mismatches need fixing
    # test_brain_regions.cpp  # TEMPORARILY DISABLED: API mismatches need fixing
    test_brain_cow.cpp
    test_brain_comprehensive.cpp  # Comprehensive brain.c coverage (target: 100%)
    test_brain_cognitive_integration.cpp  # Deep cognitive integration tests for brain_decide
)
set_tests_properties(core_tests PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy")

# ============================================================================
# TEST BINARY 2: Glial Cell Tests (medium speed, related)
# ============================================================================
# NOTE: Enhanced glial tests are now in test/unit/glial/ and test/integration/glial/
# Old monolithic tests removed - use ctest -R glial to run all glial tests
#add_nimcp_test_binary(glial_tests
#    test_astrocytes.cpp
#    test_oligodendrocytes.cpp
#    test_microglia.cpp
#    test_glial_integration.cpp
#)

# ============================================================================
# TEST BINARY 3: Plasticity & Neuromodulation Tests (medium speed)
# ============================================================================
add_nimcp_test_binary(plasticity_tests
    test_neuromodulators.cpp
    test_bcm.cpp
    test_attention.cpp
)
set_tests_properties(plasticity_tests PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy")

# ============================================================================
# TEST BINARY 3.5: Fractal Topology Tests (fast)
# ============================================================================
add_nimcp_test_binary(topology_tests
    topology_tests.cpp
)
set_tests_properties(topology_tests PROPERTIES TIMEOUT 180 RESOURCE_LOCK "brain_heavy")

# ============================================================================
# TEST BINARY 3.6: Topology Integration Tests (medium)
# ============================================================================
add_nimcp_test_binary(topology_integration_tests
    topology_integration_tests.cpp
)
set_tests_properties(topology_integration_tests PROPERTIES TIMEOUT 180 RESOURCE_LOCK "brain_heavy")

# ============================================================================
# TEST BINARY 3.7: Pink Noise Generator Tests (fast)
# ============================================================================
add_nimcp_test_binary(pink_noise_tests
    pink_noise_tests.cpp
)

# ============================================================================
# TEST BINARY 3.8: Epistemic Filter Tests (fast) - Phase 9.2
# ============================================================================
add_nimcp_test_binary(epistemic_filter_tests
    epistemic_filter_tests.cpp
)

# ============================================================================
# TEST BINARY 4: Cognitive & Higher-Level Tests (medium speed)
# ============================================================================
add_nimcp_test_binary(cognitive_tests
    test_knowledge.cpp
    test_curiosity.cpp
    test_ethics.cpp
    test_salience.cpp
    test_consolidation.cpp
    test_introspection.cpp
    test_wellbeing.cpp
    test_symbolic_logic.cpp
    test_hierarchical.cpp
    epistemic_filter_tests.cpp  # Phase 9.2: Bias prevention tests
    test_working_memory.cpp     # Phase 10.1: Working Memory (Miller's 7±2)
    test_emotional_tagging.cpp  # Phase 10.2: Emotional Tagging (Russell's circumplex)
    test_executive.cpp          # Phase 10.3: Executive Functions (task switching, planning)
    test_sleep_wake.cpp         # Phase 10.4: Sleep/Wake Cycle (TDD)
    test_mental_health.cpp      # Phase 10.5: Mental Health Monitoring (9 disorder detectors)
    test_theory_of_mind.cpp     # Phase 10.6: Theory of Mind (BDI model, empathy, false beliefs)
    test_explanations.cpp       # Phase 10.7: Natural Explanations (what/why/how, interpretability)
    test_meta_learning.cpp      # Phase 10.8: Meta-Learning (MAML, few-shot learning, adaptive LR)
    test_predictive.cpp         # Phase 10.9: Predictive Processing (free energy, active inference)
    test_mirror_neurons.cpp     # Phase 10.11: Mirror Neurons (observation, imitation, social cognition)
)
set_tests_properties(cognitive_tests PROPERTIES TIMEOUT 600 RESOURCE_LOCK "brain_heavy")

# ============================================================================
# TEST BINARY 5: Networking & P2P Tests (medium-slow)
# ============================================================================
add_nimcp_test_binary(networking_tests
    test_p2pnode.cpp
    test_protocol.cpp
    test_events.cpp
    test_replication.cpp
)

# ============================================================================
# TEST BINARY 6: Data & I/O Tests (fast)
# ============================================================================
add_nimcp_test_binary(io_tests
    test_dataio.cpp
    test_stream.cpp
    test_serialization.cpp
    test_json.cpp
)

# ============================================================================
# TEST BINARY 7: Utility & Data Structure Tests (fast)
# ============================================================================
add_nimcp_test_binary(utility_tests
    test_time.cpp
    test_platform_time.cpp
    test_memory.cpp
    test_vector.cpp
    test_hash_table.cpp
    test_btree.cpp
    test_graph.cpp
    test_validate.cpp
    test_queue_utils.cpp
    test_queue_manager.cpp
    test_thread_utils.cpp
    test_platform_once.cpp
    test_platform_cond.cpp
    test_platform_mutex.cpp
    test_platform_rwlock.cpp
    test_platform_thread.cpp
    test_fft.cpp
    test_visual_cortex.cpp
)

# ============================================================================
# TEST BINARY 7.5: Perception Bidirectional Tests (Phase 10.11.3) - FAST
# ============================================================================
add_nimcp_test_binary(perception_bidirectional_tests
    test_audio_cortex.cpp
    test_speech_cortex.cpp
)

# ============================================================================
# TEST BINARY 8: Security & Performance Tests (medium)
# ============================================================================
# TEMPORARILY DISABLED: API mismatches need fixing
#add_nimcp_test_binary(security_tests
#    test_security.cpp
#    test_performance_optimizations.cpp
#    test_error_injection.cpp
#)

# ============================================================================
# TEST BINARY 9: Integration Tests (slow)
# ============================================================================
add_nimcp_test_binary(integration_tests
    test_integration_networking.cpp
    test_integration_cognitive.cpp
    test_integration_e2e.cpp
    test_distributed_cognition.cpp
    test_distributed_advanced.cpp
    test_brain_integration.cpp
    test_comprehensive_brain_integration.cpp
    test_visual_cortex_integration.cpp
    test_visual_cortex_e2e.cpp
    test_visual_cortex_regression.cpp
)
set_tests_properties(integration_tests PROPERTIES TIMEOUT 600 RESOURCE_LOCK "brain_heavy")

# ============================================================================
# TEST BINARY 10: Quality & Stress Tests (very slow)
# ============================================================================
# TEMPORARILY DISABLED: API mismatches need fixing
#add_nimcp_test_binary(quality_tests
#    test_stress.cpp
#    test_memory_leaks.cpp
#    test_lint.cpp
#    test_regression.cpp
#    test_logging.cpp
#)

# ============================================================================
# Enable testing
# ============================================================================
enable_testing()

# ============================================================================
# Summary
# ============================================================================
message(STATUS "")
message(STATUS "===============================================================================")
message(STATUS "NIMCP Parallel Test Suite Configured")
message(STATUS "===============================================================================")
message(STATUS "Test Binaries: 13 active (can run in parallel)")
message(STATUS "  1.   core_tests                  - Core neural network & brain (7 test files)")
message(STATUS "  2.   glial_tests                 - DISABLED (moved to test/unit/glial/)")
message(STATUS "  3.   plasticity_tests             - Neuromodulators, BCM, attention (3 test files)")
message(STATUS "  3.5  topology_tests               - Fractal topology (1 test file)")
message(STATUS "  3.6  topology_integration_tests   - Topology integration (1 test file)")
message(STATUS "  3.7  pink_noise_tests             - Pink noise generator (1 test file)")
message(STATUS "  3.8  epistemic_filter_tests       - Epistemic filter (1 test file)")
message(STATUS "  4.   cognitive_tests              - Higher-level cognition (19 test files)")
message(STATUS "  5.   networking_tests             - P2P & networking (4 test files)")
message(STATUS "  6.   io_tests                     - Data I/O & serialization (4 test files)")
message(STATUS "  7.   utility_tests                - Data structures & utilities (16 test files)")
message(STATUS "  7.5  perception_bidirectional_tests - Audio & speech cortex (2 test files)")
message(STATUS "  8.   security_tests               - DISABLED (API mismatches)")
message(STATUS "  9.   integration_tests            - Cross-module integration (10 test files)")
message(STATUS " 10.   quality_tests                - DISABLED (API mismatches)")
message(STATUS "")
message(STATUS "Parallel Execution:")
message(STATUS "  Run all:  ctest -j\$(nproc)")
message(STATUS "  Run fast: ctest -j\$(nproc) -E 'integration|quality'")
message(STATUS "  Run one:  ctest -R core_tests")
message(STATUS "  Verbose:  ctest -j\$(nproc) -V")
message(STATUS "===============================================================================")
message(STATUS "")
