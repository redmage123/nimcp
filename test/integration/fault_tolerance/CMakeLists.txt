# Fault Tolerance Integration Tests

# Main Fault Tolerance Integration Tests
add_executable(integration_fault_tolerance
    test_fault_tolerance_integration.cpp
)
target_link_libraries(integration_fault_tolerance
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
)
target_include_directories(integration_fault_tolerance
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
)
add_test(NAME integration_fault_tolerance
         COMMAND integration_fault_tolerance)
set_tests_properties(integration_fault_tolerance PROPERTIES
    LABELS "integration;fault_tolerance"
    TIMEOUT 120
)

# Self-Repair Integration Tests
add_executable(integration_self_repair
    test_self_repair_integration.cpp
)
target_link_libraries(integration_self_repair
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
)
target_include_directories(integration_self_repair
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
)
add_test(NAME integration_self_repair
         COMMAND integration_self_repair)
set_tests_properties(integration_self_repair PROPERTIES
    LABELS "integration;fault_tolerance;self-repair"
    TIMEOUT 180
)

# Self-Repair Bio-Async Integration Tests
add_executable(integration_self_repair_bio_async
    test_self_repair_bio_async_integration.cpp
)
target_link_libraries(integration_self_repair_bio_async
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
)
target_include_directories(integration_self_repair_bio_async
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)
add_test(NAME integration_self_repair_bio_async
         COMMAND integration_self_repair_bio_async)
set_tests_properties(integration_self_repair_bio_async PROPERTIES
    LABELS "integration;fault_tolerance;self-repair;bio-async"
    TIMEOUT 120
)

# DISABLED: Test needs health agent setter implementations
# TODO: Implement dist_set_health_agent, adv_set_health_agent, hpo_set_health_agent,
#       meta_set_health_agent, qa_set_health_agent functions
#
# # Health Module Integration Tests (Phase 8: System-Wide Health Integration)
# add_executable(integration_health_module
#     test_health_module_integration.cpp
# )
# target_link_libraries(integration_health_module
#     PRIVATE
#         GTest::GTest
#         GTest::Main
#         nimcp
#         Threads::Threads
# )
# target_include_directories(integration_health_module
#     PRIVATE
#         ${CMAKE_SOURCE_DIR}/include
#         ${CMAKE_SOURCE_DIR}/src
# )
# add_test(NAME integration_health_module
#          COMMAND integration_health_module)
# set_tests_properties(integration_health_module PROPERTIES
#     LABELS "integration;fault_tolerance;health;phase8"
#     TIMEOUT 180
# )

# Heartbeat B01 Memory Core Integration Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(integration_heartbeat_B01_memory_core
    test_heartbeat_B01_memory_core_integration.cpp
)
target_link_libraries(integration_heartbeat_B01_memory_core
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
)
target_include_directories(integration_heartbeat_B01_memory_core
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_heartbeat_B01_memory_core
         COMMAND integration_heartbeat_B01_memory_core)
set_tests_properties(integration_heartbeat_B01_memory_core PROPERTIES
    LABELS "integration;fault_tolerance;heartbeat;B01;memory_core"
    TIMEOUT 180
)

# Heartbeat B02 Memory (non-core) Integration Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(integration_heartbeat_B02_memory
    test_heartbeat_B02_memory_integration.cpp
)
target_link_libraries(integration_heartbeat_B02_memory
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
)
target_include_directories(integration_heartbeat_B02_memory
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_heartbeat_B02_memory
         COMMAND integration_heartbeat_B02_memory)
set_tests_properties(integration_heartbeat_B02_memory PROPERTIES
    LABELS "integration;fault_tolerance;heartbeat;B02;memory"
    TIMEOUT 180
)

# Heartbeat B03 Immune Integration Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(integration_heartbeat_B03_immune
    test_heartbeat_B03_immune_integration.cpp
)
target_link_libraries(integration_heartbeat_B03_immune
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
)
target_include_directories(integration_heartbeat_B03_immune
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_heartbeat_B03_immune
         COMMAND integration_heartbeat_B03_immune)
