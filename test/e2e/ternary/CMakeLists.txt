# ============================================================================
# NIMCP Ternary End-to-End Tests
# ============================================================================
# WHAT: Build configuration for ternary representation e2e tests
# WHY:  Verify complete ternary pipelines across NIMCP subsystems
# HOW:  Link full NIMCP library with GTest for comprehensive testing
#
# TEST COVERAGE:
# - Ternary Neural Network Training (full forward/backward with quantization)
# - Ternary LNN Simulation (temporal dynamics with ternary connectivity)
# - Ternary Cortical Processing (sparse coding with ternary representations)
# - Ternary Attention System (selective attention with ternary gates)
# - Ternary Swarm Consensus (Byzantine consensus with ternary voting)
# - Ternary Mixed Precision Training (gradient accumulation with ternary forward)
# ============================================================================

# Guard: Check GTest availability
if(NOT GTest_FOUND)
    message(STATUS "GTest not found - skipping Ternary E2E tests")
    return()
endif()

message(STATUS "")
message(STATUS "===============================================================================")
message(STATUS "NIMCP Ternary E2E Test Suite")
message(STATUS "===============================================================================")

# ============================================================================
# Common Configuration
# ============================================================================

set(TERNARY_E2E_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/utils/ternary
    ${CMAKE_SOURCE_DIR}/include/snn
    ${CMAKE_SOURCE_DIR}/include/lnn
    ${CMAKE_SOURCE_DIR}/include/swarm
    ${CMAKE_SOURCE_DIR}/include/plasticity
    ${CMAKE_SOURCE_DIR}/test/e2e
    ${GTEST_INCLUDE_DIRS}
)

set(TERNARY_E2E_LIBRARIES
    nimcp
    nimcp_snn
    nimcp_lnn
    GTest::GTest
    GTest::Main
    Threads::Threads
    m
)

# ============================================================================
# Helper: Create Ternary E2E Test Binary
# ============================================================================
function(add_ternary_e2e_test TARGET_NAME SOURCE_FILE)
    # Check source exists
    if(NOT EXISTS ${SOURCE_FILE})
        message(WARNING "Ternary E2E test source not found: ${SOURCE_FILE}")
        return()
    endif()

    # Create executable
    add_executable(${TARGET_NAME} ${SOURCE_FILE})

    # Add dependency on nimcp library
    add_dependencies(${TARGET_NAME} nimcp)

    # Configure includes
    target_include_directories(${TARGET_NAME} PRIVATE ${TERNARY_E2E_INCLUDE_DIRS})

    # Enable C++20 for structured bindings, designated initializers
    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)

    # Enable test-only APIs
    target_compile_definitions(${TARGET_NAME} PRIVATE
        NIMCP_TESTING
        NIMCP_E2E_TESTING
    )

    # Link libraries
    target_link_libraries(${TARGET_NAME} PRIVATE ${TERNARY_E2E_LIBRARIES})

    # Set properties
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

    # Register with CTest
    add_test(NAME ${TARGET_NAME}
             COMMAND ${TARGET_NAME}
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

    # Set E2E test properties (longer timeout, sequential execution)
    set_tests_properties(${TARGET_NAME} PROPERTIES
        LABELS "e2e;ternary"
        TIMEOUT 180  # 3 minutes for ternary E2E tests
        RUN_SERIAL TRUE  # Run sequentially to avoid resource contention
    )

    message(STATUS "  Added Ternary E2E test: ${TARGET_NAME}")
endfunction()

# ============================================================================
# Ternary E2E Test Targets
# ============================================================================

# 1. Ternary Neural Network Training
# Tests: full forward/backward pass, convergence with quantization, memory efficiency
add_ternary_e2e_test(e2e_test_ternary_neural_network
    ${CMAKE_CURRENT_SOURCE_DIR}/test_e2e_ternary_neural_network.cpp
)

# 2. Ternary LNN Simulation
# Tests: temporal dynamics, biological plausibility, ODE integration stability
add_ternary_e2e_test(e2e_test_ternary_lnn_simulation
    ${CMAKE_CURRENT_SOURCE_DIR}/test_e2e_ternary_lnn_simulation.cpp
)

# 3. Ternary Cortical Processing
# Tests: sparse coding, sparsity targets, lateral inhibition, hierarchical processing
add_ternary_e2e_test(e2e_test_ternary_cortical_processing
    ${CMAKE_CURRENT_SOURCE_DIR}/test_e2e_ternary_cortical_processing.cpp
)

# 4. Ternary Attention System
# Tests: selective attention, attention dynamics, inhibition of return
add_ternary_e2e_test(e2e_test_ternary_attention_system
    ${CMAKE_CURRENT_SOURCE_DIR}/test_e2e_ternary_attention_system.cpp
)

# 5. Ternary Swarm Consensus
# Tests: Byzantine consensus, ternary voting, swarm convergence
add_ternary_e2e_test(e2e_test_ternary_swarm_consensus
    ${CMAKE_CURRENT_SOURCE_DIR}/test_e2e_ternary_swarm_consensus.cpp
)

# 6. Ternary Mixed Precision Training
# Tests: gradient accumulation, training stability, precision conversion
add_ternary_e2e_test(e2e_test_ternary_mixed_precision_training
    ${CMAKE_CURRENT_SOURCE_DIR}/test_e2e_ternary_mixed_precision_training.cpp
)

# ============================================================================
# Combined Ternary E2E Test Target
# ============================================================================
# WHAT: Convenience target to build all ternary e2e tests at once
# WHY:  Simplify build and CI integration
# HOW:  Custom target that depends on all ternary e2e test executables
# ============================================================================

add_custom_target(ternary_e2e_tests
    DEPENDS
        e2e_test_ternary_neural_network
        e2e_test_ternary_lnn_simulation
        e2e_test_ternary_cortical_processing
        e2e_test_ternary_attention_system
        e2e_test_ternary_swarm_consensus
        e2e_test_ternary_mixed_precision_training
    COMMENT "Building all Ternary E2E tests"
)

# ============================================================================
# Test Execution Instructions
# ============================================================================
message(STATUS "")
message(STATUS "Ternary E2E Test Execution:")
message(STATUS "  All ternary tests: ctest -L ternary")
message(STATUS "  Build all:         make ternary_e2e_tests")
message(STATUS "  Single test:       ./e2e_test_ternary_neural_network")
message(STATUS "  Verbose:           ctest -L ternary -V")
message(STATUS "")
message(STATUS "Test Coverage:")
message(STATUS "  - Neural Network:    Full training with ternary weights")
message(STATUS "  - LNN Simulation:    Temporal dynamics with ternary recurrence")
message(STATUS "  - Cortical:          Sparse coding with ternary representations")
message(STATUS "  - Attention:         Selective attention with ternary gates")
message(STATUS "  - Swarm Consensus:   Byzantine voting with ternary states")
message(STATUS "  - Mixed Precision:   Gradient accumulation with ternary forward")
message(STATUS "")
message(STATUS "===============================================================================")
message(STATUS "")
