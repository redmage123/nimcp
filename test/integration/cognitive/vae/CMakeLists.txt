# ============================================================================
# VAE Integration Tests (Check Framework)
# ============================================================================
# WHAT: Build configuration for VAE-FEP bridge integration tests
# WHY:  Test interactions between VAE and FEP systems via bridge
# HOW:  Single executable with Check framework tests

find_package(PkgConfig REQUIRED)
pkg_check_modules(CHECK check)

if(CHECK_FOUND)
    # DISABLED: Test needs API updates to match current VAE/tensor API
    # TODO: Update vae_config_t, vae_loss_t, NIMCP_DTYPE_* usage
    #
    # #=========================================================================
    # # VAE-FEP Bridge Integration Tests
    # # Tests latent-belief mapping, free energy sharing, precision sync,
    # # and prediction error propagation between VAE and FEP systems
    # #=========================================================================
    #
    # add_executable(integration_vae_fep
    #     test_vae_fep_integration.c
    # )
    # target_include_directories(integration_vae_fep PRIVATE
    #     ${CMAKE_SOURCE_DIR}/include
    #     ${CHECK_INCLUDE_DIRS}
    # )
    # target_link_libraries(integration_vae_fep
    #     nimcp
    #     ${CHECK_LIBRARIES}
    #     pthread
    #     m
    # )
    # add_test(NAME integration_vae_fep
    #          COMMAND integration_vae_fep)
    # set_tests_properties(integration_vae_fep PROPERTIES
    #     LABELS "integration;cognitive;vae;fep;bridge;latent;belief"
    #     TIMEOUT 300
    # )

    # DISABLED: Test needs API updates to match current vae_loss_t/tensor API
    # TODO: Update vae_loss_t field names and tensor access patterns
    #
    # #=========================================================================
    # # VAE System Integration Tests
    # # Tests immune bridge, BBB bridge, and logging bridge integrations
    # # for anomaly detection, security validation, and operational logging
    # #=========================================================================
    #
    # add_executable(integration_vae_system
    #     test_vae_system_integration.c
    # )
    # target_include_directories(integration_vae_system PRIVATE
    #     ${CMAKE_SOURCE_DIR}/include
    #     ${CHECK_INCLUDE_DIRS}
    # )
    # target_link_libraries(integration_vae_system
    #     nimcp
    #     ${CHECK_LIBRARIES}
    #     pthread
    #     m
    # )
    # add_test(NAME integration_vae_system
    #          COMMAND integration_vae_system)
    # set_tests_properties(integration_vae_system PROPERTIES
    #     LABELS "integration;cognitive;vae;immune;bbb;logging;bridge"
    #     TIMEOUT 300
    # )

    # DISABLED: Test needs API updates to match current VAE auditory bridge API
    # TODO: Update vae_auditory_* function names to vae_audio_* pattern
    #
    # #=========================================================================
    # # VAE Cognitive Bridge Integration Tests
    # # Tests hippocampus, imagination, visual, auditory, emotion, introspection,
    # # and world model bridges for cognitive integration
    # #=========================================================================
    #
    # add_executable(integration_vae_cognitive
    #     test_vae_cognitive_integration.c
    # )
    # target_include_directories(integration_vae_cognitive PRIVATE
    #     ${CMAKE_SOURCE_DIR}/include
    #     ${CHECK_INCLUDE_DIRS}
    # )
    # target_link_libraries(integration_vae_cognitive
    #     nimcp
    #     ${CHECK_LIBRARIES}
    #     pthread
    #     m
    # )
    # add_test(NAME integration_vae_cognitive
    #          COMMAND integration_vae_cognitive)
    # set_tests_properties(integration_vae_cognitive PROPERTIES
    #     LABELS "integration;cognitive;vae;hippocampus;imagination;visual;auditory;emotion;introspection;worldmodel"
    #     TIMEOUT 300
    # )

    # DISABLED: Test needs API updates to match current VAE config structure
    # TODO: Update vae_config_t field names
    #
    # #=========================================================================
    # # VAE Neural Integration Tests (Phase 5)
    # # Tests SNN, plasticity, training, substrate, and thalamic bridges
    # # for neural integration pathways
    # #=========================================================================
    #
    # add_executable(integration_vae_neural
    #     test_vae_neural_integration.c
    # )
    # target_include_directories(integration_vae_neural PRIVATE
    #     ${CMAKE_SOURCE_DIR}/include
    #     ${CHECK_INCLUDE_DIRS}
    # )
    # target_link_libraries(integration_vae_neural
    #     nimcp
    #     ${CHECK_LIBRARIES}
    #     pthread
    #     m
    # )
    # add_test(NAME integration_vae_neural
    #          COMMAND integration_vae_neural)
    # set_tests_properties(integration_vae_neural PROPERTIES
    #     LABELS "integration;cognitive;vae;snn;plasticity;training;substrate;thalamic;neural"
    #     TIMEOUT 300
    # )

    #=========================================================================
    # VAE Bio-Async Integration Tests (Phase 6)
    # Tests bio-async message handling, handler registration, message sending,
    # and inter-bridge communication via the bio-router infrastructure
    #=========================================================================

    add_executable(integration_vae_bio_async
        test_vae_bio_async_integration.c
    )
    target_include_directories(integration_vae_bio_async PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CHECK_INCLUDE_DIRS}
    )
    target_link_libraries(integration_vae_bio_async
        nimcp
        ${CHECK_LIBRARIES}
        pthread
        m
    )
    add_test(NAME integration_vae_bio_async
             COMMAND integration_vae_bio_async)
    set_tests_properties(integration_vae_bio_async PROPERTIES
        LABELS "integration;cognitive;vae;bio-async;messaging;handlers"
        TIMEOUT 300
    )

    #=========================================================================
    # VAE Regression Tests (Phase 6)
    # Comprehensive regression tests ensuring VAE system stability
    # across all modules and integration points
    #=========================================================================

    add_executable(integration_vae_regression
        test_vae_regression.c
    )
    target_include_directories(integration_vae_regression PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CHECK_INCLUDE_DIRS}
    )
    target_link_libraries(integration_vae_regression
        nimcp
        ${CHECK_LIBRARIES}
        pthread
        m
    )
    add_test(NAME integration_vae_regression
             COMMAND integration_vae_regression)
    set_tests_properties(integration_vae_regression PROPERTIES
        LABELS "integration;cognitive;vae;regression;stability"
        TIMEOUT 600
    )

    #=========================================================================
    # VAE End-to-End Tests (Phase 6)
    # Complete workflow tests simulating real-world usage patterns
    # including perception, active inference, memory, and learning cycles
    #=========================================================================

    add_executable(integration_vae_e2e
        test_vae_e2e.c
    )
    target_include_directories(integration_vae_e2e PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CHECK_INCLUDE_DIRS}
    )
    target_link_libraries(integration_vae_e2e
        nimcp
        ${CHECK_LIBRARIES}
        pthread
        m
    )
    add_test(NAME integration_vae_e2e
             COMMAND integration_vae_e2e)
    set_tests_properties(integration_vae_e2e PROPERTIES
        LABELS "integration;cognitive;vae;e2e;workflow;perception;inference"
        TIMEOUT 900
    )

    #=========================================================================
    # Aggregate Target
    #=========================================================================

    add_custom_target(integration_vae_all DEPENDS
        # integration_vae_fep       # DISABLED - needs API updates
        # integration_vae_system    # DISABLED - needs API updates
        # integration_vae_cognitive # DISABLED - needs API updates
        # integration_vae_neural    # DISABLED - needs API updates
        integration_vae_bio_async
        integration_vae_regression
        integration_vae_e2e
    )

else()
    message(STATUS "Check library not found - skipping VAE integration tests")
endif()
