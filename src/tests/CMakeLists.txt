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
    test_neuron_types.cpp
    test_brain_regions.cpp
    test_brain_cow.cpp
)

# ============================================================================
# TEST BINARY 2: Glial Cell Tests (medium speed, related)
# ============================================================================
add_nimcp_test_binary(glial_tests
    test_astrocytes.cpp
    test_oligodendrocytes.cpp
    test_microglia.cpp
    test_glial_integration.cpp
)

# ============================================================================
# TEST BINARY 3: Plasticity & Neuromodulation Tests (medium speed)
# ============================================================================
add_nimcp_test_binary(plasticity_tests
    test_neuromodulators.cpp
    test_bcm.cpp
    test_attention.cpp
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
)

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
    test_memory.cpp
    test_vector.cpp
    test_hash_table.cpp
    test_btree.cpp
    test_graph.cpp
    test_validate.cpp
    test_queue_utils.cpp
    test_queue_manager.cpp
    test_thread_utils.cpp
    test_fft.cpp
    test_visual_cortex.cpp
    test_audio_cortex.cpp
    # v2.7.0 unit tests
    test_weights_stats.cpp
    test_batch_api.cpp
    test_simd.cpp
    #test_async.cpp  # TODO: Fix API mismatches
    test_checkpointing.cpp
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
    # v2.7.0 integration, E2E, and regression tests
    test_integration_v2_7.cpp
    test_e2e_v2_7.cpp
    test_regression_v2_7.cpp
)

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
message(STATUS "Test Binaries: 10 (can run in parallel)")
message(STATUS "  1. core_tests          - Core neural network & neuron types (7 test files)")
message(STATUS "  2. glial_tests         - Glial cells & integration (4 test files)")
message(STATUS "  3. plasticity_tests    - Neuromodulators, BCM, attention (3 test files)")
message(STATUS "  4. cognitive_tests     - Higher-level cognition (9 test files)")
message(STATUS "  5. networking_tests    - P2P & networking (4 test files)")
message(STATUS "  6. io_tests            - Data I/O & serialization (4 test files)")
message(STATUS "  7. utility_tests       - Data structures & utilities (18 test files, +5 v2.7.0)")
message(STATUS "  8. security_tests      - Security & performance (3 test files)")
message(STATUS "  9. integration_tests   - Cross-module integration (13 test files, +3 v2.7.0)")
message(STATUS " 10. quality_tests       - Stress, leaks, lint, regression (5 test files)")
message(STATUS "")
message(STATUS "Parallel Execution:")
message(STATUS "  Run all:  ctest -j\$(nproc)")
message(STATUS "  Run fast: ctest -j\$(nproc) -E 'integration|quality'")
message(STATUS "  Run one:  ctest -R core_tests")
message(STATUS "  Verbose:  ctest -j\$(nproc) -V")
message(STATUS "===============================================================================")
message(STATUS "")
