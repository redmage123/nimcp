# Regression tests for plasticity learning stability

add_executable(test_learning_stability
    test_learning_stability.cpp
)

target_include_directories(test_learning_stability
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_learning_stability
    PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)

add_test(NAME LearningStabilityRegression
    COMMAND test_learning_stability
)

# Mark as regression test (slowest, most thorough)
set_tests_properties(LearningStabilityRegression PROPERTIES
    TIMEOUT 600
    LABELS "regression;plasticity;stability"
)

# Second messenger cascade regression tests
add_executable(regression_plasticity_second_messengers_test
    second_messengers/test_second_messengers_regression.cpp
)
target_link_libraries(regression_plasticity_second_messengers_test
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(regression_plasticity_second_messengers_test PRIVATE cxx_std_17)
gtest_discover_tests(regression_plasticity_second_messengers_test
    PROPERTIES
        TIMEOUT 600
        LABELS "regression;plasticity;second_messengers"
)

# ============================================================================
# Metabolic Plasticity Regression Tests
# ============================================================================
# WHAT: Regression tests for metabolic plasticity stability
# WHY:  Ensure energy dynamics remain stable across long-term operation
# HOW:  Test repeated cycles, consistency, bounds, transitions
# ============================================================================

add_executable(regression_plasticity_metabolic
    metabolic/test_metabolic_regression.cpp
)
target_link_libraries(regression_plasticity_metabolic
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(regression_plasticity_metabolic PRIVATE cxx_std_17)
gtest_discover_tests(regression_plasticity_metabolic
    PROPERTIES
        TIMEOUT 600
        LABELS "regression;plasticity;metabolic"
)

# Second messenger module integration regression tests
add_executable(regression_plasticity_second_messengers_module_test
    second_messengers/test_second_messengers_module_regression.cpp
)
target_link_libraries(regression_plasticity_second_messengers_module_test
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(regression_plasticity_second_messengers_module_test PRIVATE cxx_std_17)
gtest_discover_tests(regression_plasticity_second_messengers_module_test
    PROPERTIES
        TIMEOUT 600
        LABELS "regression;plasticity;second_messengers;modules"
)

# ============================================================================
# Metaplasticity Regression Tests
# ============================================================================
# WHAT: Regression tests for metaplasticity stability
# WHY:  Ensure threshold dynamics remain stable across long-term simulations
# HOW:  Test convergence, bounds, oscillation absence, multi-synapse consistency
# ============================================================================

add_executable(regression_plasticity_metaplasticity
    metaplasticity/test_metaplasticity_regression.cpp
)
target_link_libraries(regression_plasticity_metaplasticity
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(regression_plasticity_metaplasticity PRIVATE cxx_std_17)
gtest_discover_tests(regression_plasticity_metaplasticity
    PROPERTIES
        TIMEOUT 600
        LABELS "regression;plasticity;metaplasticity"
)

# Positional encoding regression tests for attention
add_executable(regression_plasticity_attention_pe_test
    attention/test_attention_pe_regression.cpp
)
target_link_libraries(regression_plasticity_attention_pe_test
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(regression_plasticity_attention_pe_test PRIVATE cxx_std_17)
gtest_discover_tests(regression_plasticity_attention_pe_test
    PROPERTIES
        TIMEOUT 600
        LABELS "regression;plasticity;attention;positional_encoding"
)

# Attention regression tests (backwards compatibility)
add_executable(regression_plasticity_attention_test
    attention/test_attention_regression.cpp
)
target_link_libraries(regression_plasticity_attention_test
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(regression_plasticity_attention_test PRIVATE cxx_std_17)
gtest_discover_tests(regression_plasticity_attention_test
    PROPERTIES
        TIMEOUT 600
        LABELS "regression;plasticity;attention"
        RESOURCE_LOCK "brain_heavy"
)

# Attention deferred callback buffer overflow regression tests
add_executable(regression_plasticity_attention_deferred_test
    attention/test_attention_deferred_regression.cpp
)
target_link_libraries(regression_plasticity_attention_deferred_test
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(regression_plasticity_attention_deferred_test PRIVATE cxx_std_17)
gtest_discover_tests(regression_plasticity_attention_deferred_test
    PROPERTIES
        TIMEOUT 300
        LABELS "regression;plasticity;attention;deferred_callbacks"
)

# Heterosynaptic plasticity subdirectory
add_subdirectory(heterosynaptic)

# Calcium dynamics subdirectory
add_subdirectory(calcium)

# ============================================================================
# Protein Synthesis Regression Tests
# ============================================================================
# WHAT: Regression tests for protein synthesis stability
# WHY:  Ensure PRP pool dynamics remain stable across long-term operation
# HOW:  Test repeated cycles, tag expiration consistency, consolidation repeatability
# ============================================================================

add_executable(regression_plasticity_protein
    protein/test_protein_regression.cpp
)
target_link_libraries(regression_plasticity_protein
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(regression_plasticity_protein PRIVATE cxx_std_17)
gtest_discover_tests(regression_plasticity_protein
    PROPERTIES
        TIMEOUT 600
        LABELS "regression;plasticity;protein"
)


# Triplet STDP Regression Tests
add_executable(regression_plasticity_triplet_stdp
    stdp/test_triplet_stdp_regression.cpp
)
target_link_libraries(regression_plasticity_triplet_stdp
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(regression_plasticity_triplet_stdp PRIVATE cxx_std_17)
gtest_discover_tests(regression_plasticity_triplet_stdp
    PROPERTIES TIMEOUT 300 LABELS "regression;plasticity;stdp;triplet")

# ============================================================================
# New Plasticity Modules Comprehensive Regression Tests
# ============================================================================
# WHAT: Comprehensive regression tests for all new plasticity modules
# WHY:  Ensure parameter stability, numerical stability, thread safety across
#       triplet STDP, calcium, structural, metabolic, protein, metaplasticity,
#       astrocyte, and heterosynaptic plasticity modules
# HOW:  Test biological parameter values, weight bounds, decay constants,
#       callback consistency, thread safety, memory leaks, numerical stability
# ============================================================================

add_executable(regression_plasticity_modules_test
    test_plasticity_regression.cpp
)
target_link_libraries(regression_plasticity_modules_test
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(regression_plasticity_modules_test PRIVATE cxx_std_17)
gtest_discover_tests(regression_plasticity_modules_test
    PROPERTIES
        TIMEOUT 600
        LABELS "regression;plasticity;triplet_stdp;calcium;structural;metabolic;protein;metaplasticity;astrocyte;heterosynaptic"
)

# ============================================================================
# Plasticity Orchestrator Regression Tests
# ============================================================================
# WHAT: Regression tests for orchestrator stability
# WHY:  Ensure orchestrator coordinates modules stably across long simulations
# HOW:  Test config stability, weight bounds, numerical stability, thread safety,
#       memory leaks, module enable/disable consistency
# ============================================================================

add_executable(regression_plasticity_orchestrator
    orchestrator/test_orchestrator_regression.cpp
)
target_link_libraries(regression_plasticity_orchestrator
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(regression_plasticity_orchestrator PRIVATE cxx_std_17)
gtest_discover_tests(regression_plasticity_orchestrator
    PROPERTIES
        TIMEOUT 600
        LABELS "regression;plasticity;orchestrator"
)

# ============================================================================
# Neural Plasticity Regression Tests
# ============================================================================
# WHAT: Regression tests for neural-plasticity bridges and coordinator
# WHY:  Ensure system remains stable across extended simulations
# HOW:  Test parameter bounds, numerical stability, weight convergence
# ============================================================================

add_executable(regression_plasticity_neural
    orchestrator/test_neural_regression.cpp
)
target_link_libraries(regression_plasticity_neural
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(regression_plasticity_neural PRIVATE cxx_std_17)
gtest_discover_tests(regression_plasticity_neural
    PROPERTIES
        TIMEOUT 600
        LABELS "regression;plasticity;neural;orchestrator"
)

# ============================================================================
# Pink Noise Enhancement Regression Tests
# ============================================================================
# WHAT: Regression tests for 9 pink noise enhancement modules
# WHY:  Ensure numerical stability, parameter bounds, and reproducibility
# HOW:  Extended runs, edge cases, stress tests, statistical validation
# ============================================================================

add_executable(regression_plasticity_pink_noise
    noise/test_pink_noise_regression.cpp
)
target_link_libraries(regression_plasticity_pink_noise
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(regression_plasticity_pink_noise PRIVATE cxx_std_17)
gtest_discover_tests(regression_plasticity_pink_noise
    PROPERTIES
        TIMEOUT 600
        LABELS "regression;plasticity;noise;pink"
)

# ============================================================================
# Pink Noise + Fractal Regression Tests
# ============================================================================
# WHAT: Regression tests for Pink Noise and Fractal modules
# WHY:  Ensure backward compatibility and stable behavior
# HOW:  Test API stability, output ranges, performance baselines
# ============================================================================

add_executable(regression_plasticity_pink_noise_fractal
    noise/test_pink_noise_fractal_regression.cpp
)
target_link_libraries(regression_plasticity_pink_noise_fractal
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(regression_plasticity_pink_noise_fractal PRIVATE cxx_std_17)
gtest_discover_tests(regression_plasticity_pink_noise_fractal
    PROPERTIES
        TIMEOUT 600
        LABELS "regression;plasticity;noise;pink;fractal"
)

# ============================================================================
# Phase 1 Bridges Regression Tests
# ============================================================================
# WHAT: Regression tests for Phase 1 bridges (dendrite, synapse, axon, glial-immune)
# WHY:  Ensure numerical stability, parameter bounds, no regressions
# HOW:  Extended simulations, stress tests, boundary conditions
# ============================================================================

add_executable(regression_plasticity_phase1_bridges
    bridges/test_phase1_bridges_regression.cpp
)
target_link_libraries(regression_plasticity_phase1_bridges
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(regression_plasticity_phase1_bridges PRIVATE cxx_std_17)
gtest_discover_tests(regression_plasticity_phase1_bridges
    PROPERTIES
        TIMEOUT 600
        LABELS "regression;plasticity;bridges;phase1"
)

# ============================================================================
# Eligibility Utils-Quantum Bridge Regression Tests
# ============================================================================
# WHAT: Regression tests for bidirectional eligibility utils-quantum bridge
# WHY:  Ensure stability, edge case handling, no numerical regressions
# HOW:  Edge cases, high load, rapid cycles, memory safety, state bounds
# ============================================================================

add_executable(regression_plasticity_eligibility_utils_quantum
    eligibility/test_eligibility_utils_quantum_regression.cpp
)
target_link_libraries(regression_plasticity_eligibility_utils_quantum
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(regression_plasticity_eligibility_utils_quantum PRIVATE cxx_std_17)
gtest_discover_tests(regression_plasticity_eligibility_utils_quantum
    PROPERTIES
        TIMEOUT 600
        LABELS "regression;plasticity;eligibility;utils;quantum"
)

# ============================================================================
# Attention Pool Init Regression Tests
# ============================================================================
# WHAT: Regression tests for attention pool CAS-based initialization
# WHY:  Verify fix for double-checked locking race condition does not regress
# HOW:  Concurrent init, consistent pool, rapid cycles, denormal edge cases
# ============================================================================

add_executable(regression_plasticity_attention_pool_init
    attention/test_attention_pool_init_regression.cpp
)
target_link_libraries(regression_plasticity_attention_pool_init
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(regression_plasticity_attention_pool_init PRIVATE cxx_std_17)
gtest_discover_tests(regression_plasticity_attention_pool_init
    PROPERTIES
        TIMEOUT 600
        LABELS "regression;plasticity;attention;pool;thread_safety"
)

# ============================================================================
# Denormal Threshold Regression Tests
# ============================================================================
# WHAT: Regression tests for denormal threshold standardization
# WHY:  Ensure BCM 1e-9->1e-10 change does not affect learning dynamics
# HOW:  Extended STDP/BCM/triplet simulations, convergence, performance
# ============================================================================

add_executable(regression_plasticity_denormal_threshold
    test_denormal_threshold_regression.cpp
)
target_link_libraries(regression_plasticity_denormal_threshold
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(regression_plasticity_denormal_threshold PRIVATE cxx_std_17)
gtest_discover_tests(regression_plasticity_denormal_threshold
    PROPERTIES
        TIMEOUT 600
        LABELS "regression;plasticity;denormal;threshold"
)

# ============================================================================
# Exception Handling Regression Tests
# ============================================================================
add_subdirectory(exception)
