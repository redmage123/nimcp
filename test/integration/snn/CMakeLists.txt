# ============================================================================
# SNN Integration Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for SNN integration tests
# WHY:  Verify SNN integration with other modules
# HOW:  Build GoogleTest executable linking all required modules
# ============================================================================

# Guard: Check GTest availability
if(NOT GTest_FOUND)
    message(STATUS "GTest not found - skipping SNN integration tests")
    return()
endif()

# Common link libraries for SNN integration tests
# z needed because libnimcp_snn.a references gzseek (mirrors regression CMakeLists).
find_package(ZLIB)
set(SNN_INTEGRATION_TEST_LIBS
    nimcp_snn
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)
if(ZLIB_FOUND)
    list(APPEND SNN_INTEGRATION_TEST_LIBS ZLIB::ZLIB)
else()
    list(APPEND SNN_INTEGRATION_TEST_LIBS z)
endif()

# Common include directories
set(SNN_INTEGRATION_TEST_INCLUDES
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/snn
)

# ============================================================================
# SNN Integration Test
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_integration.cpp)
    add_executable(integration_snn_test_integration
        test_snn_integration.cpp
    )

    target_link_libraries(integration_snn_test_integration
        PRIVATE ${SNN_INTEGRATION_TEST_LIBS}
    )

    target_include_directories(integration_snn_test_integration
        PRIVATE ${SNN_INTEGRATION_TEST_INCLUDES}
    )

    target_compile_features(integration_snn_test_integration PRIVATE cxx_std_17)

    gtest_discover_tests(integration_snn_test_integration
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "integration;snn"
            TIMEOUT 120
    )

    message(STATUS "  Added SNN integration test: test_snn_integration")
endif()

# ============================================================================
# SNN Bridge Integration Test
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_bridge_integration.cpp)
    add_executable(integration_snn_test_bridge_integration
        test_snn_bridge_integration.cpp
    )

    target_link_libraries(integration_snn_test_bridge_integration
        PRIVATE ${SNN_INTEGRATION_TEST_LIBS}
    )

    target_include_directories(integration_snn_test_bridge_integration
        PRIVATE ${SNN_INTEGRATION_TEST_INCLUDES}
    )

    target_compile_features(integration_snn_test_bridge_integration PRIVATE cxx_std_17)

    gtest_discover_tests(integration_snn_test_bridge_integration
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "integration;snn;bridges"
            TIMEOUT 120
    )

    message(STATUS "  Added SNN bridge integration test: test_snn_bridge_integration")
endif()

# ============================================================================
# SNN-Plasticity Integration Test
# ============================================================================
# WHAT: Test STDP and BCM plasticity with full SNN network training
# WHY:  Verify eligibility traces and threshold dynamics
# HOW:  Test plasticity bridges with reward signals

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_plasticity_integration.cpp)
    add_executable(integration_snn_plasticity
        test_snn_plasticity_integration.cpp
    )

    target_link_libraries(integration_snn_plasticity
        PRIVATE ${SNN_INTEGRATION_TEST_LIBS}
    )

    target_include_directories(integration_snn_plasticity
        PRIVATE ${SNN_INTEGRATION_TEST_INCLUDES}
    )

    target_compile_features(integration_snn_plasticity PRIVATE cxx_std_17)

    gtest_discover_tests(integration_snn_plasticity
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "integration;snn;plasticity;stdp;bcm"
            TIMEOUT 180
    )

    message(STATUS "  Added SNN plasticity integration test: test_snn_plasticity_integration")
endif()

# ============================================================================
# SNN Language Bridge Integration Tests
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_language_bridge_integration.cpp)
    add_executable(integration_snn_language_bridge
        test_snn_language_bridge_integration.cpp
    )

    target_link_libraries(integration_snn_language_bridge
        PRIVATE ${SNN_INTEGRATION_TEST_LIBS}
    )

    target_include_directories(integration_snn_language_bridge
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src
    )

    target_compile_features(integration_snn_language_bridge PRIVATE cxx_std_17)

    gtest_discover_tests(integration_snn_language_bridge
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "integration;snn;language;bridge"
            TIMEOUT 180
    )

    message(STATUS "  Added SNN language bridge integration test: test_snn_language_bridge_integration")
endif()

