# Integration tests for plasticity bio-async interactions

add_executable(test_plasticity_bioasync_integration
    test_plasticity_bioasync_integration.cpp
)

target_include_directories(test_plasticity_bioasync_integration
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_plasticity_bioasync_integration
    PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)

add_test(NAME PlasticityBioAsyncIntegration
    COMMAND test_plasticity_bioasync_integration
)

# Mark as integration test (slower)
set_tests_properties(PlasticityBioAsyncIntegration PROPERTIES
    TIMEOUT 300
    LABELS "integration;plasticity;bioasync"
)

# Second messenger cascade integration tests
add_executable(integration_plasticity_second_messengers_test
    second_messengers/test_second_messengers_integration.cpp
)
target_link_libraries(integration_plasticity_second_messengers_test
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(integration_plasticity_second_messengers_test PRIVATE cxx_std_17)
gtest_discover_tests(integration_plasticity_second_messengers_test
    PROPERTIES
        TIMEOUT 300
        LABELS "integration;plasticity;second_messengers"
)

# ============================================================================
# Metabolic Plasticity Integration Tests
# ============================================================================
# WHAT: Integration tests for metabolic plasticity with sleep and immune systems
# WHY:  Ensure bridge integrations work correctly across module boundaries
# HOW:  Test sleep-metabolic and immune-metabolic bidirectional interactions
# ============================================================================

add_executable(integration_plasticity_metabolic
    metabolic/test_metabolic_integration.cpp
)
target_link_libraries(integration_plasticity_metabolic
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(integration_plasticity_metabolic PRIVATE cxx_std_17)
gtest_discover_tests(integration_plasticity_metabolic
    PROPERTIES
        TIMEOUT 300
        LABELS "integration;plasticity;metabolic"
)

# Second messenger module integration tests (Broca, Audio, Speech, etc.)
add_executable(integration_plasticity_second_messengers_module_test
    second_messengers/test_second_messengers_module_integration.cpp
)
target_link_libraries(integration_plasticity_second_messengers_module_test
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(integration_plasticity_second_messengers_module_test PRIVATE cxx_std_17)
gtest_discover_tests(integration_plasticity_second_messengers_module_test
    PROPERTIES
        TIMEOUT 300
        LABELS "integration;plasticity;second_messengers;modules"
)

# Heterosynaptic plasticity subdirectory
add_subdirectory(heterosynaptic)

# Calcium dynamics subdirectory

# ============================================================================
# Metaplasticity Integration Tests
# ============================================================================
# WHAT: Integration tests for metaplasticity bridges
# WHY:  Ensure sleep and immune bridges work correctly
# HOW:  Test bidirectional interactions between metaplasticity and sleep/immune systems
# ============================================================================

add_executable(integration_plasticity_metaplasticity
    metaplasticity/test_metaplasticity_integration.cpp
)
target_link_libraries(integration_plasticity_metaplasticity
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(integration_plasticity_metaplasticity PRIVATE cxx_std_17)
gtest_discover_tests(integration_plasticity_metaplasticity
    PROPERTIES
        TIMEOUT 300
        LABELS "integration;plasticity;metaplasticity"
)
add_subdirectory(calcium)

# ============================================================================
# Protein Synthesis Integration Tests
# ============================================================================
# WHAT: Integration tests for protein synthesis with sleep and immune systems
# WHY:  Ensure bridge integrations work correctly across module boundaries
# HOW:  Test sleep-protein and immune-protein bidirectional interactions
# ============================================================================

add_executable(integration_plasticity_protein
    protein/test_protein_integration.cpp
)
target_link_libraries(integration_plasticity_protein
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(integration_plasticity_protein PRIVATE cxx_std_17)
gtest_discover_tests(integration_plasticity_protein
    PROPERTIES
        TIMEOUT 300
        LABELS "integration;plasticity;protein"
)


# Triplet STDP Integration Tests
add_executable(integration_plasticity_triplet_stdp
    stdp/test_triplet_stdp_integration.cpp
)
target_link_libraries(integration_plasticity_triplet_stdp
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(integration_plasticity_triplet_stdp PRIVATE cxx_std_17)
gtest_discover_tests(integration_plasticity_triplet_stdp
    PROPERTIES TIMEOUT 300 LABELS "integration;plasticity;stdp;triplet")

# ============================================================================
# Plasticity Orchestrator Integration Tests
# ============================================================================
# WHAT: Integration tests for plasticity orchestrator with sleep/immune systems
# WHY:  Ensure orchestrator coordinates modules correctly with external systems
# HOW:  Test sleep/immune modulation, multi-module updates, long-running simulations
# ============================================================================

add_executable(integration_plasticity_orchestrator
    orchestrator/test_orchestrator_integration.cpp
)
target_link_libraries(integration_plasticity_orchestrator
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(integration_plasticity_orchestrator PRIVATE cxx_std_17)
gtest_discover_tests(integration_plasticity_orchestrator
    PROPERTIES
        TIMEOUT 300
        LABELS "integration;plasticity;orchestrator"
)

# ============================================================================
# Neural Plasticity Integration Tests
# ============================================================================
# WHAT: Integration tests for neural-plasticity bridges and coordinator
# WHY:  Ensure complete spike-to-plasticity pipeline works correctly
# HOW:  Test multi-bridge coordination, spike cascade, bidirectional sync
# ============================================================================

add_executable(integration_plasticity_neural
    orchestrator/test_neural_integration.cpp
)
target_link_libraries(integration_plasticity_neural
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(integration_plasticity_neural PRIVATE cxx_std_17)
gtest_discover_tests(integration_plasticity_neural
    PROPERTIES
        TIMEOUT 300
        LABELS "integration;plasticity;neural;orchestrator"
)

# ============================================================================
# Pink Noise Enhancement Integration Tests
# ============================================================================
# WHAT: Integration tests for 9 pink noise enhancement modules
# WHY:  Ensure all modules work together correctly
# HOW:  Test cross-module interactions, state transitions, full pipeline
# ============================================================================

add_executable(integration_plasticity_pink_noise
    noise/test_pink_noise_integration.cpp
)
target_link_libraries(integration_plasticity_pink_noise
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(integration_plasticity_pink_noise PRIVATE cxx_std_17)
gtest_discover_tests(integration_plasticity_pink_noise
    PROPERTIES
        TIMEOUT 300
        LABELS "integration;plasticity;noise;pink"
)

# ============================================================================
# Pink Noise + Fractal Integration Tests
# ============================================================================
# WHAT: Integration tests for Pink Noise + Fractal Analysis modules
# WHY:  Verify that generated pink noise exhibits correct fractal properties
# HOW:  Generate pink noise, analyze with fractal methods, validate consistency
# ============================================================================

add_executable(integration_plasticity_pink_noise_fractal
    noise/test_pink_noise_fractal_integration.cpp
)
target_link_libraries(integration_plasticity_pink_noise_fractal
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(integration_plasticity_pink_noise_fractal PRIVATE cxx_std_17)
gtest_discover_tests(integration_plasticity_pink_noise_fractal
    PROPERTIES
        TIMEOUT 300
        LABELS "integration;plasticity;noise;pink;fractal"
)

# ============================================================================
# Phase 1 Bridges Integration Tests
# ============================================================================
# WHAT: Integration tests for Phase 1 bridges (dendrite, synapse, axon, glial-immune)
# WHY:  Ensure all 6 Phase 1 bridges work correctly together
# HOW:  Test cross-bridge interactions, cascading effects, stress scenarios
# ============================================================================

add_executable(integration_plasticity_phase1_bridges
    bridges/test_phase1_bridges_integration.cpp
)
target_link_libraries(integration_plasticity_phase1_bridges
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(integration_plasticity_phase1_bridges PRIVATE cxx_std_17)
gtest_discover_tests(integration_plasticity_phase1_bridges
    PROPERTIES
        TIMEOUT 300
        LABELS "integration;plasticity;bridges;phase1"
)

# ============================================================================
# Eligibility Utils-Quantum Bidirectional Bridge Integration Tests
# ============================================================================
# WHAT: Integration tests for bidirectional feedback between utils and quantum
# WHY:  Ensure closed-loop optimization improves learning dynamics
# HOW:  Test full pipeline, feedback convergence, multi-module coordination
# ============================================================================

add_executable(integration_plasticity_eligibility_utils_quantum
    eligibility/test_eligibility_utils_quantum_integration.cpp
)
target_link_libraries(integration_plasticity_eligibility_utils_quantum
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(integration_plasticity_eligibility_utils_quantum PRIVATE cxx_std_17)
gtest_discover_tests(integration_plasticity_eligibility_utils_quantum
    PROPERTIES
        TIMEOUT 300
        LABELS "integration;plasticity;eligibility;utils;quantum"
)

# ============================================================================
# Exception Handling Integration Tests
# ============================================================================
# WHAT: Integration tests for plasticity exception handling with immune system
# WHY:  Verify end-to-end exception flow from plasticity modules to immune system
# HOW:  Include exception subdirectory
# ============================================================================

add_subdirectory(exception)

# ============================================================================
# Bio-Async Plasticity Bridge Integration Tests
# ============================================================================
# WHAT: Integration tests for bio-async plasticity bridge
# WHY:  Validates spike timing messages carry STDP data across module boundaries

add_executable(test_plasticity_bridge_integration
    test_plasticity_bridge_integration.cpp
)

target_include_directories(test_plasticity_bridge_integration
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_plasticity_bridge_integration
    PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)

target_compile_features(test_plasticity_bridge_integration PRIVATE cxx_std_17)

add_test(NAME PlasticityBridgeIntegration
    COMMAND test_plasticity_bridge_integration
)

set_tests_properties(PlasticityBridgeIntegration PROPERTIES
    TIMEOUT 300
    LABELS "integration;plasticity;bridge;bio-async"
)
