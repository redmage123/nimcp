# ============================================================================
# SNN Unit Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for SNN unit tests
# WHY:  Verify correctness of spiking neural network components
# HOW:  Build GoogleTest executables for each SNN module
# ============================================================================

# Guard: Check GTest availability
if(NOT GTest_FOUND)
    message(STATUS "GTest not found - skipping SNN unit tests")
    return()
endif()

# Common link libraries for all SNN tests
set(SNN_TEST_LIBS
    nimcp_snn
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
    z
)

# Common include directories
set(SNN_TEST_INCLUDES
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/snn
)

# ============================================================================
# Test 1: SNN Configuration
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_config.cpp)
    add_executable(unit_snn_test_snn_config
        test_snn_config.cpp
    )

    target_link_libraries(unit_snn_test_snn_config
        PRIVATE ${SNN_TEST_LIBS}
    )

    target_include_directories(unit_snn_test_snn_config
        PRIVATE ${SNN_TEST_INCLUDES}
    )

    target_compile_features(unit_snn_test_snn_config PRIVATE cxx_std_17)

    gtest_discover_tests(unit_snn_test_snn_config
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;snn;config"
            TIMEOUT 60
    )

    message(STATUS "  Added SNN unit test: test_snn_config")
endif()

# ============================================================================
# Test 2: SNN Network
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_network.cpp)
    add_executable(unit_snn_test_snn_network
        test_snn_network.cpp
    )

    target_link_libraries(unit_snn_test_snn_network
        PRIVATE ${SNN_TEST_LIBS}
    )

    target_include_directories(unit_snn_test_snn_network
        PRIVATE ${SNN_TEST_INCLUDES}
    )

    target_compile_features(unit_snn_test_snn_network PRIVATE cxx_std_17)

    gtest_discover_tests(unit_snn_test_snn_network
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;snn;network"
            TIMEOUT 120
    )

    message(STATUS "  Added SNN unit test: test_snn_network")
endif()

# ============================================================================
# Test 3: SNN Encoding/Decoding
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_encoding.cpp)
    add_executable(unit_snn_test_snn_encoding
        test_snn_encoding.cpp
    )

    target_link_libraries(unit_snn_test_snn_encoding
        PRIVATE ${SNN_TEST_LIBS}
    )

    target_include_directories(unit_snn_test_snn_encoding
        PRIVATE ${SNN_TEST_INCLUDES}
    )

    target_compile_features(unit_snn_test_snn_encoding PRIVATE cxx_std_17)

    gtest_discover_tests(unit_snn_test_snn_encoding
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;snn;encoding"
            TIMEOUT 60
    )

    message(STATUS "  Added SNN unit test: test_snn_encoding")
endif()

# ============================================================================
# Test 4: SNN Training
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_training.cpp)
    add_executable(unit_snn_test_snn_training
        test_snn_training.cpp
    )

    target_link_libraries(unit_snn_test_snn_training
        PRIVATE ${SNN_TEST_LIBS}
    )

    target_include_directories(unit_snn_test_snn_training
        PRIVATE ${SNN_TEST_INCLUDES}
    )

    target_compile_features(unit_snn_test_snn_training PRIVATE cxx_std_17)

    gtest_discover_tests(unit_snn_test_snn_training
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;snn;training"
            TIMEOUT 60
    )

    message(STATUS "  Added SNN unit test: test_snn_training")
endif()

# ============================================================================
# Test 4b: SNN Anti-Reward Branch
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_anti_reward.cpp)
    add_executable(unit_snn_test_snn_anti_reward
        test_snn_anti_reward.cpp
    )

    target_link_libraries(unit_snn_test_snn_anti_reward
        PRIVATE ${SNN_TEST_LIBS}
    )

    target_include_directories(unit_snn_test_snn_anti_reward
        PRIVATE ${SNN_TEST_INCLUDES}
    )

    target_compile_features(unit_snn_test_snn_anti_reward PRIVATE cxx_std_17)

    gtest_discover_tests(unit_snn_test_snn_anti_reward
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;snn;training;anti_reward"
            TIMEOUT 60
    )

    message(STATUS "  Added SNN unit test: test_snn_anti_reward")
endif()

