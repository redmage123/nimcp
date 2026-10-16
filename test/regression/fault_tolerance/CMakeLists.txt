# ============================================================================
# Fault Tolerance Regression Tests
# ============================================================================
# WHAT: Regression tests for health system stability
# WHY:  Phase 8 health integration requires stability testing
# HOW:  GoogleTest-based tests for edge cases and boundary conditions
# ============================================================================

# Main Fault Tolerance Regression Tests
add_executable(regression_fault_tolerance
    test_fault_tolerance_regression.cpp
)

target_include_directories(regression_fault_tolerance PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(regression_fault_tolerance
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(regression_fault_tolerance PRIVATE cxx_std_17)
target_compile_definitions(regression_fault_tolerance PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_fault_tolerance
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance"
        TIMEOUT 180
)

# Health Stability Regression Tests (Phase 8)
add_executable(regression_health_stability
    test_health_stability_regression.cpp
)

target_include_directories(regression_health_stability PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(regression_health_stability
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(regression_health_stability PRIVATE cxx_std_17)
target_compile_definitions(regression_health_stability PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_health_stability
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;health;phase8"
        TIMEOUT 180
)

# Heartbeat B01 Memory Core Regression Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(regression_heartbeat_B01_memory_core
    test_heartbeat_B01_memory_core_regression.cpp
)

target_include_directories(regression_heartbeat_B01_memory_core PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(regression_heartbeat_B01_memory_core
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(regression_heartbeat_B01_memory_core PRIVATE cxx_std_17)
target_compile_definitions(regression_heartbeat_B01_memory_core PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_heartbeat_B01_memory_core
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;heartbeat;B01;memory_core"
        TIMEOUT 180
)

# Heartbeat B02 Memory (non-core) Regression Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(regression_heartbeat_B02_memory
    test_heartbeat_B02_memory_regression.cpp
)

target_include_directories(regression_heartbeat_B02_memory PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(regression_heartbeat_B02_memory
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(regression_heartbeat_B02_memory PRIVATE cxx_std_17)
target_compile_definitions(regression_heartbeat_B02_memory PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_heartbeat_B02_memory
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;heartbeat;B02;memory"
        TIMEOUT 180
)

# Heartbeat B03 Immune Regression Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(regression_heartbeat_B03_immune
    test_heartbeat_B03_immune_regression.cpp
)

target_include_directories(regression_heartbeat_B03_immune PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(regression_heartbeat_B03_immune
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(regression_heartbeat_B03_immune PRIVATE cxx_std_17)
target_compile_definitions(regression_heartbeat_B03_immune PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_heartbeat_B03_immune
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;heartbeat;B03;immune"
        TIMEOUT 180
)

# Heartbeat B04 Parietal Regression Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(regression_heartbeat_B04_parietal
    test_heartbeat_B04_parietal_regression.cpp
)

target_include_directories(regression_heartbeat_B04_parietal PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(regression_heartbeat_B04_parietal
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(regression_heartbeat_B04_parietal PRIVATE cxx_std_17)
target_compile_definitions(regression_heartbeat_B04_parietal PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_heartbeat_B04_parietal
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;heartbeat;B04;parietal"
        TIMEOUT 180
)

# Heartbeat B05 Mirror Neurons Regression Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(regression_heartbeat_B05_mirror_neurons
    test_heartbeat_B05_mirror_neurons_regression.cpp
)

target_include_directories(regression_heartbeat_B05_mirror_neurons PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(regression_heartbeat_B05_mirror_neurons
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(regression_heartbeat_B05_mirror_neurons PRIVATE cxx_std_17)
target_compile_definitions(regression_heartbeat_B05_mirror_neurons PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_heartbeat_B05_mirror_neurons
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;heartbeat;B05;mirror_neurons"
        TIMEOUT 180
)

# Heartbeat B06 Integration Regression Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(regression_heartbeat_B06_integration
    test_heartbeat_B06_integration_regression.cpp
)

target_include_directories(regression_heartbeat_B06_integration PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(regression_heartbeat_B06_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(regression_heartbeat_B06_integration PRIVATE cxx_std_17)
target_compile_definitions(regression_heartbeat_B06_integration PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_heartbeat_B06_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;heartbeat;B06;integration"
        TIMEOUT 180
)

# Heartbeat B07 Game Theory Regression Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(regression_heartbeat_B07_game_theory
    test_heartbeat_B07_game_theory_regression.cpp
)

target_include_directories(regression_heartbeat_B07_game_theory PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(regression_heartbeat_B07_game_theory
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(regression_heartbeat_B07_game_theory PRIVATE cxx_std_17)
target_compile_definitions(regression_heartbeat_B07_game_theory PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_heartbeat_B07_game_theory
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;heartbeat;B07;game_theory"
        TIMEOUT 180
)

# Heartbeat B08 Ethics/Reasoning Regression Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(regression_heartbeat_B08_ethics_reasoning
    test_heartbeat_B08_ethics_reasoning_regression.cpp
)

target_include_directories(regression_heartbeat_B08_ethics_reasoning PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(regression_heartbeat_B08_ethics_reasoning
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(regression_heartbeat_B08_ethics_reasoning PRIVATE cxx_std_17)
target_compile_definitions(regression_heartbeat_B08_ethics_reasoning PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_heartbeat_B08_ethics_reasoning
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;heartbeat;B08;ethics_reasoning"
        TIMEOUT 180
)

# Heartbeat B09 Omni/Recursive Regression Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(regression_heartbeat_B09_omni_recursive
    test_heartbeat_B09_omni_recursive_regression.cpp
)

target_include_directories(regression_heartbeat_B09_omni_recursive PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(regression_heartbeat_B09_omni_recursive
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(regression_heartbeat_B09_omni_recursive PRIVATE cxx_std_17)
target_compile_definitions(regression_heartbeat_B09_omni_recursive PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_heartbeat_B09_omni_recursive
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;heartbeat;B09;omni_recursive"
        TIMEOUT 180
)

# Heartbeat B10 FEP/FaultTolerance Regression Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(regression_heartbeat_B10_fep_ft
    test_heartbeat_B10_fep_ft_regression.cpp
)

target_include_directories(regression_heartbeat_B10_fep_ft PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(regression_heartbeat_B10_fep_ft
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(regression_heartbeat_B10_fep_ft PRIVATE cxx_std_17)
target_compile_definitions(regression_heartbeat_B10_fep_ft PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_heartbeat_B10_fep_ft
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;heartbeat;B10;fep_ft"
        TIMEOUT 180
)

# Heartbeat B11 Wellbeing/Introspection Regression Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(regression_heartbeat_B11_wellbeing_introspection
    test_heartbeat_B11_wellbeing_introspection_regression.cpp
)

target_include_directories(regression_heartbeat_B11_wellbeing_introspection PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(regression_heartbeat_B11_wellbeing_introspection
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(regression_heartbeat_B11_wellbeing_introspection PRIVATE cxx_std_17)
target_compile_definitions(regression_heartbeat_B11_wellbeing_introspection PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_heartbeat_B11_wellbeing_introspection
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;heartbeat;B11;wellbeing_introspection"
        TIMEOUT 180
)

# Heartbeat B12 Neuro-Symbolic/JEPA Regression Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(regression_heartbeat_B12_neuro_symbolic_jepa
    test_heartbeat_B12_neuro_symbolic_jepa_regression.cpp
)

target_include_directories(regression_heartbeat_B12_neuro_symbolic_jepa PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(regression_heartbeat_B12_neuro_symbolic_jepa
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(regression_heartbeat_B12_neuro_symbolic_jepa PRIVATE cxx_std_17)
target_compile_definitions(regression_heartbeat_B12_neuro_symbolic_jepa PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_heartbeat_B12_neuro_symbolic_jepa
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;heartbeat;B12;neuro_symbolic_jepa"
        TIMEOUT 180
)

# Heartbeat B13 Knowledge/MentalHealth/Imagination Regression Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(regression_heartbeat_B13_knowledge_mentalhealth_imagination
    test_heartbeat_B13_knowledge_mentalhealth_imagination_regression.cpp
)

target_include_directories(regression_heartbeat_B13_knowledge_mentalhealth_imagination PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(regression_heartbeat_B13_knowledge_mentalhealth_imagination
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(regression_heartbeat_B13_knowledge_mentalhealth_imagination PRIVATE cxx_std_17)
target_compile_definitions(regression_heartbeat_B13_knowledge_mentalhealth_imagination PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_heartbeat_B13_knowledge_mentalhealth_imagination
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;heartbeat;B13;knowledge_mentalhealth_imagination"
        TIMEOUT 180
)

# B14: cognitive/curiosity + cognitive/collective_cognition + cognitive/logic
add_executable(regression_heartbeat_B14_curiosity_collective_logic
    test_heartbeat_B14_curiosity_collective_logic_regression.cpp
)

target_include_directories(regression_heartbeat_B14_curiosity_collective_logic PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(regression_heartbeat_B14_curiosity_collective_logic
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(regression_heartbeat_B14_curiosity_collective_logic PRIVATE cxx_std_17)
target_compile_definitions(regression_heartbeat_B14_curiosity_collective_logic PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_heartbeat_B14_curiosity_collective_logic
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;heartbeat;B14;curiosity_collective_logic"
        TIMEOUT 180
)

# B15: cognitive/theory_of_mind + cognitive/symbolic_logic + cognitive/self_model + cognitive/predictive
add_executable(regression_heartbeat_B15_tom_symboliclogic_selfmodel_predictive
    test_heartbeat_B15_tom_symboliclogic_selfmodel_predictive_regression.cpp
)

target_link_libraries(regression_heartbeat_B15_tom_symboliclogic_selfmodel_predictive
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(regression_heartbeat_B15_tom_symboliclogic_selfmodel_predictive
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(regression_heartbeat_B15_tom_symboliclogic_selfmodel_predictive PRIVATE cxx_std_17)
target_compile_definitions(regression_heartbeat_B15_tom_symboliclogic_selfmodel_predictive PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_heartbeat_B15_tom_symboliclogic_selfmodel_predictive
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;heartbeat;B15;tom_symboliclogic_selfmodel_predictive"
        TIMEOUT 180
)

# B16: cognitive/executive + cognitive/consolidation + cognitive/autobiographical_memory + cognitive/attention
add_executable(regression_heartbeat_B16_executive_consolidation_autobio_attention
    test_heartbeat_B16_executive_consolidation_autobio_attention_regression.cpp
)

target_link_libraries(regression_heartbeat_B16_executive_consolidation_autobio_attention
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(regression_heartbeat_B16_executive_consolidation_autobio_attention
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(regression_heartbeat_B16_executive_consolidation_autobio_attention PRIVATE cxx_std_17)
target_compile_definitions(regression_heartbeat_B16_executive_consolidation_autobio_attention PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_heartbeat_B16_executive_consolidation_autobio_attention
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;heartbeat;B16;executive_consolidation_autobio_attention"
        TIMEOUT 180
)

# B17: cognitive/emotion + emotional_tagging + emotion_recognition + emotions + emotion_tensor + grief + joy + remorse
add_executable(regression_heartbeat_B17_emotion_cluster
    test_heartbeat_B17_emotion_cluster_regression.cpp
)

target_link_libraries(regression_heartbeat_B17_emotion_cluster
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(regression_heartbeat_B17_emotion_cluster
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(regression_heartbeat_B17_emotion_cluster PRIVATE cxx_std_17)
target_compile_definitions(regression_heartbeat_B17_emotion_cluster PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_heartbeat_B17_emotion_cluster
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;heartbeat;B17;emotion_cluster"
        TIMEOUT 180
)

# B18: cognitive/empathetic_response + cognitive/ethics + cognitive/epistemic
add_executable(regression_heartbeat_B18_empathy_ethics_epistemic
    test_heartbeat_B18_empathy_ethics_epistemic_regression.cpp
)

target_link_libraries(regression_heartbeat_B18_empathy_ethics_epistemic
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(regression_heartbeat_B18_empathy_ethics_epistemic
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(regression_heartbeat_B18_empathy_ethics_epistemic PRIVATE cxx_std_17)
target_compile_definitions(regression_heartbeat_B18_empathy_ethics_epistemic PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_heartbeat_B18_empathy_ethics_epistemic
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;heartbeat;B18;empathy_ethics_epistemic"
        TIMEOUT 180
)

# B19: All remaining 89 uncovered modules (final batch for 100% coverage)
add_executable(regression_heartbeat_B19_remaining_modules
    test_heartbeat_B19_remaining_modules_regression.cpp
)

target_link_libraries(regression_heartbeat_B19_remaining_modules
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(regression_heartbeat_B19_remaining_modules
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(regression_heartbeat_B19_remaining_modules PRIVATE cxx_std_17)
target_compile_definitions(regression_heartbeat_B19_remaining_modules PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_heartbeat_B19_remaining_modules
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;heartbeat;B19;remaining_modules"
        TIMEOUT 180
)

# B20: cognitive/salience surprise bridges (plasticity, SNN, substrate, thalamic, pink_noise, imagination, self_model)
add_executable(regression_heartbeat_B20_surprise_bridges
    test_surprise_bridges_B20_regression.cpp
)

target_link_libraries(regression_heartbeat_B20_surprise_bridges
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(regression_heartbeat_B20_surprise_bridges
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(regression_heartbeat_B20_surprise_bridges PRIVATE cxx_std_17)
target_compile_definitions(regression_heartbeat_B20_surprise_bridges PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_heartbeat_B20_surprise_bridges
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;heartbeat;B20;surprise_bridges"
        TIMEOUT 180
)

# B22: Mirror neuron bridge heartbeat/utility regression tests
add_executable(regression_heartbeat_B22_mirror_bridges
    test_mirror_bridges_B22_regression.cpp
)
target_link_libraries(regression_heartbeat_B22_mirror_bridges
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(regression_heartbeat_B22_mirror_bridges
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(regression_heartbeat_B22_mirror_bridges PRIVATE cxx_std_17)
target_compile_definitions(regression_heartbeat_B22_mirror_bridges PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_heartbeat_B22_mirror_bridges
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;heartbeat;B22;mirror_bridges"
        TIMEOUT 180
)

# B23: Parietal bridge heartbeat/utility regression tests
add_executable(regression_heartbeat_B23_parietal_bridges
    test_parietal_bridges_B23_regression.cpp
)
target_link_libraries(regression_heartbeat_B23_parietal_bridges
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(regression_heartbeat_B23_parietal_bridges
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(regression_heartbeat_B23_parietal_bridges PRIVATE cxx_std_17)
target_compile_definitions(regression_heartbeat_B23_parietal_bridges PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_heartbeat_B23_parietal_bridges
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;heartbeat;B23;parietal_bridges"
        TIMEOUT 180
)

# B24: Predictive+Personality+Self-Awareness bridge heartbeat/utility regression tests
add_executable(regression_heartbeat_B24_predictive_personality_selfawareness_bridges
    test_predictive_personality_selfawareness_bridges_B24_regression.cpp
)
target_link_libraries(regression_heartbeat_B24_predictive_personality_selfawareness_bridges
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(regression_heartbeat_B24_predictive_personality_selfawareness_bridges
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(regression_heartbeat_B24_predictive_personality_selfawareness_bridges PRIVATE cxx_std_17)
target_compile_definitions(regression_heartbeat_B24_predictive_personality_selfawareness_bridges PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_heartbeat_B24_predictive_personality_selfawareness_bridges
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;heartbeat;B24;predictive_personality_selfawareness_bridges"
        TIMEOUT 180
)

# B25: Memory/Core bridge heartbeat/utility regression tests
add_executable(regression_heartbeat_B25_memory_core_bridges
    test_memory_core_bridges_B25_regression.cpp
)
target_link_libraries(regression_heartbeat_B25_memory_core_bridges
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(regression_heartbeat_B25_memory_core_bridges
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(regression_heartbeat_B25_memory_core_bridges PRIVATE cxx_std_17)
target_compile_definitions(regression_heartbeat_B25_memory_core_bridges PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_heartbeat_B25_memory_core_bridges
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;heartbeat;B25;memory_core_bridges"
        TIMEOUT 180
)

# B26: Emotion cluster bridge heartbeat/utility regression tests
add_executable(regression_heartbeat_B26_emotion_cluster_bridges
    test_emotion_cluster_bridges_B26_regression.cpp
)
target_link_libraries(regression_heartbeat_B26_emotion_cluster_bridges
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(regression_heartbeat_B26_emotion_cluster_bridges
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(regression_heartbeat_B26_emotion_cluster_bridges PRIVATE cxx_std_17)
target_compile_definitions(regression_heartbeat_B26_emotion_cluster_bridges PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_heartbeat_B26_emotion_cluster_bridges
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;heartbeat;B26;emotion_cluster_bridges"
        TIMEOUT 180
)

# B25nb: Memory/Core non-bridge module heartbeat/utility regression tests
add_executable(regression_heartbeat_B25nb_memory_core_nonbridge
    test_memory_core_nonbridge_B25nb_regression.cpp
)
target_link_libraries(regression_heartbeat_B25nb_memory_core_nonbridge
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(regression_heartbeat_B25nb_memory_core_nonbridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(regression_heartbeat_B25nb_memory_core_nonbridge PRIVATE cxx_std_17)
target_compile_definitions(regression_heartbeat_B25nb_memory_core_nonbridge PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_heartbeat_B25nb_memory_core_nonbridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;heartbeat;B25nb;memory_core_nonbridge"
        TIMEOUT 180
)

# P1-P3 Recovery (Circuit Breaker) Regression Tests
add_executable(regression_fault_tolerance_test_recovery_regression
    test_recovery_regression.cpp
)
target_link_libraries(regression_fault_tolerance_test_recovery_regression
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
        m
)
target_include_directories(regression_fault_tolerance_test_recovery_regression
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)
target_compile_features(regression_fault_tolerance_test_recovery_regression PRIVATE cxx_std_17)
target_compile_definitions(regression_fault_tolerance_test_recovery_regression PRIVATE NIMCP_TESTING)
gtest_discover_tests(regression_fault_tolerance_test_recovery_regression
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;recovery;p1_p3"
        TIMEOUT 180
)

# P1-P3 Graceful Degradation Regression Tests
add_executable(regression_fault_tolerance_test_graceful_degradation_regression
    test_graceful_degradation_regression.cpp
)
target_link_libraries(regression_fault_tolerance_test_graceful_degradation_regression
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
        m
)
target_include_directories(regression_fault_tolerance_test_graceful_degradation_regression
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)
target_compile_features(regression_fault_tolerance_test_graceful_degradation_regression PRIVATE cxx_std_17)
target_compile_definitions(regression_fault_tolerance_test_graceful_degradation_regression PRIVATE NIMCP_TESTING)
gtest_discover_tests(regression_fault_tolerance_test_graceful_degradation_regression
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;fault_tolerance;graceful_degradation;p1_p3"
        TIMEOUT 180
)

message(STATUS "Added fault tolerance regression tests")
