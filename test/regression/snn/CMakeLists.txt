# ============================================================================
# SNN Regression Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for SNN regression tests
# WHY:  Verify SNN stability and prevent regressions
# HOW:  Build GoogleTest executable with deterministic tests
# ============================================================================

# Guard: Check GTest availability
if(NOT GTest_FOUND)
    message(STATUS "GTest not found - skipping SNN regression tests")
    return()
endif()

# Common link libraries for SNN regression tests
# NOTE: z is needed because libnimcp_snn.a references gzseek from nimcp_snn_network.c.
# libnimcp.so exports zlib symbols transitively, but the static archive ordering in
# the link line requires an explicit -lz here.
find_package(ZLIB)
set(SNN_REGRESSION_TEST_LIBS
    nimcp_snn
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)
if(ZLIB_FOUND)
    list(APPEND SNN_REGRESSION_TEST_LIBS ZLIB::ZLIB)
else()
    list(APPEND SNN_REGRESSION_TEST_LIBS z)
endif()

# Common include directories
set(SNN_REGRESSION_TEST_INCLUDES
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/snn
)

# ============================================================================
# SNN Regression Test
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_regression.cpp)
    add_executable(regression_snn_test_regression
        test_snn_regression.cpp
    )

    target_link_libraries(regression_snn_test_regression
        PRIVATE ${SNN_REGRESSION_TEST_LIBS}
    )

    target_include_directories(regression_snn_test_regression
        PRIVATE ${SNN_REGRESSION_TEST_INCLUDES}
    )

    target_compile_features(regression_snn_test_regression PRIVATE cxx_std_17)

    gtest_discover_tests(regression_snn_test_regression
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;snn"
            TIMEOUT 120
    )

    message(STATUS "  Added SNN regression test: test_snn_regression")
endif()

# ============================================================================
# SNN Bridge Regression Test
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_bridge_regression.cpp)
    add_executable(regression_snn_test_bridge_regression
        test_snn_bridge_regression.cpp
    )

    target_link_libraries(regression_snn_test_bridge_regression
        PRIVATE ${SNN_REGRESSION_TEST_LIBS}
    )

    target_include_directories(regression_snn_test_bridge_regression
        PRIVATE ${SNN_REGRESSION_TEST_INCLUDES}
    )

    target_compile_features(regression_snn_test_bridge_regression PRIVATE cxx_std_17)

    gtest_discover_tests(regression_snn_test_bridge_regression
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;snn;bridges"
            TIMEOUT 120
    )

    message(STATUS "  Added SNN bridge regression test: test_snn_bridge_regression")
endif()

# ============================================================================
# SNN Numerical Stability Regression Test
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_numerical_regression.cpp)
    add_executable(regression_snn_test_numerical_regression
        test_snn_numerical_regression.cpp
    )

    target_link_libraries(regression_snn_test_numerical_regression
        PRIVATE ${SNN_REGRESSION_TEST_LIBS}
    )

    target_include_directories(regression_snn_test_numerical_regression
        PRIVATE ${SNN_REGRESSION_TEST_INCLUDES}
    )

    target_compile_features(regression_snn_test_numerical_regression PRIVATE cxx_std_17)

    gtest_discover_tests(regression_snn_test_numerical_regression
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;snn;numerical"
            TIMEOUT 120
    )

    message(STATUS "  Added SNN numerical regression test: test_snn_numerical_regression")
endif()

# ============================================================================
# SNN Language Bridge Regression Tests
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_language_bridge_regression.cpp)
    add_executable(regression_snn_language_bridge
        test_snn_language_bridge_regression.cpp
    )

    target_link_libraries(regression_snn_language_bridge
        PRIVATE ${SNN_REGRESSION_TEST_LIBS}
    )

    target_include_directories(regression_snn_language_bridge
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src
    )

    target_compile_features(regression_snn_language_bridge PRIVATE cxx_std_17)

    gtest_discover_tests(regression_snn_language_bridge
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;snn;language;bridge"
            TIMEOUT 120
    )

    message(STATUS "  Added SNN language bridge regression test: test_snn_language_bridge_regression")