# ============================================================================
# SNN Biophysical Integration Test (2026-04-22)
# ============================================================================
# WHAT: Live integration tests for the five biophysical stability mechanisms
#       (AHP, Na+/K+ pump, basket-cell inhibition, E/I-balanced noise,
#       anti-reward) exercised through snn_network_step and snn_rstdp_apply.
# WHY:  Unit tests cover each mechanism in isolation; this test verifies each
#       knob observably changes pop-level firing or synaptic dynamics in the
#       actual runtime hot path when toggled on/off.
# HOW:  Google Test. Creates lightweight (CSR) populations, forces CPU
#       fallback so the biophysics path runs, and compares baseline vs
#       feature-enabled spike counts / peak rates / synaptic weight deltas.

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_biophysical_integration.cpp)
    add_executable(integration_snn_test_snn_biophysical_integration
        test_snn_biophysical_integration.cpp
    )

    target_link_libraries(integration_snn_test_snn_biophysical_integration
        PRIVATE
            nimcp_snn
            nimcp
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
            m
            z
    )

    target_include_directories(integration_snn_test_snn_biophysical_integration
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/include/snn
    )

    target_compile_features(integration_snn_test_snn_biophysical_integration
        PRIVATE cxx_std_17)

    gtest_discover_tests(integration_snn_test_snn_biophysical_integration
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "integration;snn;biophysical"
            TIMEOUT 300
    )

    message(STATUS "  Added SNN biophysical integration test: "
                   "test_snn_biophysical_integration")
endif()

# ============================================================================
# SNN Thalamic Substrate Composition Test (2026-04-22)
# ============================================================================
# WHAT: Integration test verifying the existing SNN thalamic bridge composes
#       correctly with Wave A+B biophysical features (basket pool, AHP,
#       Na+/K+ pump, E/I-balanced noise, anti-reward).
# WHY:  The bridge predates these features. Wave A+B changes the distribution
#       of pop->spike_output — prove the bridge still routes, attention
#       gates stay finite, and burst-mode thresholds remain sensible.
# HOW:  Google Test. Creates lightweight CSR populations, forces CPU
#       fallback so biophysics runs, builds the thalamic router + SNN
#       thalamic bridge on top, routes spikes through the bridge for each
#       Wave A+B knob configuration.

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_thalamic_substrate_compose.cpp)
    add_executable(integration_snn_test_snn_thalamic_substrate_compose
        test_snn_thalamic_substrate_compose.cpp
    )

    target_link_libraries(integration_snn_test_snn_thalamic_substrate_compose
        PRIVATE
            nimcp_snn
            nimcp
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
            m
            z
    )

    target_include_directories(integration_snn_test_snn_thalamic_substrate_compose
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/include/snn
    )

    target_compile_features(integration_snn_test_snn_thalamic_substrate_compose
        PRIVATE cxx_std_17)

    gtest_discover_tests(integration_snn_test_snn_thalamic_substrate_compose
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "integration;snn;thalamic;biophysical"
            TIMEOUT 300
    )

    message(STATUS "  Added SNN thalamic substrate composition test: "
                   "test_snn_thalamic_substrate_compose")
endif()

# ============================================================================
# SNN Substrate Integration Test (2026-04-22)
# ============================================================================
# WHAT: Integration tests for the SNN substrate adapter (Phase 1 biological-
#       substrate wiring) running inside a live snn_network_step loop with
#       the Wave A+B biophysics in composition.
# WHY:  The unit tests isolate the effects cache + tunables; integration is
#       where composition with AHP/pump/basket/noise is verified — they must
#       stack without destabilization and the substrate must observably
#       modulate firing across a multi-step run.
# HOW:  Google Test. Lightweight CSR pops + CPU fallback. Three cases:
#       ATP ramp-down → reduced firing in last window; full biophysics +
#       substrate stable (no NaN/Inf); update_period rate-limits the cache.

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_substrate_integration.cpp)
    add_executable(integration_snn_test_snn_substrate_integration
        test_snn_substrate_integration.cpp
    )

    target_link_libraries(integration_snn_test_snn_substrate_integration
        PRIVATE
            nimcp_snn
            nimcp
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
            m
            z
    )

    target_include_directories(integration_snn_test_snn_substrate_integration
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/include/snn
    )

    target_compile_features(integration_snn_test_snn_substrate_integration
        PRIVATE cxx_std_17)

    gtest_discover_tests(integration_snn_test_snn_substrate_integration
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "integration;snn;substrate"
            TIMEOUT 300
    )

    message(STATUS "  Added SNN substrate integration test: "
                   "test_snn_substrate_integration")
endif()