# ============================================================================
# Test 5: SNN Bio-Async Integration
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_bio_async.cpp)
    add_executable(unit_snn_test_snn_bio_async
        test_snn_bio_async.cpp
    )

    target_link_libraries(unit_snn_test_snn_bio_async
        PRIVATE ${SNN_TEST_LIBS}
    )

    target_include_directories(unit_snn_test_snn_bio_async
        PRIVATE ${SNN_TEST_INCLUDES}
    )

    target_compile_features(unit_snn_test_snn_bio_async PRIVATE cxx_std_17)

    gtest_discover_tests(unit_snn_test_snn_bio_async
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;snn;bio_async"
            TIMEOUT 60
    )

    message(STATUS "  Added SNN unit test: test_snn_bio_async")
endif()

# ============================================================================
# Test 6: SNN Immune Integration
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_immune.cpp)
    add_executable(unit_snn_test_snn_immune
        test_snn_immune.cpp
    )

    target_link_libraries(unit_snn_test_snn_immune
        PRIVATE ${SNN_TEST_LIBS}
    )

    target_include_directories(unit_snn_test_snn_immune
        PRIVATE ${SNN_TEST_INCLUDES}
    )

    target_compile_features(unit_snn_test_snn_immune PRIVATE cxx_std_17)

    gtest_discover_tests(unit_snn_test_snn_immune
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;snn;immune"
            TIMEOUT 60
    )

    message(STATUS "  Added SNN unit test: test_snn_immune")
endif()

# ============================================================================
# Include bridges subdirectory
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/bridges/CMakeLists.txt)
    add_subdirectory(bridges)
endif()

# ============================================================================
# Test 7: SNN Numerical Stability
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_numerical_stability.cpp)
    add_executable(unit_snn_test_snn_numerical_stability
        test_snn_numerical_stability.cpp
    )

    target_link_libraries(unit_snn_test_snn_numerical_stability
        PRIVATE ${SNN_TEST_LIBS}
    )

    target_include_directories(unit_snn_test_snn_numerical_stability
        PRIVATE ${SNN_TEST_INCLUDES}
    )

    target_compile_features(unit_snn_test_snn_numerical_stability PRIVATE cxx_std_17)

    gtest_discover_tests(unit_snn_test_snn_numerical_stability
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;snn;numerical_stability"
            TIMEOUT 60
    )

    message(STATUS "  Added SNN unit test: test_snn_numerical_stability")
endif()

# ============================================================================
# Test 8: SNN Adaptation (shared AHP / Na-K pump state)
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_adaptation.cpp)
    add_executable(unit_snn_test_snn_adaptation
        test_snn_adaptation.cpp
    )

    target_link_libraries(unit_snn_test_snn_adaptation
        PRIVATE ${SNN_TEST_LIBS}
    )

    target_include_directories(unit_snn_test_snn_adaptation
        PRIVATE ${SNN_TEST_INCLUDES}
    )

    target_compile_features(unit_snn_test_snn_adaptation PRIVATE cxx_std_17)

    gtest_discover_tests(unit_snn_test_snn_adaptation
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;snn;adaptation"
            TIMEOUT 60
    )

    message(STATUS "  Added SNN unit test: test_snn_adaptation")
endif()

# ============================================================================
# Test 9: SNN Basket Cell Pool
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_basket.cpp)
    add_executable(unit_snn_test_snn_basket
        test_snn_basket.cpp
    )

    target_link_libraries(unit_snn_test_snn_basket
        PRIVATE ${SNN_TEST_LIBS}
    )

    target_include_directories(unit_snn_test_snn_basket
        PRIVATE ${SNN_TEST_INCLUDES}
    )

    target_compile_features(unit_snn_test_snn_basket PRIVATE cxx_std_17)

    gtest_discover_tests(unit_snn_test_snn_basket
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;snn;basket"
            TIMEOUT 60
    )

    message(STATUS "  Added SNN unit test: test_snn_basket")
endif()

# ============================================================================
# Test 10: SNN Depression Tuning (runtime-tunable STD dynamics)
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_depression_tuning.cpp)
    add_executable(unit_snn_test_snn_depression_tuning
        test_snn_depression_tuning.cpp
    )

    target_link_libraries(unit_snn_test_snn_depression_tuning
        PRIVATE ${SNN_TEST_LIBS}
    )

    target_include_directories(unit_snn_test_snn_depression_tuning
        PRIVATE ${SNN_TEST_INCLUDES}
    )

    target_compile_features(unit_snn_test_snn_depression_tuning PRIVATE cxx_std_17)

    gtest_discover_tests(unit_snn_test_snn_depression_tuning
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;snn;depression_tuning"
            TIMEOUT 60
    )

    message(STATUS "  Added SNN unit test: test_snn_depression_tuning")