endif()

# ============================================================================
# SNN Biophysical Disable-Path Regression Test
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_biophysical_regression.cpp)
    add_executable(regression_snn_test_snn_biophysical_regression
        test_snn_biophysical_regression.cpp
    )

    target_link_libraries(regression_snn_test_snn_biophysical_regression
        PRIVATE ${SNN_REGRESSION_TEST_LIBS}
    )

    target_include_directories(regression_snn_test_snn_biophysical_regression
        PRIVATE ${SNN_REGRESSION_TEST_INCLUDES}
    )

    target_compile_features(regression_snn_test_snn_biophysical_regression PRIVATE cxx_std_17)

    gtest_discover_tests(regression_snn_test_snn_biophysical_regression
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;snn;biophysical"
            TIMEOUT 180
    )

    message(STATUS "  Added SNN biophysical regression test: test_snn_biophysical_regression")
endif()

# ============================================================================
# CB conductance regression test (CB migration — bit-identity guarantee)
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_conductance_regression.cpp)
    add_executable(regression_snn_test_snn_conductance_regression
        test_snn_conductance_regression.cpp
    )
    target_link_libraries(regression_snn_test_snn_conductance_regression
        PRIVATE ${SNN_REGRESSION_TEST_LIBS}
    )
    target_include_directories(regression_snn_test_snn_conductance_regression
        PRIVATE ${SNN_REGRESSION_TEST_INCLUDES}
    )
    target_compile_features(regression_snn_test_snn_conductance_regression
        PRIVATE cxx_std_17)
    gtest_discover_tests(regression_snn_test_snn_conductance_regression
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;snn;cb"
            TIMEOUT 120
    )
    message(STATUS "  Added SNN regression test: test_snn_conductance_regression")
endif()

# ============================================================================
# CB GPU regression test — current-based path unchanged, lifecycle safe
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_cb_gpu_regression.cpp)
    add_executable(regression_snn_test_snn_cb_gpu_regression
        test_snn_cb_gpu_regression.cpp
    )
    target_link_libraries(regression_snn_test_snn_cb_gpu_regression
        PRIVATE ${SNN_REGRESSION_TEST_LIBS}
    )
    target_include_directories(regression_snn_test_snn_cb_gpu_regression
        PRIVATE ${SNN_REGRESSION_TEST_INCLUDES}
    )
    target_compile_features(regression_snn_test_snn_cb_gpu_regression
        PRIVATE cxx_std_17)
    gtest_discover_tests(regression_snn_test_snn_cb_gpu_regression
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;snn;cb;gpu"
            TIMEOUT 120
    )
    message(STATUS "  Added SNN regression test: test_snn_cb_gpu_regression")
endif()

# ============================================================================
# CB CPU hot loop baseline — regression hash-locks the inline loop's
# behavior so the CB-GPU-5 Phase A helper extraction can't drift.
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_cb_loop_baseline.cpp)
    add_executable(regression_snn_test_snn_cb_loop_baseline
        test_snn_cb_loop_baseline.cpp
    )
    target_link_libraries(regression_snn_test_snn_cb_loop_baseline
        PRIVATE ${SNN_REGRESSION_TEST_LIBS}
    )
    target_include_directories(regression_snn_test_snn_cb_loop_baseline
        PRIVATE ${SNN_REGRESSION_TEST_INCLUDES}
    )
    target_compile_features(regression_snn_test_snn_cb_loop_baseline
        PRIVATE cxx_std_17)
    gtest_discover_tests(regression_snn_test_snn_cb_loop_baseline
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;snn;cb;baseline"
            TIMEOUT 60
    )
    message(STATUS "  Added SNN regression test: test_snn_cb_loop_baseline")
endif()

