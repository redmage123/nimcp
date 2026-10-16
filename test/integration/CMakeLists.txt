# Integration tests
# This CMakeLists.txt exists to prevent auto-discovery of problematic top-level tests
# The test/*.cpp files at this level have significant API mismatches

message(STATUS "  [SKIP] Integration top-level tests - API has changed significantly")
message(STATUS "    - test_brain_factory_integration.cpp")
message(STATUS "    - test_cross_bridge_integration.cpp")
message(STATUS "    - test_thalamic_bridge_integration.cpp")
message(STATUS "    - test_quantum_bridge_integration.cpp")

# Subdirectories with their own CMakeLists.txt are now processed by parent
# test/CMakeLists.txt via direct add_subdirectory() calls

# =============================================================================
# GPU/CPU Fallback Integration Tests
# =============================================================================
# These tests verify GPU detection, execution mode switching, and CPU fallback

macro(add_integration_gpu_test TEST_NAME)
    add_executable(integration_gpu_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(integration_gpu_${TEST_NAME}
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            nimcp
            m
            pthread
    )
    target_compile_features(integration_gpu_${TEST_NAME} PRIVATE cxx_std_20)
    add_test(NAME integration_gpu_${TEST_NAME}
             COMMAND integration_gpu_${TEST_NAME})
endmacro()

# GPU CPU fallback integration tests
add_integration_gpu_test(test_gpu_cpu_fallback_integration)

# =============================================================================
# Wernicke Integration Tests
# =============================================================================
# Note: wernicke subdirectory is added by test/CMakeLists.txt

# =============================================================================
# Language Layer Integration Tests
# =============================================================================
# Note: language subdirectory is added by test/CMakeLists.txt

# =============================================================================
# Brain KG Integration Tests
# =============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/brain/CMakeLists.txt)
    message(STATUS "Adding Brain KG Integration tests...")
    add_subdirectory(brain)
endif()

# =============================================================================
# Exception Integration Tests
# =============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/exception/CMakeLists.txt)
    message(STATUS "Adding Exception Integration tests...")
    add_subdirectory(exception)
endif()

# =============================================================================
# Core Integration Tests (neural_substrate, substrate, axon, dendrite, etc.)
# =============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/core/CMakeLists.txt)
    message(STATUS "Adding Core Integration tests...")
    add_subdirectory(core)
endif()

# =============================================================================
# Statistics Integration Tests
# =============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/statistics/CMakeLists.txt)
    message(STATUS "Adding Statistics Integration tests...")
    add_subdirectory(statistics)
endif()

# =============================================================================
# Reasoning-Brain Integration Tests
# =============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/reasoning/CMakeLists.txt)
    message(STATUS "Adding Reasoning-Brain Integration tests...")
    add_subdirectory(reasoning)
endif()

# =============================================================================
# Generation Pipeline Integration Tests
# =============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/generation/CMakeLists.txt)
    message(STATUS "Adding Generation Pipeline Integration tests...")
    add_subdirectory(generation)
endif()

# =============================================================================
# Backprop Gradient Clipping Integration Tests
# =============================================================================
# WHAT: Integration tests for gradient clipping in backprop kernel + adaptive network
# WHY:  Athena 95% accuracy plan added max_grad_norm/out_grad_norm to backprop_sparse_full()
# HOW:  Test end-to-end gradient norm tracking, bounding, and convergence

add_executable(integration_test_backprop_grad_clip
    test_backprop_gradient_clipping_integration.cpp
)
target_link_libraries(integration_test_backprop_grad_clip
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(integration_test_backprop_grad_clip PRIVATE cxx_std_20)
gtest_discover_tests(integration_test_backprop_grad_clip
    PROPERTIES LABELS "integration;plasticity;backprop;gradient_clipping"
    TIMEOUT 120
)

# =============================================================================
# Training Integration Tests
# =============================================================================
# Note: training subdirectory is added by test/CMakeLists.txt

# =============================================================================
# Brain Subsystem Integration Tests (Check Framework, C)
# =============================================================================
# WHAT: Integration tests for new brain subsystems (glymphatic, ECS, neuropeptide,
#       cortical interneurons, white matter, inferior colliculus, spinal cord)
# WHY:  Verify cross-module interactions for biologically-inspired subsystems
# HOW:  Check framework tests with proper fixtures and lifecycle management

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(CHECK_LIB check)
endif()

if(CHECK_LIB_FOUND)
    message(STATUS "Adding brain subsystem integration tests (Check framework)...")

    # Macro for Check-based C integration tests
    macro(add_check_integration_test TEST_TARGET TEST_SOURCE)
        add_executable(${TEST_TARGET} ${TEST_SOURCE})
        target_include_directories(${TEST_TARGET} PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CHECK_LIB_INCLUDE_DIRS}
        )
        target_link_libraries(${TEST_TARGET}
            nimcp
            ${CHECK_LIB_LIBRARIES}
            pthread
            m
        )
        add_test(NAME ${TEST_TARGET} COMMAND ${TEST_TARGET})
        set_tests_properties(${TEST_TARGET} PROPERTIES
            LABELS "integration;brain_subsystems"
            TIMEOUT 120
        )
    endmacro()

    # 1. Glymphatic + Sleep Integration
    add_check_integration_test(
        integration_glymphatic_sleep
        test_glymphatic_sleep_integration.c
    )

    # 2. Endocannabinoid + STDP Integration
    add_check_integration_test(
        integration_ecb_stdp
        test_ecb_stdp_integration.c
    )

    # 3. Neuropeptide + Hypothalamus Integration
    add_check_integration_test(
        integration_neuropeptide_hypothalamus
        test_neuropeptide_hypothalamus_integration.c
    )

    # 4. Cortical Interneurons + Columns Integration
    add_check_integration_test(
        integration_cortical_interneurons_columns
        test_cortical_interneurons_columns_integration.c
    )

    # 5. White Matter + Thalamic Integration
    add_check_integration_test(
        integration_white_matter_thalamic
        test_white_matter_thalamic_integration.c
    )

    # 6. Inferior Colliculus + Audio Integration
    add_check_integration_test(
        integration_ic_audio
        test_ic_audio_integration.c
    )

    # 7. Spinal Cord + Motor Integration
    add_check_integration_test(
        integration_spinal_motor
        test_spinal_motor_integration.c
    )

    # 8. Parallel Init Subsystem Integration
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_parallel_init_subsystem_integration.c)
        add_executable(integration_parallel_init test_parallel_init_subsystem_integration.c)
        target_include_directories(integration_parallel_init PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CHECK_LIB_INCLUDE_DIRS}
        )
        target_link_libraries(integration_parallel_init
            nimcp ${CHECK_LIB_LIBRARIES} pthread m
        )
        add_test(NAME integration_parallel_init COMMAND integration_parallel_init)
        set_tests_properties(integration_parallel_init PROPERTIES
            LABELS "integration;brain_subsystems;parallel_init"
            TIMEOUT 300
            RESOURCE_LOCK "brain_heavy"
        )
    endif()

    # 9. Bridge Base Lifecycle (verifies bridge_base_init/cleanup in all bridges)
    add_check_integration_test(
        integration_bridge_base_lifecycle
        test_bridge_base_lifecycle_integration.c
    )
    set_tests_properties(integration_bridge_base_lifecycle PROPERTIES
        RESOURCE_LOCK "brain_heavy"
        TIMEOUT 600
    )

    # 10. SNN Per-Receptor Integration (P0 — AMPA/NMDA/GABA_A/GABA_B routing)
    # Lives in snn/ subdir but uses Check framework + nimcp_snn, so registered
    # here where the Check infrastructure is set up. nimcp_snn is required
    # because synapse_type_per_src and the per-receptor deposit kernel live
    # in libnimcp_snn.a.
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/snn/test_snn_per_receptor_integration.c)
        add_executable(integration_snn_per_receptor
            snn/test_snn_per_receptor_integration.c
        )
        target_include_directories(integration_snn_per_receptor PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/include/snn
            ${CHECK_LIB_INCLUDE_DIRS}
        )
        target_link_libraries(integration_snn_per_receptor
            nimcp_snn
            nimcp
            ${CHECK_LIB_LIBRARIES}
            pthread
            m
            z
        )
        add_test(NAME integration_snn_per_receptor
                 COMMAND integration_snn_per_receptor)
        set_tests_properties(integration_snn_per_receptor PROPERTIES
            LABELS "integration;snn;per_receptor;cb"
            TIMEOUT 180
        )
        message(STATUS "  Added SNN per-receptor integration test")
    endif()

    # P1.1 — top-down NMDA feedback projections (descending L5→L3, L6→L2).
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/snn/test_top_down_feedback.c)
        add_executable(integration_snn_top_down
            snn/test_top_down_feedback.c
        )
        target_include_directories(integration_snn_top_down PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/include/snn
            ${CHECK_LIB_INCLUDE_DIRS}
        )
        target_link_libraries(integration_snn_top_down
            nimcp_snn
            nimcp
            ${CHECK_LIB_LIBRARIES}
            pthread
            m
            z
        )
        add_test(NAME integration_snn_top_down
                 COMMAND integration_snn_top_down)
        set_tests_properties(integration_snn_top_down PROPERTIES
            LABELS "integration;snn;top_down;p1.1"
            TIMEOUT 900
        )
        message(STATUS "  Added SNN top-down feedback (P1.1) integration test")
    endif()

    # P3.1 — reticular thalamic nucleus (TRN gating).
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/snn/test_trn_gating.c)
        add_executable(integration_snn_trn
            snn/test_trn_gating.c
        )
        target_include_directories(integration_snn_trn PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/include/snn
            ${CHECK_LIB_INCLUDE_DIRS}
        )
        target_link_libraries(integration_snn_trn
            nimcp_snn
            nimcp
            ${CHECK_LIB_LIBRARIES}
            pthread
            m
            z
        )
        add_test(NAME integration_snn_trn
                 COMMAND integration_snn_trn)
        set_tests_properties(integration_snn_trn PROPERTIES
            LABELS "integration;snn;trn;p3.1"
            TIMEOUT 900
        )
        message(STATUS "  Added SNN reticular thalamic nucleus (P3.1) integration test")
    endif()

    # P2.2 — PV/SOM/VIP interneuron disinhibition chain.
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/snn/test_pv_som_vip_disinhibition.c)
        add_executable(integration_snn_pv_som_vip
            snn/test_pv_som_vip_disinhibition.c
        )
        target_include_directories(integration_snn_pv_som_vip PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/include/snn
            ${CHECK_LIB_INCLUDE_DIRS}
        )
        target_link_libraries(integration_snn_pv_som_vip
            nimcp_snn
            nimcp
            ${CHECK_LIB_LIBRARIES}
            pthread
            m
            z
        )
        add_test(NAME integration_snn_pv_som_vip
                 COMMAND integration_snn_pv_som_vip)
        set_tests_properties(integration_snn_pv_som_vip PROPERTIES
            LABELS "integration;snn;disinhibition;p2.2"
            TIMEOUT 900
        )
        message(STATUS "  Added SNN PV/SOM/VIP disinhibition (P2.2) integration test")
    endif()

    # Wave F task #263 — substrate skip-path extension (3 new SKIP_DEFS).
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/snn/test_snn_skip_topology.c)
        add_executable(integration_snn_skip_topology
            snn/test_snn_skip_topology.c
        )
        target_include_directories(integration_snn_skip_topology PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/include/snn
            ${CHECK_LIB_INCLUDE_DIRS}
        )
        target_link_libraries(integration_snn_skip_topology
            nimcp_snn
            nimcp
            ${CHECK_LIB_LIBRARIES}
            pthread
            m
            z
        )
        add_test(NAME integration_snn_skip_topology
                 COMMAND integration_snn_skip_topology)
        set_tests_properties(integration_snn_skip_topology PROPERTIES
            LABELS "integration;snn;skip;wave_f"
            TIMEOUT 900
        )
        message(STATUS "  Added SNN cortico-cortical skip topology (Wave F) integration test")
    endif()

    # Wave C task #260 — Dale's principle audit for full hierarchical SNN.
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/snn/test_snn_dale_integration.c)
        add_executable(integration_snn_dale
            snn/test_snn_dale_integration.c
        )
        target_include_directories(integration_snn_dale PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/include/snn
            ${CHECK_LIB_INCLUDE_DIRS}
        )
        target_link_libraries(integration_snn_dale
            nimcp_snn
            nimcp
            ${CHECK_LIB_LIBRARIES}
            pthread
            m
            z
        )
        add_test(NAME integration_snn_dale
                 COMMAND integration_snn_dale)
        set_tests_properties(integration_snn_dale PROPERTIES
            LABELS "integration;snn;dale;wave_c"
            TIMEOUT 900
        )
        message(STATUS "  Added SNN Dale's principle (Wave C) integration test")
    endif()

    # P4.1 — layer-specific pyramidal subtypes (L23 / L4_STELLATE / L5_BETZ).
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/snn/test_layer_subtypes_wiring.c)
        add_executable(integration_snn_layer_subtypes
            snn/test_layer_subtypes_wiring.c
        )
        target_include_directories(integration_snn_layer_subtypes PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/include/snn
            ${CHECK_LIB_INCLUDE_DIRS}
        )
        target_link_libraries(integration_snn_layer_subtypes
            nimcp_snn
            nimcp
            ${CHECK_LIB_LIBRARIES}
            pthread
            m
            z
        )
        add_test(NAME integration_snn_layer_subtypes
                 COMMAND integration_snn_layer_subtypes)
        set_tests_properties(integration_snn_layer_subtypes PROPERTIES
            LABELS "integration;snn;layer_subtypes;p4.1"
            TIMEOUT 900
        )
        message(STATUS "  Added SNN layer-specific subtypes (P4.1) integration test")
    endif()

    # Wave D — PV gap-junction (Connexin-36) electrical coupling.
    # Verifies the per-pop gap-junction term in the SNN hot loop propagates
    # voltage from one driven neuron to the rest of the syncytium under CB
    # mode. Uses libcheck for parity with the other PV/SOM/VIP tests.
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/snn/test_snn_gap_junction_integration.c)
        add_executable(integration_snn_gap_junction
            snn/test_snn_gap_junction_integration.c
        )
        target_include_directories(integration_snn_gap_junction PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/include/snn
            ${CHECK_LIB_INCLUDE_DIRS}
        )
        target_link_libraries(integration_snn_gap_junction
            nimcp_snn
            nimcp
            ${CHECK_LIB_LIBRARIES}
            pthread
            m
            z
        )
        add_test(NAME integration_snn_gap_junction
                 COMMAND integration_snn_gap_junction)
        set_tests_properties(integration_snn_gap_junction PROPERTIES
            LABELS "integration;snn;gap_junction;cb;wave_d"
            TIMEOUT 180
        )
        message(STATUS "  Added SNN gap-junction (Wave D) integration test")
    endif()

    # Wave E FFI fix — per-pop conduction delay (spike-history ring buffer).
    # Verifies the deposit kernel's delayed-spike read produces measurable
    # timing offsets in the SNN hot loop. Uses libcheck for parity with
    # the gap-junction integration test.
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/snn/test_snn_conduction_delay_integration.c)
        add_executable(integration_snn_conduction_delay
            snn/test_snn_conduction_delay_integration.c
        )
        target_include_directories(integration_snn_conduction_delay PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/include/snn
            ${CHECK_LIB_INCLUDE_DIRS}
        )
        target_link_libraries(integration_snn_conduction_delay
            nimcp_snn
            nimcp
            ${CHECK_LIB_LIBRARIES}
            pthread
            m
            z
        )
        add_test(NAME integration_snn_conduction_delay
                 COMMAND integration_snn_conduction_delay)
        set_tests_properties(integration_snn_conduction_delay PROPERTIES
            LABELS "integration;snn;conduction_delay;ffi;wave_e"
            TIMEOUT 180
        )
        message(STATUS "  Added SNN conduction-delay (Wave E) integration test")
    endif()

    # Wave G — per-neuron LIF heterogeneity.
    # Verifies σ=0.2 produces measurably higher first-spike-time variance
    # than σ=0 under matched drive — i.e. heterogeneity desynchronises
    # firing instead of merely allocating buffers. Uses libcheck for parity
    # with the other SNN biophysical integration tests.
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/snn/test_snn_heterogeneity_integration.c)
        add_executable(integration_snn_heterogeneity
            snn/test_snn_heterogeneity_integration.c
        )
        target_include_directories(integration_snn_heterogeneity PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/include/snn
            ${CHECK_LIB_INCLUDE_DIRS}
        )
        target_link_libraries(integration_snn_heterogeneity
            nimcp_snn
            nimcp
            ${CHECK_LIB_LIBRARIES}
            pthread
            m
            z
        )
        add_test(NAME integration_snn_heterogeneity
                 COMMAND integration_snn_heterogeneity)
        set_tests_properties(integration_snn_heterogeneity PROPERTIES
            LABELS "integration;snn;heterogeneity;wave_g"
            TIMEOUT 180
        )
        message(STATUS "  Added SNN heterogeneity (Wave G) integration test")
    endif()

else()
    message(STATUS "  [SKIP] Brain subsystem integration tests - Check library not found")
endif()