endif()

# ============================================================================
# Test 11: SNN Substrate Adapter (Phase 1 biological-substrate wiring)
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_substrate_adapter.cpp)
    add_executable(unit_snn_test_snn_substrate_adapter
        test_snn_substrate_adapter.cpp
    )

    target_link_libraries(unit_snn_test_snn_substrate_adapter
        PRIVATE ${SNN_TEST_LIBS}
    )

    target_include_directories(unit_snn_test_snn_substrate_adapter
        PRIVATE ${SNN_TEST_INCLUDES}
    )

    target_compile_features(unit_snn_test_snn_substrate_adapter PRIVATE cxx_std_17)

    gtest_discover_tests(unit_snn_test_snn_substrate_adapter
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;snn;substrate"
            TIMEOUT 120
    )

    message(STATUS "  Added SNN unit test: test_snn_substrate_adapter")
endif()

# ============================================================================
# Test 12: CB membrane helpers (conductance-based migration, header-only)
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_membrane_unit.cpp)
    add_executable(unit_snn_test_snn_membrane_unit
        test_snn_membrane_unit.cpp
    )
    target_link_libraries(unit_snn_test_snn_membrane_unit
        PRIVATE ${SNN_TEST_LIBS}
    )
    target_include_directories(unit_snn_test_snn_membrane_unit
        PRIVATE ${SNN_TEST_INCLUDES}
    )
    target_compile_features(unit_snn_test_snn_membrane_unit PRIVATE cxx_std_17)
    gtest_discover_tests(unit_snn_test_snn_membrane_unit
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;snn;cb"
            TIMEOUT 60
    )
    message(STATUS "  Added SNN unit test: test_snn_membrane_unit")
endif()

# ============================================================================
# Test 13: SNN Per-Receptor Membrane Kernels (CB Phase 0 P0 — 4-receptor split)
# ============================================================================
# WHAT: Pure-function tests for snn_membrane_nmda_mg_block + the new 4-receptor
#       (AMPA / NMDA / GABA_A / GABA_B) signatures of compute_dv, decay_one,
#       and deposit_synapse in nimcp_snn_membrane.h.
# WHY:  These header-inline helpers are the single source of truth for the
#       per-receptor membrane equation. Bugs here would corrupt every CB-mode
#       SNN population.
# HOW:  Self-contained C99 test harness — no GTest, no Check, no link to the
#       full nimcp library — so the suite remains buildable even while other
#       SNN translation units are mid-migration.
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_per_receptor.c)
    add_executable(test_snn_per_receptor
        test_snn_per_receptor.c
    )
    target_include_directories(test_snn_per_receptor PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/snn
    )
    target_compile_features(test_snn_per_receptor PRIVATE c_std_11)
    target_compile_options(test_snn_per_receptor PRIVATE
        -Wall -Wextra
    )
    target_link_libraries(test_snn_per_receptor PRIVATE m)

    add_test(NAME unit_snn_test_snn_per_receptor
             COMMAND test_snn_per_receptor
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(unit_snn_test_snn_per_receptor
        PROPERTIES
            LABELS "unit;snn;cb;per_receptor"
            TIMEOUT 30)

    message(STATUS "  Added SNN unit test: test_snn_per_receptor")
endif()

# ============================================================================
# Test 14: SNN Layer-Specific Pyramidal Subtypes (P4.1 — L23/L4_STELLATE/L5_BETZ)
# ============================================================================
# WHAT: Pure-function tests for snn_pop_lif_params() under the three new
#       layer-specific pyramidal subtypes added by P4.1.
# WHY:  Layer-aware membrane dynamics depend on the parameter deltas applied
#       by snn_pop_lif_params(). Silent regression there breaks Betz-fires-
#       earlier and L4-stellate-demands-convergence ranking.
# HOW:  Header-only — stack-allocates an snn_population_t with only the
#       subclass set, plus an snn_config_default()-loaded snn_config_t.
#       No SNN network construction, no GPU; links nimcp_snn for
#       snn_config_default symbol.
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_layer_subtypes_unit.cpp)
    add_executable(unit_snn_test_snn_layer_subtypes_unit
        test_snn_layer_subtypes_unit.cpp
    )

    target_link_libraries(unit_snn_test_snn_layer_subtypes_unit
        PRIVATE ${SNN_TEST_LIBS}
    )

    target_include_directories(unit_snn_test_snn_layer_subtypes_unit
        PRIVATE ${SNN_TEST_INCLUDES}
    )

    target_compile_features(unit_snn_test_snn_layer_subtypes_unit PRIVATE cxx_std_17)

    gtest_discover_tests(unit_snn_test_snn_layer_subtypes_unit
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;snn;layer_subtypes;p4.1"
            TIMEOUT 30
    )

    message(STATUS "  Added SNN unit test: test_snn_layer_subtypes_unit")