# ============================================================================
# SNN per-receptor (P0) regression test — plain C, no GTest.
# Verifies that the four-bucket conductance model matches the old lumped
# (g_exc / g_inh) CB behavior when wired with a single receptor type per
# pathway. Self-contained mini test harness (same style as
# test/regression/test_cycle_coordinator_regression.c).
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_per_receptor_regression.c)
    add_executable(regression_snn_test_per_receptor_regression
        test_snn_per_receptor_regression.c
    )
    target_link_libraries(regression_snn_test_per_receptor_regression
        PRIVATE
            nimcp_snn
            nimcp
            Threads::Threads
            m
    )
    if(ZLIB_FOUND)
        target_link_libraries(regression_snn_test_per_receptor_regression
            PRIVATE ZLIB::ZLIB)
    else()
        target_link_libraries(regression_snn_test_per_receptor_regression
            PRIVATE z)
    endif()
    target_include_directories(regression_snn_test_per_receptor_regression
        PRIVATE ${SNN_REGRESSION_TEST_INCLUDES}
    )
    target_compile_features(regression_snn_test_per_receptor_regression
        PRIVATE c_std_11)
    add_test(NAME regression_snn_test_per_receptor_regression
             COMMAND regression_snn_test_per_receptor_regression
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(regression_snn_test_per_receptor_regression
        PROPERTIES
            LABELS "regression;snn;cb;per_receptor"
            TIMEOUT 120)
    message(STATUS "  Added SNN per-receptor regression test: test_snn_per_receptor_regression")
endif()

# ============================================================================
# SNN Dale's Principle Regression Test (Wave C task #260)
# ============================================================================
# WHAT: Regression test for snn_network_validate_dale() — guarantees the
#       validator always catches a deliberately-introduced Dale violation
#       (pop A → dst1 AMPA, pop A → dst2 GABA_A) and stays silent on a
#       clean control with the same shape.
# WHY:  If a future refactor lets a chimeric src pop slip past the validator,
#       the daemon would deposit the same spike into both g_ampa and g_gaba_a.
# HOW:  GoogleTest, two cases (violation + matched clean control).
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_dale_regression.cpp)
    add_executable(regression_snn_test_snn_dale_regression
        test_snn_dale_regression.cpp
    )
    target_link_libraries(regression_snn_test_snn_dale_regression
        PRIVATE ${SNN_REGRESSION_TEST_LIBS}
    )
    target_include_directories(regression_snn_test_snn_dale_regression
        PRIVATE ${SNN_REGRESSION_TEST_INCLUDES}
    )
    target_compile_features(regression_snn_test_snn_dale_regression
        PRIVATE cxx_std_17)
    gtest_discover_tests(regression_snn_test_snn_dale_regression
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;snn;dale;wave_c"
            TIMEOUT 60
    )
    message(STATUS "  Added SNN Dale regression test: test_snn_dale_regression")
endif()

# ============================================================================
# SNN Gap-Junction Regression Test (Wave D — default-OFF bit-identity)
# ============================================================================
# WHAT: Verifies gap_coupling=0 (default) produces BIT-IDENTICAL CB output
#       to a network whose gap_coupling was explicitly set to 0 — pinning
#       the contract that the gap-junction hot-loop branch is a pure no-op
#       at default.
# WHY:  Wave D adds a new branch in snn_network_step. The CB regression
#       contract requires the default-OFF behaviour to be unchanged; this
#       test guards against silent-drift in the legacy path.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_gap_junction_regression.cpp)
    add_executable(regression_snn_test_snn_gap_junction_regression
        test_snn_gap_junction_regression.cpp
    )
    target_link_libraries(regression_snn_test_snn_gap_junction_regression
        PRIVATE ${SNN_REGRESSION_TEST_LIBS}
    )
    target_include_directories(regression_snn_test_snn_gap_junction_regression
        PRIVATE ${SNN_REGRESSION_TEST_INCLUDES}
    )
    target_compile_features(regression_snn_test_snn_gap_junction_regression
        PRIVATE cxx_std_17)
    gtest_discover_tests(regression_snn_test_snn_gap_junction_regression
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;snn;gap_junction;cb;wave_d"
            TIMEOUT 90
    )
    message(STATUS "  Added SNN gap-junction regression test: test_snn_gap_junction_regression")