# ============================================================================
# CB conductance integration test (CB migration)
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_conductance_integration.cpp)
    add_executable(integration_snn_test_snn_conductance_integration
        test_snn_conductance_integration.cpp
    )
    target_link_libraries(integration_snn_test_snn_conductance_integration
        PRIVATE ${SNN_INTEGRATION_TEST_LIBS}
    )
    target_include_directories(integration_snn_test_snn_conductance_integration
        PRIVATE ${SNN_INTEGRATION_TEST_INCLUDES}
    )
    target_compile_features(integration_snn_test_snn_conductance_integration
        PRIVATE cxx_std_17)
    gtest_discover_tests(integration_snn_test_snn_conductance_integration
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "integration;snn;cb"
            TIMEOUT 120
    )
    message(STATUS "  Added SNN integration test: test_snn_conductance_integration")
endif()

# ============================================================================
# CB GPU integration test — GPU CB kernel matches CPU CB membrane reference
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_cb_gpu_integration.cpp)
    add_executable(integration_snn_test_snn_cb_gpu_integration
        test_snn_cb_gpu_integration.cpp
    )
    target_link_libraries(integration_snn_test_snn_cb_gpu_integration
        PRIVATE ${SNN_INTEGRATION_TEST_LIBS}
    )
    target_include_directories(integration_snn_test_snn_cb_gpu_integration
        PRIVATE ${SNN_INTEGRATION_TEST_INCLUDES}
    )
    target_compile_features(integration_snn_test_snn_cb_gpu_integration
        PRIVATE cxx_std_17)
    gtest_discover_tests(integration_snn_test_snn_cb_gpu_integration
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "integration;snn;cb;gpu"
            TIMEOUT 120
    )
    message(STATUS "  Added SNN integration test: test_snn_cb_gpu_integration")
endif()

# ============================================================================
# SNN Dendritic Integration Test (Wave H — BAC firing)
# ============================================================================
# WHAT: 3-pop integration circuit that pins the BAC-firing phenotype:
#       bottom-up alone fires, top-down alone is silent, coincident
#       drives a burst.
# WHY:  The two-compartment integration is the substrate for dendritic
#       computation. BAC firing is the canonical biological phenotype.
# HOW:  C + libcheck (matches test_snn_per_receptor_integration.c style).
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_dendritic_integration.c)
    add_executable(integration_snn_test_snn_dendritic_integration
        test_snn_dendritic_integration.c
    )
    target_include_directories(integration_snn_test_snn_dendritic_integration
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/include/snn
    )
    target_compile_features(integration_snn_test_snn_dendritic_integration
        PRIVATE c_std_11)
    target_link_libraries(integration_snn_test_snn_dendritic_integration
        PRIVATE
            nimcp_snn
            nimcp
            Threads::Threads
            check_pic subunit rt m z
    )
    add_test(NAME integration_snn_test_snn_dendritic_integration
             COMMAND integration_snn_test_snn_dendritic_integration
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(integration_snn_test_snn_dendritic_integration
        PROPERTIES
            LABELS "integration;snn;dendritic;wave_h"
            TIMEOUT 60)
    message(STATUS "  Added SNN integration test: test_snn_dendritic_integration")
endif()

# ============================================================================
# Language adapter ↔ SNN substrate integration
# ============================================================================
# WHAT: Brings up a tiny SNN with broca_substrate / wernicke_substrate pops
#       and verifies broca/wernicke_attach_snn_pop binds cleanly + survives
#       SNN ticks.
# WHY:  Catches drift between init code's pop-name constants and adapter
#       expectations. Real SNN exercise (vs unit test which uses sentinel).
# HOW:  GoogleTest. Bypasses brain init for speed.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_lang_attach_integration.cpp)
    add_executable(integration_snn_test_lang_attach_integration
        test_lang_attach_integration.cpp
    )
    target_link_libraries(integration_snn_test_lang_attach_integration
        PRIVATE ${SNN_INTEGRATION_TEST_LIBS}
    )
    target_include_directories(integration_snn_test_lang_attach_integration
        PRIVATE ${SNN_INTEGRATION_TEST_INCLUDES}
    )
    target_compile_features(integration_snn_test_lang_attach_integration
        PRIVATE cxx_std_17)
    gtest_discover_tests(integration_snn_test_lang_attach_integration
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "integration;snn;language;adapter"
            TIMEOUT 120
    )
    message(STATUS "  Added SNN integration test: test_lang_attach_integration")
endif()

# ============================================================================
# Include Subdirectories
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/exception)
    add_subdirectory(exception)
endif()