endif()

# ============================================================================
# Test 15: SNN Dale's Principle Validator (Wave C task #260)
# ============================================================================
# WHAT: Unit tests for snn_network_validate_dale() — flags any source pop
#       that emits both excitatory (AMPA/NMDA) and inhibitory (GABA_A/GABA_B)
#       synapses (a Dale's principle violation).
# WHY:  The per-receptor migration (g_ampa/g_nmda/g_gaba_a/g_gaba_b) makes
#       Dale violations biophysically observable for the first time. Pure
#       read; no wiring side effects.
# HOW:  GoogleTest. Synthetic mini-networks (clean E-only, clean I-only,
#       mixed violator, empty, single pop with no outgoing).
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_dale_unit.cpp)
    add_executable(unit_snn_test_snn_dale_unit
        test_snn_dale_unit.cpp
    )
    target_link_libraries(unit_snn_test_snn_dale_unit
        PRIVATE ${SNN_TEST_LIBS}
    )
    target_include_directories(unit_snn_test_snn_dale_unit
        PRIVATE ${SNN_TEST_INCLUDES}
    )
    target_compile_features(unit_snn_test_snn_dale_unit PRIVATE cxx_std_17)
    gtest_discover_tests(unit_snn_test_snn_dale_unit
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;snn;dale;wave_c"
            TIMEOUT 60
    )
    message(STATUS "  Added SNN unit test: test_snn_dale_unit")
endif()

# ============================================================================
# SNN Gap-Junction Unit Test (Wave D — PV electrical syncytium)
# ============================================================================
# WHAT: Unit tests for snn_network_set_pop_gap_coupling() + the per-pop
#       mean-pull-toward-mean math applied in the SNN hot loop.
# WHY:  PV gap junctions are the substrate for cortical gamma rhythm; the
#       per-neuron coupling math must be correct in isolation before
#       integration tests can verify electrical propagation.
# HOW:  Google Test. Builds tiny lightweight CSR pops, sets membrane V
#       directly, drives a single network step with conductance_mode ON,
#       and verifies post-step V matches the closed-form coupling formula.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_gap_junction_unit.cpp)
    add_executable(unit_snn_test_snn_gap_junction_unit
        test_snn_gap_junction_unit.cpp
    )
    target_link_libraries(unit_snn_test_snn_gap_junction_unit
        PRIVATE ${SNN_TEST_LIBS}
    )
    target_include_directories(unit_snn_test_snn_gap_junction_unit
        PRIVATE ${SNN_TEST_INCLUDES}
    )
    target_compile_features(unit_snn_test_snn_gap_junction_unit PRIVATE cxx_std_17)
    gtest_discover_tests(unit_snn_test_snn_gap_junction_unit
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;snn;gap_junction;cb;wave_d"
            TIMEOUT 60
    )
    message(STATUS "  Added SNN unit test: test_snn_gap_junction_unit")
endif()