endif()

# ============================================================================
# SNN Conduction-Delay Regression Test (Wave E FFI fix, 2026-04-27)
# ============================================================================
# WHAT: Pins the OFF-mode regression contract: with all pops at default
#       conduction_delay_steps = 0, the new per-pop ring-buffer machinery
#       is observably indistinguishable from the pre-Wave-E zero-delay
#       deposit kernel. Also pins that delay > 0 actually shifts arrival.
# WHY:  Wave E adds a deposit-time read indirection. The default-OFF case
#       must remain bit-identical to a network where the setter was never
#       called — otherwise downstream training checkpoints could silently
#       change.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_conduction_delay_regression.cpp)
    add_executable(regression_snn_test_snn_conduction_delay_regression
        test_snn_conduction_delay_regression.cpp
    )
    target_link_libraries(regression_snn_test_snn_conduction_delay_regression
        PRIVATE ${SNN_REGRESSION_TEST_LIBS}
    )
    target_include_directories(regression_snn_test_snn_conduction_delay_regression
        PRIVATE ${SNN_REGRESSION_TEST_INCLUDES}
    )
    target_compile_features(regression_snn_test_snn_conduction_delay_regression
        PRIVATE cxx_std_17)
    gtest_discover_tests(regression_snn_test_snn_conduction_delay_regression
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;snn;conduction_delay;ffi;wave_e"
            TIMEOUT 90
    )
    message(STATUS "  Added SNN conduction-delay regression test: test_snn_conduction_delay_regression")
endif()

# ============================================================================
# SNN Layer-Specific Pyramidal Subtypes (P4.1) regression test.
# WHAT: Round-trip property — re-tagging a pop back to PYRAMIDAL after L23/
#       L4_STELLATE/L5_BETZ must restore network-default LIF params bit-for-bit.
# WHY:  The default arm of the snn_pop_lif_params() switch is the only path
#       back to defaults. Future caching / state on the pop struct could
#       silently regress this — pin it.
# HOW:  Three pure-function round trips + one through-the-API round trip.
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_layer_subtypes_regression.cpp)
    add_executable(regression_snn_test_layer_subtypes_regression
        test_layer_subtypes_regression.cpp
    )

    target_link_libraries(regression_snn_test_layer_subtypes_regression
        PRIVATE ${SNN_REGRESSION_TEST_LIBS}
    )

    target_include_directories(regression_snn_test_layer_subtypes_regression
        PRIVATE ${SNN_REGRESSION_TEST_INCLUDES}
    )

    target_compile_features(regression_snn_test_layer_subtypes_regression
        PRIVATE cxx_std_17)

    gtest_discover_tests(regression_snn_test_layer_subtypes_regression
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;snn;layer_subtypes;p4.1"
            TIMEOUT 60
    )

    message(STATUS "  Added SNN regression test: test_layer_subtypes_regression")
endif()

