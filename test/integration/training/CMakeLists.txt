# CNN Training Integration Tests
# Tests for CNN training pipeline integration with other subsystems
# TODO: Prefer gtest_discover_tests over add_test for automatic test registration

# CNN Backprop Integration Tests
add_executable(integration_training_test_cnn_backprop_integration
    test_cnn_backprop_integration.cpp
)

target_include_directories(integration_training_test_cnn_backprop_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_training_test_cnn_backprop_integration PRIVATE cxx_std_20)

target_link_libraries(integration_training_test_cnn_backprop_integration
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_training_test_cnn_backprop_integration
         COMMAND integration_training_test_cnn_backprop_integration WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_training_test_cnn_backprop_integration
    PROPERTIES
        LABELS "integration;training;cnn;backprop;gradient"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Training Symbolic Logic Hub Bridge Integration Tests
add_executable(integration_training_symbolic_logic_hub
    test_training_symbolic_logic_hub_integration.cpp
)

target_include_directories(integration_training_symbolic_logic_hub PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_training_symbolic_logic_hub PRIVATE cxx_std_20)

target_link_libraries(integration_training_symbolic_logic_hub
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_training_symbolic_logic_hub
         COMMAND integration_training_symbolic_logic_hub WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_training_symbolic_logic_hub
    PROPERTIES
        LABELS "integration;training;symbolic-logic;hub"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# CNN-Cortex Bridge Integration Tests
add_executable(integration_training_test_cnn_cortex_integration
    test_cnn_cortex_integration.cpp
)

target_include_directories(integration_training_test_cnn_cortex_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_training_test_cnn_cortex_integration PRIVATE cxx_std_20)

target_link_libraries(integration_training_test_cnn_cortex_integration
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_training_test_cnn_cortex_integration
         COMMAND integration_training_test_cnn_cortex_integration WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_training_test_cnn_cortex_integration
    PROPERTIES
        LABELS "integration;training;cnn;cortex;bridge"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Training Dispatch Integration Tests (C)
add_executable(integration_training_dispatch
    test_training_dispatch_integration.c
)

target_include_directories(integration_training_dispatch PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_training_dispatch PRIVATE c_std_11)
target_compile_options(integration_training_dispatch PRIVATE
    -Wall
    -Wextra
)

target_link_libraries(integration_training_dispatch
    PRIVATE
        nimcp
        nimcp_middleware
        Threads::Threads
        m
)

add_test(NAME integration_training_dispatch
         COMMAND integration_training_dispatch WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_training_dispatch
    PROPERTIES
        LABELS "integration;training;dispatch;snn;lnn;cnn;adaptive"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Bio Learning Integration Tests (Engram, Mammillary, Neuromodulators, Glial, PR Memory)
add_executable(integration_training_bio_learning
    test_bio_learning_integration.cpp
)

target_include_directories(integration_training_bio_learning PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_training_bio_learning PRIVATE cxx_std_20)

target_link_libraries(integration_training_bio_learning
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_training_bio_learning
         COMMAND integration_training_bio_learning WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_training_bio_learning
    PROPERTIES
        LABELS "integration;training;bio;engram;mammillary;neuromodulator;glial"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Multi-Network Ensemble Training Integration Tests (LNN + CNN + Adaptive)
add_executable(integration_training_multi_network
    test_multi_network_training.cpp
)

target_include_directories(integration_training_multi_network PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_training_multi_network PRIVATE cxx_std_20)

target_link_libraries(integration_training_multi_network
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_training_multi_network
         COMMAND integration_training_multi_network WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_training_multi_network
    PROPERTIES
        LABELS "integration;training;multi-network;lnn;cnn;ensemble"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# VAE Training Integration Tests
add_executable(integration_training_vae
    test_vae_training_integration.cpp
)

target_include_directories(integration_training_vae PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_training_vae PRIVATE cxx_std_20)

target_link_libraries(integration_training_vae
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_training_vae
         COMMAND integration_training_vae WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_training_vae
    PROPERTIES
        LABELS "integration;training;vae;autoencoder;fep"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Attention Training Integration Tests
add_executable(integration_training_attention
    test_attention_training_integration.cpp
)

target_include_directories(integration_training_attention PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_training_attention PRIVATE cxx_std_20)

target_link_libraries(integration_training_attention
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_training_attention
         COMMAND integration_training_attention WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_training_attention
    PROPERTIES
        LABELS "integration;training;attention;plasticity;multihead"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# =============================================================================
# Cognitive Training Integration Tests (brain_ti_* facade)
# =============================================================================
# WHAT: Integration tests for the unified training-integration facade
# WHY:  Verify reward/RPE, habit, arousal/circadian, symbolic-logic, and
#       reasoning subsystems are correctly wired to a real brain instance
# HOW:  GTest executable linked against nimcp library
# =============================================================================

add_executable(integration_training_test_training_integration
    test_training_integration.cpp
)

target_link_libraries(integration_training_test_training_integration PRIVATE
    nimcp
    nimcp_middleware
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)

target_include_directories(integration_training_test_training_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/test
)

target_compile_features(integration_training_test_training_integration PRIVATE cxx_std_20)

gtest_discover_tests(integration_training_test_training_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;training;brain"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy"
)

# =============================================================================
# Bio Modulation Integration Tests
# =============================================================================
# WHAT: Tests that individual biological modulation subsystems correctly
#       affect the unified learning rate computation and compose multiplicatively
# WHY:  The unified continuous modulation pipeline composes 8+ factors; these
#       tests verify each factor's directional effect and composition
# HOW:  Creates brain_t instances, manipulates subsystem state, checks unified LR
#
# TESTS:
# 1. ArousalAffectsLR - Boosted arousal changes LR
# 2. InflammationFactorReasonable - Baseline inflammation factor
# 3. StressReducesLR - Stress penalty applied
# 4. AllModulatorsCompose - Factors compose multiplicatively
# 5. CognitiveCapacityAffectsLR - Capacity modulates LR
# 6. CircadianEfficiencyAffectsLR - Circadian phase modulates LR
# 7. RewardPredictionErrorModulatesLR - RPE changes LR
# 8. ModulationAfterPostBatchUpdate - Post-batch changes state
# 9. HabitFormationIntegration - Habit lifecycle
# 10. ReasoningIntegrationWithModulation - Reasoning + modulation
# 11-14. Portia, Instability, Consistency, ShouldPause
# =============================================================================

add_executable(integration_training_bio_modulation
    test_bio_modulation_integration.cpp
)

target_link_libraries(integration_training_bio_modulation PRIVATE
    nimcp
    nimcp_middleware
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)

target_include_directories(integration_training_bio_modulation PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/test
)

target_compile_features(integration_training_bio_modulation PRIVATE cxx_std_20)

gtest_discover_tests(integration_training_bio_modulation
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;training;bio;modulation;arousal;inflammation;stress"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy"
)

# =============================================================================
# SNN Backprop Integration Tests
# =============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_backprop_integration.cpp)
    add_executable(integration_training_snn_backprop
        test_snn_backprop_integration.cpp
    )
    target_include_directories(integration_training_snn_backprop PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_compile_features(integration_training_snn_backprop PRIVATE cxx_std_20)
    target_link_libraries(integration_training_snn_backprop PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME integration_training_snn_backprop
             COMMAND integration_training_snn_backprop
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(integration_training_snn_backprop
        PROPERTIES
            LABELS "integration;training;snn;backprop"
            TIMEOUT 300
            RESOURCE_LOCK "brain_heavy")
endif()

# =============================================================================
# Cortex CNN Thalamic Compose Integration Tests
# =============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_cortex_cnn_thalamic_compose.cpp)
    add_executable(integration_training_test_cortex_cnn_thalamic_compose
        test_cortex_cnn_thalamic_compose.cpp
    )
    target_include_directories(integration_training_test_cortex_cnn_thalamic_compose PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_compile_features(integration_training_test_cortex_cnn_thalamic_compose PRIVATE cxx_std_20)
    target_link_libraries(integration_training_test_cortex_cnn_thalamic_compose PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME integration_training_test_cortex_cnn_thalamic_compose
             COMMAND integration_training_test_cortex_cnn_thalamic_compose
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(integration_training_test_cortex_cnn_thalamic_compose
        PROPERTIES
            LABELS "integration;training;cnn;cortex;thalamic;compose"
            TIMEOUT 180
            RESOURCE_LOCK "brain_heavy")
endif()

# =============================================================================
# Cortex CNN Substrate Adapter Integration Tests (Phase 3)
# =============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_cortex_cnn_substrate_integration.cpp)
    add_executable(integration_training_test_cortex_cnn_substrate_integration
        test_cortex_cnn_substrate_integration.cpp
    )
    target_include_directories(integration_training_test_cortex_cnn_substrate_integration PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_compile_features(integration_training_test_cortex_cnn_substrate_integration PRIVATE cxx_std_20)
    target_link_libraries(integration_training_test_cortex_cnn_substrate_integration PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME integration_training_test_cortex_cnn_substrate_integration
             COMMAND integration_training_test_cortex_cnn_substrate_integration
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(integration_training_test_cortex_cnn_substrate_integration
        PROPERTIES
            LABELS "integration;training;cnn;cortex;substrate"
            TIMEOUT 300
            RESOURCE_LOCK "brain_heavy")
endif()

# =============================================================================
# FNO + HNN Substrate/Thalamic Compose Integration Tests
# =============================================================================
# WHAT: Verifies that FNO (embedded in cortex_cnn_processor_t) and HNN
#       (embedded in lnn_layer_t) observe biological substrate modulation
#       and thalamic attention gating through their parent network
#       composition — without any FNO/HNN-specific adapter work.
# WHY:  FNO and HNN are not standalone networks. Their parent CNN/LNN run
#       the substrate + thalamic pipelines, and the FNO/HNN outputs flow
#       through those pipelines for free. This is the composition that
#       replaces the originally-planned Phase 4 P4B adapter work.
# =============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_fno_hnn_substrate_compose.cpp)
    add_executable(integration_training_test_fno_hnn_substrate_compose
        test_fno_hnn_substrate_compose.cpp
    )
    target_include_directories(integration_training_test_fno_hnn_substrate_compose PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_compile_features(integration_training_test_fno_hnn_substrate_compose PRIVATE cxx_std_20)
    target_link_libraries(integration_training_test_fno_hnn_substrate_compose PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME integration_training_test_fno_hnn_substrate_compose
             COMMAND integration_training_test_fno_hnn_substrate_compose
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(integration_training_test_fno_hnn_substrate_compose
        PROPERTIES LABELS "integration;training;fno;hnn;substrate;thalamic;compose"
                   TIMEOUT 300
                   RESOURCE_LOCK "brain_heavy")
endif()

# Training Exception Handling Integration Tests
add_subdirectory(exception)