# ============================================================================
# SNN Conduction-Delay Unit Test (Wave E FFI fix, 2026-04-27)
# ============================================================================
# WHAT: Unit tests for snn_network_set_pop_conduction_delay() and the
#       per-pop spike-history ring buffer math used by the deposit kernel.
# WHY:  Pre-Wave-E the deposit kernel collapsed conduction delay to 0 ms
#       (read src->spike_output for the SAME tick the spike was emitted),
#       eliminating the canonical FFI timing window. The fix adds a
#       per-pop ring; the index math must be correct in isolation before
#       integration tests can verify FFI timing emerges.
# HOW:  Google Test. Builds tiny lightweight CSR pops, pokes the ring
#       directly via the public population struct, and verifies the
#       round-trip semantics at offsets 0..MAX.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_conduction_delay_unit.cpp)
    add_executable(unit_snn_test_snn_conduction_delay_unit
        test_snn_conduction_delay_unit.cpp
    )
    target_link_libraries(unit_snn_test_snn_conduction_delay_unit
        PRIVATE ${SNN_TEST_LIBS}
    )
    target_include_directories(unit_snn_test_snn_conduction_delay_unit
        PRIVATE ${SNN_TEST_INCLUDES}
    )
    target_compile_features(unit_snn_test_snn_conduction_delay_unit PRIVATE cxx_std_17)
    gtest_discover_tests(unit_snn_test_snn_conduction_delay_unit
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;snn;conduction_delay;ffi;wave_e"
            TIMEOUT 60
    )
    message(STATUS "  Added SNN unit test: test_snn_conduction_delay_unit")
endif()

# ============================================================================
# SNN Per-Neuron Heterogeneity Unit Test (Wave G, 2026-04-27)
# ============================================================================
# WHAT: Unit tests for snn_network_set_pop_heterogeneity() and the
#       snn_pop_neuron_lif_params() lookup helper.
# WHY:  Per-neuron τ_mem / v_thresh distributions are the structural fix for
#       lock-step firing in homogeneous pops. The setter contract + helper
#       fallback semantics must be correct in isolation before integration
#       tests can verify desynchronisation emerges.
# HOW:  Google Test. Builds tiny lightweight CSR pops, exercises the setter
#       (allocation, σ clamp, round-trip), and pins the helper's NULL-
#       fallback / per-neuron-override behaviour.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_heterogeneity_unit.cpp)
    add_executable(unit_snn_test_snn_heterogeneity_unit
        test_snn_heterogeneity_unit.cpp
    )
    target_link_libraries(unit_snn_test_snn_heterogeneity_unit
        PRIVATE ${SNN_TEST_LIBS}
    )
    target_include_directories(unit_snn_test_snn_heterogeneity_unit
        PRIVATE ${SNN_TEST_INCLUDES}
    )
    target_compile_features(unit_snn_test_snn_heterogeneity_unit PRIVATE cxx_std_17)
    gtest_discover_tests(unit_snn_test_snn_heterogeneity_unit
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;snn;heterogeneity;wave_g"
            TIMEOUT 60
    )
    message(STATUS "  Added SNN unit test: test_snn_heterogeneity_unit")
endif()

# ============================================================================
# SNN GPU Per-Neuron LIF Unit Test (Wave G GPU sync, schema v17, 2026-04-27)
# ============================================================================
# WHAT: Unit tests for nimcp_gpu_lif_state_upload_per_neuron_params and the
#       kernel_lif_forward / kernel_lif_forward_simple per-neuron path. Pins
#       (1) bit-identity contract (NULL arrays → scalar fallback),
#       (2) heterogeneous τ_mem produces V-trajectory divergence,
#       (3) heterogeneous v_thresh produces spike-count divergence,
#       (4) NULL re-upload symmetry, (5) size-mismatch rejection,
#       (6) fresh-state dirty flag.
# WHY:  Pre-Wave-G GPU LIF kernel ignored both subclass deltas AND per-neuron
#       heterogeneity, so tier-pyr pops behaved homogeneously on GPU. Pin
#       the new contract before integration tests.
# HOW:  Google Test. Skips gracefully on hosts without a GPU (nimcp_gpu_
#       context_create_auto returning NULL).
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_gpu_per_neuron_lif_unit.cpp)
    add_executable(unit_snn_test_snn_gpu_per_neuron_lif_unit
        test_snn_gpu_per_neuron_lif_unit.cpp
    )
    target_link_libraries(unit_snn_test_snn_gpu_per_neuron_lif_unit
        PRIVATE ${SNN_TEST_LIBS}
    )
    target_include_directories(unit_snn_test_snn_gpu_per_neuron_lif_unit
        PRIVATE ${SNN_TEST_INCLUDES}
    )
    target_compile_features(unit_snn_test_snn_gpu_per_neuron_lif_unit
        PRIVATE cxx_std_17)
    gtest_discover_tests(unit_snn_test_snn_gpu_per_neuron_lif_unit
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;snn;heterogeneity;gpu;wave_g"
            TIMEOUT 60
    )
    message(STATUS "  Added SNN unit test: test_snn_gpu_per_neuron_lif_unit")