# ============================================================================
# SNN Per-Neuron Heterogeneity Regression Test (Wave G, 2026-04-27)
# ============================================================================
# WHAT: Pins the OFF-mode regression contract: with σ=0 (default, arrays
#       NULL) the SNN hot loop produces spike trains identical to a network
#       whose per-neuron arrays are explicitly populated with the pop-wide
#       value (uniform variance 0). Also pins the symmetric off-switch:
#       round-trip σ=0 → σ>0 → σ=0 must restore arrays to NULL.
# WHY:  Wave G adds a per-neuron LIF lookup branch in three step paths.
#       The default-OFF case must remain bit-identical to the pre-Wave-G
#       NULL-array fallback — otherwise downstream training checkpoints
#       could silently change the day this code lands.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_heterogeneity_regression.cpp)
    add_executable(regression_snn_test_snn_heterogeneity_regression
        test_snn_heterogeneity_regression.cpp
    )
    target_link_libraries(regression_snn_test_snn_heterogeneity_regression
        PRIVATE ${SNN_REGRESSION_TEST_LIBS}
    )
    target_include_directories(regression_snn_test_snn_heterogeneity_regression
        PRIVATE ${SNN_REGRESSION_TEST_INCLUDES}
    )
    target_compile_features(regression_snn_test_snn_heterogeneity_regression
        PRIVATE cxx_std_17)
    gtest_discover_tests(regression_snn_test_snn_heterogeneity_regression
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;snn;heterogeneity;wave_g"
            TIMEOUT 90
    )
    message(STATUS "  Added SNN heterogeneity regression test: test_snn_heterogeneity_regression")
endif()

# ============================================================================
# SNN Dendritic Regression Test (Wave H — basal+apical compartments)
# ============================================================================
# WHAT: Pins the OFF-mode regression contract: with dendritic_enabled = false
#       on every pop (default), the SNN step is bit-identical (within ±1
#       spike) to a network where the gate was never set. Also pins that
#       enabling dendritic on a pop measurably changes the per-neuron
#       state — the new branch is not silently dead.
# WHY:  Wave H adds a per-neuron-per-step branch in snn_network_step. The
#       default-OFF case must remain bit-identical to pre-Wave-H — every
#       existing checkpoint must continue to step the same way.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_dendritic_regression.cpp)
    add_executable(regression_snn_test_snn_dendritic_regression
        test_snn_dendritic_regression.cpp
    )
    target_link_libraries(regression_snn_test_snn_dendritic_regression
        PRIVATE ${SNN_REGRESSION_TEST_LIBS}
    )
    target_include_directories(regression_snn_test_snn_dendritic_regression
        PRIVATE ${SNN_REGRESSION_TEST_INCLUDES}
    )
    target_compile_features(regression_snn_test_snn_dendritic_regression
        PRIVATE cxx_std_17)
    gtest_discover_tests(regression_snn_test_snn_dendritic_regression
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;snn;dendritic;wave_h"
            TIMEOUT 90
    )
    message(STATUS "  Added SNN dendritic regression test: test_snn_dendritic_regression")
endif()

# ============================================================================
# Sensorymotor synfire ring regression
# WHAT: Pin synfire-ring topology contract: stage→stage fan-in honored,
#       seed deterministic across runs, receptor type AMPA, no NaN under step.
# WHY:  Drift in SENSORYMOTOR_RING_LCG_SEED or fan-in would silently change
#       delay-line dynamics and break checkpoint reproducibility.
# HOW:  GoogleTest. Re-implements the wiring locally to mirror the
#       production code; this catches divergences between test and impl.
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_lang_synfire_ring_regression.cpp)
    add_executable(regression_snn_test_lang_synfire_ring_regression
        test_lang_synfire_ring_regression.cpp
    )
    target_link_libraries(regression_snn_test_lang_synfire_ring_regression
        PRIVATE ${SNN_REGRESSION_TEST_LIBS}
    )
    target_include_directories(regression_snn_test_lang_synfire_ring_regression
        PRIVATE ${SNN_REGRESSION_TEST_INCLUDES}
    )
    target_compile_features(regression_snn_test_lang_synfire_ring_regression
        PRIVATE cxx_std_17)
    gtest_discover_tests(regression_snn_test_lang_synfire_ring_regression
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;snn;language;sensorymotor"
            TIMEOUT 120
    )
    message(STATUS "  Added SNN synfire ring regression test: test_lang_synfire_ring_regression")
endif()

# ============================================================================
# Include Subdirectories
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/exception)
    add_subdirectory(exception)
endif()