set_tests_properties(integration_heartbeat_B03_immune PROPERTIES
    LABELS "integration;fault_tolerance;heartbeat;B03;immune"
    TIMEOUT 180
)

# Heartbeat B04 Parietal Integration Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(integration_heartbeat_B04_parietal
    test_heartbeat_B04_parietal_integration.cpp
)
target_link_libraries(integration_heartbeat_B04_parietal
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
)
target_include_directories(integration_heartbeat_B04_parietal
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_heartbeat_B04_parietal
         COMMAND integration_heartbeat_B04_parietal)
set_tests_properties(integration_heartbeat_B04_parietal PROPERTIES
    LABELS "integration;fault_tolerance;heartbeat;B04;parietal"
    TIMEOUT 180
)

# Heartbeat B05 Mirror Neurons Integration Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(integration_heartbeat_B05_mirror_neurons
    test_heartbeat_B05_mirror_neurons_integration.cpp
)
target_link_libraries(integration_heartbeat_B05_mirror_neurons
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
)
target_include_directories(integration_heartbeat_B05_mirror_neurons
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_heartbeat_B05_mirror_neurons
         COMMAND integration_heartbeat_B05_mirror_neurons)
set_tests_properties(integration_heartbeat_B05_mirror_neurons PROPERTIES
    LABELS "integration;fault_tolerance;heartbeat;B05;mirror_neurons"
    TIMEOUT 180
)

# Heartbeat B06 Integration Integration Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(integration_heartbeat_B06_integration
    test_heartbeat_B06_integration_integration.cpp
)
target_link_libraries(integration_heartbeat_B06_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
)
target_include_directories(integration_heartbeat_B06_integration
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_heartbeat_B06_integration
         COMMAND integration_heartbeat_B06_integration)
set_tests_properties(integration_heartbeat_B06_integration PROPERTIES
    LABELS "integration;fault_tolerance;heartbeat;B06;integration"
    TIMEOUT 180
)

# Heartbeat B07 Game Theory Integration Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(integration_heartbeat_B07_game_theory
    test_heartbeat_B07_game_theory_integration.cpp
)
target_link_libraries(integration_heartbeat_B07_game_theory
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
)
target_include_directories(integration_heartbeat_B07_game_theory
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_heartbeat_B07_game_theory
         COMMAND integration_heartbeat_B07_game_theory)
set_tests_properties(integration_heartbeat_B07_game_theory PROPERTIES
    LABELS "integration;fault_tolerance;heartbeat;B07;game_theory"
    TIMEOUT 180
)

# Heartbeat B08 Ethics/Reasoning Integration Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(integration_heartbeat_B08_ethics_reasoning
    test_heartbeat_B08_ethics_reasoning_integration.cpp
)
target_link_libraries(integration_heartbeat_B08_ethics_reasoning
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
)
target_include_directories(integration_heartbeat_B08_ethics_reasoning
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_heartbeat_B08_ethics_reasoning
         COMMAND integration_heartbeat_B08_ethics_reasoning)
set_tests_properties(integration_heartbeat_B08_ethics_reasoning PROPERTIES
    LABELS "integration;fault_tolerance;heartbeat;B08;ethics_reasoning"
    TIMEOUT 180
)

# Heartbeat B09 Omni/Recursive Integration Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(integration_heartbeat_B09_omni_recursive
    test_heartbeat_B09_omni_recursive_integration.cpp
)
target_link_libraries(integration_heartbeat_B09_omni_recursive
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
)
target_include_directories(integration_heartbeat_B09_omni_recursive
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_heartbeat_B09_omni_recursive
         COMMAND integration_heartbeat_B09_omni_recursive)
set_tests_properties(integration_heartbeat_B09_omni_recursive PROPERTIES
    LABELS "integration;fault_tolerance;heartbeat;B09;omni_recursive"
    TIMEOUT 180
)

# Heartbeat B10 FEP/FaultTolerance Integration Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(integration_heartbeat_B10_fep_ft
    test_heartbeat_B10_fep_ft_integration.cpp
)
target_link_libraries(integration_heartbeat_B10_fep_ft
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
)
target_include_directories(integration_heartbeat_B10_fep_ft
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_heartbeat_B10_fep_ft
         COMMAND integration_heartbeat_B10_fep_ft)