endif()

# ============================================================================
# SNN CB GPU unit test — alloc/free/upload/download + forward_cb dynamics
# ============================================================================
# WHAT: Unit-level pinning of the CB GPU port (CB-GPU-2 + CB-GPU-3):
#       lif_state CB tensor lifecycle, host↔device round-trip for the four
#       receptor conductance arrays, excitatory saturation toward E_ampa,
#       inhibitory clamp toward E_gaba_a, and NMDA Mg²⁺ block.
# WHY:  CB-GPU-4 will swap snn_network_step over to nimcp_gpu_lif_forward_cb.
#       The per-function contract has to be locked first or any subsequent
#       wiring bug looks like a kernel bug.
# HOW:  Google Test. Skips on no-GPU build hosts.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_cb_gpu_unit.cpp)
    add_executable(unit_snn_test_snn_cb_gpu_unit
        test_snn_cb_gpu_unit.cpp
    )
    target_link_libraries(unit_snn_test_snn_cb_gpu_unit
        PRIVATE ${SNN_TEST_LIBS}
    )
    target_include_directories(unit_snn_test_snn_cb_gpu_unit
        PRIVATE ${SNN_TEST_INCLUDES}
    )
    target_compile_features(unit_snn_test_snn_cb_gpu_unit
        PRIVATE cxx_std_17)
    gtest_discover_tests(unit_snn_test_snn_cb_gpu_unit
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;snn;cb;gpu"
            TIMEOUT 60
    )
    message(STATUS "  Added SNN unit test: test_snn_cb_gpu_unit")
endif()

# ============================================================================
# SNN Dendritic Unit Test (Wave H — basal+apical compartments + NMDA plateau)
# ============================================================================
# WHAT: Unit tests for snn_membrane_compute_dv_two_compartment, the plateau
#       onset detector, the plateau decay envelope, the compartmental
#       deposit-routing helper, and the dendritic_enabled tune flag.
# WHY:  These header-inline helpers are the single source of truth for the
#       Wave H two-compartment integration. Bugs here corrupt every
#       dendritic-mode SNN pop.
# HOW:  GoogleTest. Pure-function tests on the header — no SNN network
#       construction required for the unit tier.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_dendritic_unit.cpp)
    add_executable(unit_snn_test_snn_dendritic_unit
        test_snn_dendritic_unit.cpp
    )
    target_link_libraries(unit_snn_test_snn_dendritic_unit
        PRIVATE ${SNN_TEST_LIBS}
    )
    target_include_directories(unit_snn_test_snn_dendritic_unit
        PRIVATE ${SNN_TEST_INCLUDES}
    )
    target_compile_features(unit_snn_test_snn_dendritic_unit PRIVATE cxx_std_17)
    gtest_discover_tests(unit_snn_test_snn_dendritic_unit
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;snn;dendritic;wave_h"
            TIMEOUT 60
    )
    message(STATUS "  Added SNN unit test: test_snn_dendritic_unit")
endif()

# ============================================================================
# Test: Language adapter SNN-pop attach (broca + wernicke)
# WHAT: Pure state-machine of broca/wernicke_attach_snn_pop + the get-helpers.
# WHY:  Pin "0 is a valid pop id" calloc trap + unbind path consistency.
# HOW:  GoogleTest. Sentinel SNN pointer (never derefed) keeps it unit-fast.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_lang_attach_unit.cpp)
    add_executable(unit_snn_test_lang_attach_unit
        test_lang_attach_unit.cpp
    )
    target_link_libraries(unit_snn_test_lang_attach_unit
        PRIVATE ${SNN_TEST_LIBS}
    )
    target_include_directories(unit_snn_test_lang_attach_unit
        PRIVATE ${SNN_TEST_INCLUDES}
    )
    target_compile_features(unit_snn_test_lang_attach_unit PRIVATE cxx_std_17)
    gtest_discover_tests(unit_snn_test_lang_attach_unit
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;snn;language;adapter"
            TIMEOUT 60
    )
    message(STATUS "  Added SNN unit test: test_lang_attach_unit")
endif()

# ============================================================================
# Future tests (uncomment as source files are added):
# ============================================================================
# - test_snn_population.cpp
# - test_snn_simulation.cpp

# Exception handling tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/exception/CMakeLists.txt")
    add_subdirectory(exception)
endif()
