# Unit tests for plasticity modules with bio-async integration

# Attention bio-async tests
add_executable(test_attention_bioasync
    attention/test_attention_bioasync.cpp
)
target_link_libraries(test_attention_bioasync
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(test_attention_bioasync PRIVATE cxx_std_20)
gtest_discover_tests(test_attention_bioasync
    PROPERTIES LABELS "unit;plasticity;attention")

# Dendritic bio-async tests
add_executable(test_dendritic_bioasync
    dendritic/test_dendritic_bioasync.cpp
)
target_link_libraries(test_dendritic_bioasync
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(test_dendritic_bioasync PRIVATE cxx_std_20)
gtest_discover_tests(test_dendritic_bioasync
    PROPERTIES LABELS "unit;plasticity;dendritic")

# Adaptive bio-async tests
add_executable(test_adaptive_bioasync
    adaptive/test_adaptive_bioasync.cpp
)
target_link_libraries(test_adaptive_bioasync
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(test_adaptive_bioasync PRIVATE cxx_std_20)
gtest_discover_tests(test_adaptive_bioasync
    PROPERTIES LABELS "unit;plasticity;adaptive")

# Predictive coding bio-async tests
add_executable(test_predictive_bioasync
    predictive/test_predictive_bioasync.cpp
)
target_link_libraries(test_predictive_bioasync
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(test_predictive_bioasync PRIVATE cxx_std_20)
gtest_discover_tests(test_predictive_bioasync
    PROPERTIES LABELS "unit;plasticity;predictive")

# Second messenger cascade tests
add_executable(unit_plasticity_second_messengers_test
    second_messengers/test_second_messengers.cpp
)
target_link_libraries(unit_plasticity_second_messengers_test
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_plasticity_second_messengers_test PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_second_messengers_test
    PROPERTIES LABELS "unit;plasticity;second_messengers")

# Second messenger module integration unit tests
add_executable(unit_plasticity_second_messengers_modules_test
    second_messengers/test_second_messengers_modules.cpp
)
target_link_libraries(unit_plasticity_second_messengers_modules_test
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_plasticity_second_messengers_modules_test PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_second_messengers_modules_test
    PROPERTIES LABELS "unit;plasticity;second_messengers;modules")

# Neuromodulator-immune integration tests
add_executable(unit_plasticity_neuromodulator_immune
    immune/test_neuromodulator_immune.cpp
)
target_link_libraries(unit_plasticity_neuromodulator_immune
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_plasticity_neuromodulator_immune PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_neuromodulator_immune
    PROPERTIES LABELS "unit;plasticity;immune;neuromodulator")

# ============================================================================
# Plasticity FEP Bridge Tests
# ============================================================================
# WHAT: Unit tests for Plasticity-FEP bidirectional integration bridges
# WHY:  Ensure synaptic plasticity integrates with FEP precision and predictions
# HOW:  Test lifecycle, effects, and bio-async for each bridge type
# ============================================================================

add_executable(unit_plasticity_test_fep_bridges test_plasticity_fep_bridges.cpp)
target_link_libraries(unit_plasticity_test_fep_bridges
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_plasticity_test_fep_bridges PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_test_fep_bridges
    PROPERTIES LABELS "unit;plasticity;fep;bridge"
    TIMEOUT 60
)

# ============================================================================
# Plasticity Coordinator Tests
# ============================================================================
# WHAT: Unit tests for Plasticity Coordinator module
# WHY:  Central coordinator managing 8 plasticity mechanisms with conflict resolution
# HOW:  Test lifecycle, registration, updates, state management, conflict resolution
# ============================================================================

add_executable(unit_plasticity_test_coordinator test_plasticity_coordinator.cpp)
target_link_libraries(unit_plasticity_test_coordinator
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_plasticity_test_coordinator PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_test_coordinator
    PROPERTIES LABELS "unit;plasticity;coordinator"
    TIMEOUT 60
)

# ============================================================================
# Plasticity Coordinator No-Deadlock Tests (Bug H3)
# ============================================================================
# WHAT: Tests for callback-under-mutex fix in plasticity coordinator
# WHY:  Verify callbacks are invoked outside the lock to prevent deadlock
# HOW:  Register re-entrant callbacks, concurrent updates, lifecycle tests
# ============================================================================

add_executable(unit_plasticity_test_coordinator_no_deadlock test_plasticity_coordinator_no_deadlock.cpp)
target_link_libraries(unit_plasticity_test_coordinator_no_deadlock
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_plasticity_test_coordinator_no_deadlock PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_test_coordinator_no_deadlock
    PROPERTIES LABELS "unit;plasticity;coordinator;deadlock"
    TIMEOUT 30
)

# Attention subdirectory (PE tests)
add_subdirectory(attention)

# ============================================================================
# Sleep Bridge Integration Tests
# ============================================================================
# WHAT: Unit tests for Sleep-Plasticity integration bridges
# WHY:  Ensure plasticity mechanisms correctly modulate based on sleep state
# HOW:  Test helper functions, default config, biological validity
# ============================================================================

# Neuromodulators Sleep Bridge Tests
add_executable(unit_plasticity_neuromodulators_sleep_bridge
    neuromodulators/sleep/test_neuromodulators_sleep_bridge.cpp
)
target_link_libraries(unit_plasticity_neuromodulators_sleep_bridge
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_plasticity_neuromodulators_sleep_bridge PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_neuromodulators_sleep_bridge
    PROPERTIES LABELS "unit;plasticity;neuromodulators;sleep")

# STDP Sleep Bridge Tests
add_executable(unit_plasticity_stdp_sleep_bridge
    stdp/sleep/test_stdp_sleep_bridge.cpp
)
target_link_libraries(unit_plasticity_stdp_sleep_bridge
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_plasticity_stdp_sleep_bridge PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_stdp_sleep_bridge
    PROPERTIES LABELS "unit;plasticity;stdp;sleep")

# Triplet STDP Tests
add_executable(unit_plasticity_triplet_stdp
    stdp/test_triplet_stdp.cpp
)
target_link_libraries(unit_plasticity_triplet_stdp
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_plasticity_triplet_stdp PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_triplet_stdp
    PROPERTIES LABELS "unit;plasticity;stdp;triplet")

# Quantum STDP Optimizer Tests
add_executable(unit_plasticity_stdp_quantum_optimizer
    stdp/test_quantum_stdp_optimizer.cpp
)
target_link_libraries(unit_plasticity_stdp_quantum_optimizer
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_plasticity_stdp_quantum_optimizer PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_stdp_quantum_optimizer
    PROPERTIES LABELS "unit;plasticity;stdp;quantum")

# STDP Utils Bridge Tests
add_executable(unit_plasticity_stdp_utils_bridge
    stdp/test_stdp_utils_bridge.cpp
)
target_link_libraries(unit_plasticity_stdp_utils_bridge
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_plasticity_stdp_utils_bridge PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_stdp_utils_bridge
    PROPERTIES LABELS "unit;plasticity;stdp;utils;bridge"
    TIMEOUT 60)

# STDP State Ops Tests (Phase 8: System-Wide Health Integration)
add_executable(unit_plasticity_stdp_state_ops
    stdp/test_stdp_state_ops.cpp
)
target_link_libraries(unit_plasticity_stdp_state_ops
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_plasticity_stdp_state_ops PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_stdp_state_ops
    PROPERTIES LABELS "unit;plasticity;stdp;state;fault_tolerance"
    TIMEOUT 120)

# BCM Sleep Bridge Tests
add_executable(unit_plasticity_bcm_sleep_bridge
    bcm/sleep/test_bcm_sleep_bridge.cpp
)
target_link_libraries(unit_plasticity_bcm_sleep_bridge
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_plasticity_bcm_sleep_bridge PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_bcm_sleep_bridge
    PROPERTIES LABELS "unit;plasticity;bcm;sleep")

# Homeostatic Sleep Bridge Tests
add_executable(unit_plasticity_homeostatic_sleep_bridge
    homeostatic/sleep/test_homeostatic_sleep_bridge.cpp
)
target_link_libraries(unit_plasticity_homeostatic_sleep_bridge
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_plasticity_homeostatic_sleep_bridge PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_homeostatic_sleep_bridge
    PROPERTIES LABELS "unit;plasticity;homeostatic;sleep")

# Heterosynaptic plasticity subdirectory
add_subdirectory(heterosynaptic)

# Calcium dynamics subdirectory
add_subdirectory(calcium)

# Substrate bridge subdirectory
add_subdirectory(substrate)

# ============================================================================
# Protein Synthesis Tests
# ============================================================================
# WHAT: Unit tests for Protein Synthesis Constraints System
# WHY:  Test Frey & Morris synaptic tagging and capture model
# HOW:  Test PRP pool, tag management, consolidation, sleep/immune bridges
# ============================================================================

add_executable(unit_plasticity_protein_synthesis
    protein/test_protein_synthesis.cpp
)
target_link_libraries(unit_plasticity_protein_synthesis
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_plasticity_protein_synthesis PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_protein_synthesis
    PROPERTIES LABELS "unit;plasticity;protein"
    TIMEOUT 60
)

# ============================================================================
# Metabolic Plasticity Tests
# ============================================================================
# WHAT: Unit tests for metabolic plasticity and energy constraints
# WHY:  Ensure ATP-based gating and energy dynamics work correctly
# HOW:  Test lifecycle, consumption, recovery, gating, statistics
# ============================================================================

add_executable(unit_plasticity_metabolic
    metabolic/test_metabolic_plasticity.cpp
)
target_link_libraries(unit_plasticity_metabolic
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_plasticity_metabolic PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_metabolic
    PROPERTIES LABELS "unit;plasticity;metabolic"
    TIMEOUT 60
)

# ============================================================================
# Structural Plasticity Tests
# ============================================================================
# WHAT: Unit tests for structural plasticity spine dynamics
# WHY:  Test spine formation, stabilization, potentiation, and pruning
# HOW:  Test lifecycle, morphology, state transitions, sleep/immune integration
# ============================================================================

add_executable(unit_plasticity_structural
    structural/test_structural_plasticity.cpp
)
target_link_libraries(unit_plasticity_structural
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_plasticity_structural PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_structural
    PROPERTIES LABELS "unit;plasticity;structural"
    TIMEOUT 60
)

# ============================================================================
# Extended Metaplasticity Tests
# ============================================================================
# WHAT: Unit tests for extended metaplasticity module
# WHY:  Test multi-timescale threshold adaptation (Abraham & Bear 1996)
# HOW:  Test lifecycle, baseline updates, neuromodulator shifts, sleep resets, controller
# ============================================================================

add_executable(unit_plasticity_extended_metaplasticity
    metaplasticity/test_extended_metaplasticity.cpp
)
target_link_libraries(unit_plasticity_extended_metaplasticity
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_plasticity_extended_metaplasticity PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_extended_metaplasticity
    PROPERTIES LABELS "unit;plasticity;metaplasticity"
    TIMEOUT 60
)

# ============================================================================
# Astrocyte Plasticity Tests
# ============================================================================
# WHAT: Unit tests for astrocyte plasticity and gliotransmitter modulation
# WHY:  Test glial-synaptic plasticity interactions
# HOW:  Test lifecycle, calcium waves, gliotransmitter release, modulation
# ============================================================================

add_executable(unit_plasticity_astrocyte
    astrocyte/test_astrocyte_plasticity.cpp
)
target_link_libraries(unit_plasticity_astrocyte
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_plasticity_astrocyte PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_astrocyte
    PROPERTIES LABELS "unit;plasticity;astrocyte"
    TIMEOUT 60
)

# ============================================================================
# Plasticity Orchestrator Tests
# ============================================================================
# WHAT: Comprehensive unit tests for the unified plasticity orchestrator
# WHY:  Test the central coordinator for all NIMCP plasticity mechanisms
# HOW:  Test lifecycle, synapse/neuron management, updates, callbacks, statistics
# ============================================================================

add_executable(unit_plasticity_orchestrator
    orchestrator/test_plasticity_orchestrator.cpp
)
target_link_libraries(unit_plasticity_orchestrator
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_plasticity_orchestrator PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_orchestrator
    PROPERTIES LABELS "unit;plasticity;orchestrator"
    TIMEOUT 120
)

# ============================================================================
# Axon-Orchestrator Bridge Tests
# ============================================================================
# WHAT: Unit tests for axon-orchestrator bridge
# WHY:  Test spike arrival forwarding to plasticity orchestrator
# HOW:  Test lifecycle, mapping, spike forwarding, delay compensation
# ============================================================================

add_executable(unit_plasticity_axon_orchestrator_bridge
    orchestrator/test_axon_orchestrator_bridge.cpp
)
target_link_libraries(unit_plasticity_axon_orchestrator_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_plasticity_axon_orchestrator_bridge PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_axon_orchestrator_bridge
    PROPERTIES LABELS "unit;plasticity;orchestrator;axon"
    TIMEOUT 120
)

# ============================================================================
# Neuron-Orchestrator Bridge Tests
# ============================================================================
# WHAT: Unit tests for neuron-orchestrator bridge
# WHY:  Test spike cascade (post_spike + axon + bAP)
# HOW:  Test lifecycle, neuron registration, step, firing rate
# ============================================================================

add_executable(unit_plasticity_neuron_orchestrator_bridge
    orchestrator/test_neuron_orchestrator_bridge.cpp
)
target_link_libraries(unit_plasticity_neuron_orchestrator_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_plasticity_neuron_orchestrator_bridge PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_neuron_orchestrator_bridge
    PROPERTIES LABELS "unit;plasticity;orchestrator;neuron"
    TIMEOUT 120
)

# ============================================================================
# Dendrite-Orchestrator Bridge Tests
# ============================================================================
# WHAT: Unit tests for dendrite-orchestrator bridge
# WHY:  Test bidirectional spine-synapse synchronization
# HOW:  Test lifecycle, mapping, sync, spine formation/elimination
# ============================================================================

add_executable(unit_plasticity_dendrite_orchestrator_bridge
    orchestrator/test_dendrite_orchestrator_bridge.cpp
)
target_link_libraries(unit_plasticity_dendrite_orchestrator_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_plasticity_dendrite_orchestrator_bridge PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_dendrite_orchestrator_bridge
    PROPERTIES LABELS "unit;plasticity;orchestrator;dendrite"
    TIMEOUT 120
)

# ============================================================================
# Neural Plasticity Coordinator Tests
# ============================================================================
# WHAT: Unit tests for unified neural-plasticity coordinator
# WHY:  Test integrated neural computation and plasticity
# HOW:  Test lifecycle, registration, simulation, rewards, stats
# ============================================================================

add_executable(unit_plasticity_neural_coordinator
    orchestrator/test_neural_plasticity_coordinator.cpp
)
target_link_libraries(unit_plasticity_neural_coordinator
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_plasticity_neural_coordinator PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_neural_coordinator
    PROPERTIES LABELS "unit;plasticity;orchestrator;coordinator"
    TIMEOUT 180
)

# ============================================================================
# Pink Noise Enhancement Tests
# ============================================================================
# WHAT: Unit tests for 9 pink noise enhancement modules
# WHY:  Ensure comprehensive coverage of biologically-inspired noise features
# HOW:  Test lifecycle, generation, modulation, statistics for each module
# ============================================================================

add_executable(unit_plasticity_pink_noise_enhancements
    noise/test_pink_noise_enhancements.cpp
)
target_link_libraries(unit_plasticity_pink_noise_enhancements
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_plasticity_pink_noise_enhancements PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_pink_noise_enhancements
    PROPERTIES LABELS "unit;plasticity;noise;pink"
    TIMEOUT 120
)

# ============================================================================
# Pink Noise Exception Handling Tests
# ============================================================================
# WHAT: Unit tests for NIMCP_THROW_TO_IMMUNE exception handling in Pink Noise
# WHY:  Ensure errors are properly reported to the brain immune system
# HOW:  Test all NULL pointer and invalid parameter paths
# ============================================================================

add_executable(unit_plasticity_pink_noise_exception_handling
    noise/test_pink_noise_exception_handling.cpp
)
target_link_libraries(unit_plasticity_pink_noise_exception_handling
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_plasticity_pink_noise_exception_handling PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_pink_noise_exception_handling
    PROPERTIES LABELS "unit;plasticity;noise;pink;exception"
    TIMEOUT 60
)

# ============================================================================
# Plasticity Bridge Tests (Phase 1)
# ============================================================================
# WHAT: Unit tests for neural component-plasticity bridges
# WHY:  Ensure bridges correctly connect dendrites, synapses, axons to plasticity
# HOW:  Test lifecycle, API functions, integration, statistics
# ============================================================================

# Dendrite-Plasticity Bridge Tests
add_executable(unit_plasticity_dendrite_bridge
    bridges/test_dendrite_plasticity_bridge.cpp
)
target_link_libraries(unit_plasticity_dendrite_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_plasticity_dendrite_bridge PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_dendrite_bridge
    PROPERTIES LABELS "unit;plasticity;bridges;dendrite"
    TIMEOUT 120
)

# Synapse-Plasticity Bridge Tests
add_executable(unit_plasticity_synapse_bridge
    bridges/test_synapse_plasticity_bridge.cpp
)
target_link_libraries(unit_plasticity_synapse_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_plasticity_synapse_bridge PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_synapse_bridge
    PROPERTIES LABELS "unit;plasticity;bridges;synapse"
    TIMEOUT 120
)

# Axon-Plasticity Bridge Tests
add_executable(unit_plasticity_axon_bridge
    bridges/test_axon_plasticity_bridge.cpp
)
target_link_libraries(unit_plasticity_axon_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_plasticity_axon_bridge PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_axon_bridge
    PROPERTIES LABELS "unit;plasticity;bridges;axon"
    TIMEOUT 120
)

# Eligibility Quantum Bridge Tests
add_executable(unit_plasticity_eligibility_quantum_bridge
    eligibility/test_eligibility_quantum_bridge.cpp
)
target_link_libraries(unit_plasticity_eligibility_quantum_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_plasticity_eligibility_quantum_bridge PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_eligibility_quantum_bridge
    PROPERTIES LABELS "unit;plasticity;eligibility;quantum;bridge"
    TIMEOUT 120
)

# Eligibility Utils-Quantum Bidirectional Bridge Tests
add_executable(unit_plasticity_eligibility_utils_quantum_bridge
    eligibility/test_eligibility_utils_quantum_bridge.cpp
)
target_link_libraries(unit_plasticity_eligibility_utils_quantum_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_plasticity_eligibility_utils_quantum_bridge PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_eligibility_utils_quantum_bridge
    PROPERTIES LABELS "unit;plasticity;eligibility;utils;quantum;bridge"
    TIMEOUT 120
)

# ============================================================================
# Denormal Threshold Consistency Tests
# ============================================================================
# WHAT: Unit tests for standardized NIMCP_DENORMAL_THRESHOLD across plasticity
# WHY:  Verify all learning rules (STDP, BCM, triplet STDP, calcium, attention)
#       use the same denormal threshold constant from nimcp_plasticity_constants.h
# HOW:  Test threshold value, trace flushing behavior, cross-module consistency
# ============================================================================

add_executable(unit_plasticity_denormal_threshold
    test_denormal_threshold_consistency.cpp
)
target_link_libraries(unit_plasticity_denormal_threshold
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_plasticity_denormal_threshold PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_denormal_threshold
    PROPERTIES
        TIMEOUT 120
        LABELS "unit;plasticity;denormal;threshold;consistency"
)

# ============================================================================
# Exception Handling Tests
# ============================================================================
# WHAT: Unit tests for plasticity exception handling
# WHY:  Verify error-to-exception mapping and handler chain for all plasticity modules
# HOW:  Include exception subdirectory
# ============================================================================

add_subdirectory(exception)

# ============================================================================
# Hybrid Learning Tests
# ============================================================================
# WHAT: Unit tests for LEARN_MODE_HYBRID and lateral inhibition
# WHY:  Verify supervised+biological plasticity integration and WTA competition
# HOW:  Test loss convergence, bio-plasticity activation, lateral inhibition
# ============================================================================

add_executable(unit_plasticity_hybrid_learning
    adaptive/test_hybrid_learning.cpp
)
target_link_libraries(unit_plasticity_hybrid_learning
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_plasticity_hybrid_learning PRIVATE cxx_std_20)
gtest_discover_tests(unit_plasticity_hybrid_learning
    PROPERTIES LABELS "unit;plasticity;adaptive;hybrid"
    TIMEOUT 60
)

# ============================================================================
# Backprop Gradient Clipping Tests (C, no GTest)
# ============================================================================
# WHAT: Unit tests for gradient norm clipping in backprop_sparse_full()
# WHY:  Ensure max_grad_norm parameter correctly bounds gradient magnitudes
# HOW:  Direct C tests with NULL/invalid params and functional network tests
# ============================================================================

add_executable(test_backprop_grad_clip
    test_backprop_grad_clip.c
)
target_link_libraries(test_backprop_grad_clip
    PRIVATE nimcp pthread m
)
target_include_directories(test_backprop_grad_clip PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
add_test(NAME test_backprop_grad_clip COMMAND test_backprop_grad_clip)
set_tests_properties(test_backprop_grad_clip PROPERTIES
    LABELS "unit;plasticity;adaptive;backprop;grad_clip"
    TIMEOUT 60
)

# ============================================================================
# EMA isfinite() Guard Tests
# WHAT: Tests that EMA calculations recover from NaN/Inf inputs
# WHY:  A single NaN/Inf permanently corrupts an EMA without isfinite guards
# HOW:  Feed NaN/Inf into NIMCP_EMA_GUARD macros and nimcp_safe_expf, verify recovery
# ============================================================================

add_executable(test_ema_isfinite_guards
    test_ema_isfinite_guards.c
)
target_link_libraries(test_ema_isfinite_guards
    PRIVATE m
)
target_include_directories(test_ema_isfinite_guards PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
add_test(NAME test_ema_isfinite_guards COMMAND test_ema_isfinite_guards)
set_tests_properties(test_ema_isfinite_guards PROPERTIES
    LABELS "unit;plasticity;math;ema;isfinite"
    TIMEOUT 30
)