set_tests_properties(integration_heartbeat_B10_fep_ft PROPERTIES
    LABELS "integration;fault_tolerance;heartbeat;B10;fep_ft"
    TIMEOUT 180
)

# Heartbeat B11 Wellbeing/Introspection Integration Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(integration_heartbeat_B11_wellbeing_introspection
    test_heartbeat_B11_wellbeing_introspection_integration.cpp
)
target_link_libraries(integration_heartbeat_B11_wellbeing_introspection
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
)
target_include_directories(integration_heartbeat_B11_wellbeing_introspection
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_heartbeat_B11_wellbeing_introspection
         COMMAND integration_heartbeat_B11_wellbeing_introspection)
set_tests_properties(integration_heartbeat_B11_wellbeing_introspection PROPERTIES
    LABELS "integration;fault_tolerance;heartbeat;B11;wellbeing_introspection"
    TIMEOUT 180
)

# Heartbeat B12 Neuro-Symbolic/JEPA Integration Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(integration_heartbeat_B12_neuro_symbolic_jepa
    test_heartbeat_B12_neuro_symbolic_jepa_integration.cpp
)
target_link_libraries(integration_heartbeat_B12_neuro_symbolic_jepa
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
)
target_include_directories(integration_heartbeat_B12_neuro_symbolic_jepa
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_heartbeat_B12_neuro_symbolic_jepa
         COMMAND integration_heartbeat_B12_neuro_symbolic_jepa)
set_tests_properties(integration_heartbeat_B12_neuro_symbolic_jepa PROPERTIES
    LABELS "integration;fault_tolerance;heartbeat;B12;neuro_symbolic_jepa"
    TIMEOUT 180
)

# Heartbeat B13 Knowledge/MentalHealth/Imagination Integration Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(integration_heartbeat_B13_knowledge_mentalhealth_imagination
    test_heartbeat_B13_knowledge_mentalhealth_imagination_integration.cpp
)
target_link_libraries(integration_heartbeat_B13_knowledge_mentalhealth_imagination
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
)
target_include_directories(integration_heartbeat_B13_knowledge_mentalhealth_imagination
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_heartbeat_B13_knowledge_mentalhealth_imagination
         COMMAND integration_heartbeat_B13_knowledge_mentalhealth_imagination)
set_tests_properties(integration_heartbeat_B13_knowledge_mentalhealth_imagination PROPERTIES
    LABELS "integration;fault_tolerance;heartbeat;B13;knowledge_mentalhealth_imagination"
    TIMEOUT 180
)

# B14: cognitive/curiosity + cognitive/collective_cognition + cognitive/logic
add_executable(integration_heartbeat_B14_curiosity_collective_logic
    test_heartbeat_B14_curiosity_collective_logic_integration.cpp
)
target_link_libraries(integration_heartbeat_B14_curiosity_collective_logic
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
)
target_include_directories(integration_heartbeat_B14_curiosity_collective_logic
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_heartbeat_B14_curiosity_collective_logic
         COMMAND integration_heartbeat_B14_curiosity_collective_logic)
set_tests_properties(integration_heartbeat_B14_curiosity_collective_logic PROPERTIES
    LABELS "integration;fault_tolerance;heartbeat;B14;curiosity_collective_logic"
    TIMEOUT 180
)

# B15: cognitive/theory_of_mind + cognitive/symbolic_logic + cognitive/self_model + cognitive/predictive
add_executable(integration_heartbeat_B15_tom_symboliclogic_selfmodel_predictive
    test_heartbeat_B15_tom_symboliclogic_selfmodel_predictive_integration.cpp
)

target_link_libraries(integration_heartbeat_B15_tom_symboliclogic_selfmodel_predictive
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(integration_heartbeat_B15_tom_symboliclogic_selfmodel_predictive
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_heartbeat_B15_tom_symboliclogic_selfmodel_predictive
         COMMAND integration_heartbeat_B15_tom_symboliclogic_selfmodel_predictive)
set_tests_properties(integration_heartbeat_B15_tom_symboliclogic_selfmodel_predictive PROPERTIES
    LABELS "integration;fault_tolerance;heartbeat;B15;tom_symboliclogic_selfmodel_predictive"
    TIMEOUT 180
)

# B16: cognitive/executive + cognitive/consolidation + cognitive/autobiographical_memory + cognitive/attention
add_executable(integration_heartbeat_B16_executive_consolidation_autobio_attention
    test_heartbeat_B16_executive_consolidation_autobio_attention_integration.cpp
)

target_link_libraries(integration_heartbeat_B16_executive_consolidation_autobio_attention
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(integration_heartbeat_B16_executive_consolidation_autobio_attention
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_heartbeat_B16_executive_consolidation_autobio_attention
         COMMAND integration_heartbeat_B16_executive_consolidation_autobio_attention)
set_tests_properties(integration_heartbeat_B16_executive_consolidation_autobio_attention PROPERTIES
    LABELS "integration;fault_tolerance;heartbeat;B16;executive_consolidation_autobio_attention"
    TIMEOUT 180
)

# B17: cognitive/emotion + emotional_tagging + emotion_recognition + emotions + emotion_tensor + grief + joy + remorse
add_executable(integration_heartbeat_B17_emotion_cluster
    test_heartbeat_B17_emotion_cluster_integration.cpp
)

target_link_libraries(integration_heartbeat_B17_emotion_cluster
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(integration_heartbeat_B17_emotion_cluster
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_heartbeat_B17_emotion_cluster
         COMMAND integration_heartbeat_B17_emotion_cluster)
set_tests_properties(integration_heartbeat_B17_emotion_cluster PROPERTIES
    LABELS "integration;fault_tolerance;heartbeat;B17;emotion_cluster"
    TIMEOUT 180
)

# B18: cognitive/empathetic_response + cognitive/ethics + cognitive/epistemic
add_executable(integration_heartbeat_B18_empathy_ethics_epistemic
    test_heartbeat_B18_empathy_ethics_epistemic_integration.cpp
)

target_link_libraries(integration_heartbeat_B18_empathy_ethics_epistemic
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(integration_heartbeat_B18_empathy_ethics_epistemic
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_heartbeat_B18_empathy_ethics_epistemic
         COMMAND integration_heartbeat_B18_empathy_ethics_epistemic)
set_tests_properties(integration_heartbeat_B18_empathy_ethics_epistemic PROPERTIES
    LABELS "integration;fault_tolerance;heartbeat;B18;empathy_ethics_epistemic"
    TIMEOUT 180
)

# B19: All remaining 89 uncovered modules (final batch for 100% coverage)
add_executable(integration_heartbeat_B19_remaining_modules
    test_heartbeat_B19_remaining_modules_integration.cpp
)

target_link_libraries(integration_heartbeat_B19_remaining_modules
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(integration_heartbeat_B19_remaining_modules
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_heartbeat_B19_remaining_modules
         COMMAND integration_heartbeat_B19_remaining_modules)
set_tests_properties(integration_heartbeat_B19_remaining_modules PROPERTIES
    LABELS "integration;fault_tolerance;heartbeat;B19;remaining_modules"
    TIMEOUT 180
)

# B20: cognitive/salience surprise bridges (plasticity, SNN, substrate, thalamic, pink_noise, imagination, self_model)
add_executable(integration_heartbeat_B20_surprise_bridges
    test_surprise_bridges_B20_integration.cpp
)

target_link_libraries(integration_heartbeat_B20_surprise_bridges
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(integration_heartbeat_B20_surprise_bridges
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_heartbeat_B20_surprise_bridges
         COMMAND integration_heartbeat_B20_surprise_bridges)
set_tests_properties(integration_heartbeat_B20_surprise_bridges PROPERTIES
    LABELS "integration;fault_tolerance;heartbeat;B20;surprise_bridges"
    TIMEOUT 180
)

# B22: Mirror neuron bridge heartbeat/utility integration tests
add_executable(integration_heartbeat_B22_mirror_bridges
    test_mirror_bridges_B22_integration.cpp
)
target_link_libraries(integration_heartbeat_B22_mirror_bridges
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(integration_heartbeat_B22_mirror_bridges
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_heartbeat_B22_mirror_bridges
         COMMAND integration_heartbeat_B22_mirror_bridges)
set_tests_properties(integration_heartbeat_B22_mirror_bridges PROPERTIES
    LABELS "integration;fault_tolerance;heartbeat;B22;mirror_bridges"
    TIMEOUT 180
)

# B23: Parietal bridge heartbeat/utility integration tests
add_executable(integration_heartbeat_B23_parietal_bridges
    test_parietal_bridges_B23_integration.cpp
)
target_link_libraries(integration_heartbeat_B23_parietal_bridges
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(integration_heartbeat_B23_parietal_bridges
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_heartbeat_B23_parietal_bridges
         COMMAND integration_heartbeat_B23_parietal_bridges)
set_tests_properties(integration_heartbeat_B23_parietal_bridges PROPERTIES
    LABELS "integration;fault_tolerance;heartbeat;B23;parietal_bridges"
    TIMEOUT 180
)

# B24: Predictive+Personality+Self-Awareness bridge heartbeat/utility integration tests
add_executable(integration_heartbeat_B24_predictive_personality_selfawareness_bridges
    test_predictive_personality_selfawareness_bridges_B24_integration.cpp
)
target_link_libraries(integration_heartbeat_B24_predictive_personality_selfawareness_bridges
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(integration_heartbeat_B24_predictive_personality_selfawareness_bridges
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_heartbeat_B24_predictive_personality_selfawareness_bridges
         COMMAND integration_heartbeat_B24_predictive_personality_selfawareness_bridges)
set_tests_properties(integration_heartbeat_B24_predictive_personality_selfawareness_bridges PROPERTIES
    LABELS "integration;fault_tolerance;heartbeat;B24;predictive_personality_selfawareness_bridges"
    TIMEOUT 180
)

# B25: Memory/Core bridge heartbeat/utility integration tests
add_executable(integration_heartbeat_B25_memory_core_bridges
    test_memory_core_bridges_B25_integration.cpp
)
target_link_libraries(integration_heartbeat_B25_memory_core_bridges
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(integration_heartbeat_B25_memory_core_bridges
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_heartbeat_B25_memory_core_bridges
         COMMAND integration_heartbeat_B25_memory_core_bridges)
set_tests_properties(integration_heartbeat_B25_memory_core_bridges PROPERTIES
    LABELS "integration;fault_tolerance;heartbeat;B25;memory_core_bridges"
    TIMEOUT 180
)

# B26: Emotion cluster bridge heartbeat/utility integration tests
add_executable(integration_heartbeat_B26_emotion_cluster_bridges
    test_emotion_cluster_bridges_B26_integration.cpp
)
target_link_libraries(integration_heartbeat_B26_emotion_cluster_bridges
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(integration_heartbeat_B26_emotion_cluster_bridges
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_heartbeat_B26_emotion_cluster_bridges
         COMMAND integration_heartbeat_B26_emotion_cluster_bridges)
set_tests_properties(integration_heartbeat_B26_emotion_cluster_bridges PROPERTIES
    LABELS "integration;fault_tolerance;heartbeat;B26;emotion_cluster_bridges"
    TIMEOUT 180
)

# B25nb: Memory/Core non-bridge module heartbeat/utility integration tests
add_executable(integration_heartbeat_B25nb_memory_core_nonbridge
    test_memory_core_nonbridge_B25nb_integration.cpp
)
target_link_libraries(integration_heartbeat_B25nb_memory_core_nonbridge
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(integration_heartbeat_B25nb_memory_core_nonbridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_heartbeat_B25nb_memory_core_nonbridge
         COMMAND integration_heartbeat_B25nb_memory_core_nonbridge)
set_tests_properties(integration_heartbeat_B25nb_memory_core_nonbridge PROPERTIES
    LABELS "integration;fault_tolerance;heartbeat;B25nb;memory_core_nonbridge"
    TIMEOUT 180
)
