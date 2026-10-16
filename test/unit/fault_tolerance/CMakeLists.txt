# ============================================================================
# Fault Tolerance Unit Tests
# ============================================================================
# WHAT: Unit tests for health agent, state manager, and recovery systems
# WHY:  Phase 8 health integration requires comprehensive testing
# HOW:  GoogleTest-based tests for individual components
# ============================================================================

# Health Agent Heartbeat Unit Tests
add_executable(unit_health_agent_heartbeat
    test_health_agent_heartbeat.cpp
)

target_include_directories(unit_health_agent_heartbeat PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_health_agent_heartbeat
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_health_agent_heartbeat PRIVATE cxx_std_17)
target_compile_definitions(unit_health_agent_heartbeat PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_health_agent_heartbeat
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;health;heartbeat"
        TIMEOUT 120
)

# State Manager Unit Tests
add_executable(unit_state_manager
    test_state_manager.cpp
)

target_include_directories(unit_state_manager PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_state_manager
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_state_manager PRIVATE cxx_std_17)
target_compile_definitions(unit_state_manager PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_state_manager
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;state_manager"
        TIMEOUT 120
)

# Training State Manager Unit Tests (Phase 8)
add_executable(unit_training_state_manager
    test_training_state_manager.cpp
)

target_include_directories(unit_training_state_manager PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_training_state_manager
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_training_state_manager PRIVATE cxx_std_17)
target_compile_definitions(unit_training_state_manager PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_training_state_manager
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;training;state_manager"
        TIMEOUT 120
)

# Heartbeat B01 Memory Core Unit Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(unit_heartbeat_B01_memory_core
    test_heartbeat_B01_memory_core_functions.cpp
)

target_include_directories(unit_heartbeat_B01_memory_core PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_heartbeat_B01_memory_core
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_heartbeat_B01_memory_core PRIVATE cxx_std_17)
target_compile_definitions(unit_heartbeat_B01_memory_core PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_heartbeat_B01_memory_core
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;heartbeat;B01;memory_core"
        TIMEOUT 120
)

# Heartbeat B02 Memory (non-core) Unit Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(unit_heartbeat_B02_memory
    test_heartbeat_B02_memory_functions.cpp
)

target_include_directories(unit_heartbeat_B02_memory PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_heartbeat_B02_memory
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_heartbeat_B02_memory PRIVATE cxx_std_17)
target_compile_definitions(unit_heartbeat_B02_memory PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_heartbeat_B02_memory
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;heartbeat;B02;memory"
        TIMEOUT 120
)

# Heartbeat B03 Immune Unit Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(unit_heartbeat_B03_immune
    test_heartbeat_B03_immune_functions.cpp
)

target_include_directories(unit_heartbeat_B03_immune PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_heartbeat_B03_immune
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_heartbeat_B03_immune PRIVATE cxx_std_17)
target_compile_definitions(unit_heartbeat_B03_immune PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_heartbeat_B03_immune
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;heartbeat;B03;immune"
        TIMEOUT 120
)

# Heartbeat B04 Parietal Unit Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(unit_heartbeat_B04_parietal
    test_heartbeat_B04_parietal_functions.cpp
)

target_include_directories(unit_heartbeat_B04_parietal PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_heartbeat_B04_parietal
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_heartbeat_B04_parietal PRIVATE cxx_std_17)
target_compile_definitions(unit_heartbeat_B04_parietal PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_heartbeat_B04_parietal
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;heartbeat;B04;parietal"
        TIMEOUT 120
)

# Heartbeat B05 Mirror Neurons Unit Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(unit_heartbeat_B05_mirror_neurons
    test_heartbeat_B05_mirror_neurons_functions.cpp
)

target_include_directories(unit_heartbeat_B05_mirror_neurons PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_heartbeat_B05_mirror_neurons
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_heartbeat_B05_mirror_neurons PRIVATE cxx_std_17)
target_compile_definitions(unit_heartbeat_B05_mirror_neurons PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_heartbeat_B05_mirror_neurons
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;heartbeat;B05;mirror_neurons"
        TIMEOUT 120
)

# Heartbeat B06 Integration Unit Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(unit_heartbeat_B06_integration
    test_heartbeat_B06_integration_functions.cpp
)

target_include_directories(unit_heartbeat_B06_integration PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_heartbeat_B06_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_heartbeat_B06_integration PRIVATE cxx_std_17)
target_compile_definitions(unit_heartbeat_B06_integration PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_heartbeat_B06_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;heartbeat;B06;integration"
        TIMEOUT 120
)

# Heartbeat B07 Game Theory Unit Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(unit_heartbeat_B07_game_theory
    test_heartbeat_B07_game_theory_functions.cpp
)

target_include_directories(unit_heartbeat_B07_game_theory PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_heartbeat_B07_game_theory
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_heartbeat_B07_game_theory PRIVATE cxx_std_17)
target_compile_definitions(unit_heartbeat_B07_game_theory PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_heartbeat_B07_game_theory
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;heartbeat;B07;game_theory"
        TIMEOUT 120
)

# Heartbeat B08 Ethics/Reasoning Unit Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(unit_heartbeat_B08_ethics_reasoning
    test_heartbeat_B08_ethics_reasoning_functions.cpp
)

target_include_directories(unit_heartbeat_B08_ethics_reasoning PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_heartbeat_B08_ethics_reasoning
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_heartbeat_B08_ethics_reasoning PRIVATE cxx_std_17)
target_compile_definitions(unit_heartbeat_B08_ethics_reasoning PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_heartbeat_B08_ethics_reasoning
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;heartbeat;B08;ethics_reasoning"
        TIMEOUT 120
)

# Heartbeat B09 Omni/Recursive Unit Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(unit_heartbeat_B09_omni_recursive
    test_heartbeat_B09_omni_recursive_functions.cpp
)

target_include_directories(unit_heartbeat_B09_omni_recursive PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_heartbeat_B09_omni_recursive
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_heartbeat_B09_omni_recursive PRIVATE cxx_std_17)
target_compile_definitions(unit_heartbeat_B09_omni_recursive PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_heartbeat_B09_omni_recursive
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;heartbeat;B09;omni_recursive"
        TIMEOUT 120
)

# Heartbeat B10 FEP/FaultTolerance Unit Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(unit_heartbeat_B10_fep_ft
    test_heartbeat_B10_fep_ft_functions.cpp
)

target_include_directories(unit_heartbeat_B10_fep_ft PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_heartbeat_B10_fep_ft
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_heartbeat_B10_fep_ft PRIVATE cxx_std_17)
target_compile_definitions(unit_heartbeat_B10_fep_ft PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_heartbeat_B10_fep_ft
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;heartbeat;B10;fep_ft"
        TIMEOUT 120
)

# Heartbeat B11 Wellbeing/Introspection Unit Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(unit_heartbeat_B11_wellbeing_introspection
    test_heartbeat_B11_wellbeing_introspection_functions.cpp
)

target_include_directories(unit_heartbeat_B11_wellbeing_introspection PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_heartbeat_B11_wellbeing_introspection
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_heartbeat_B11_wellbeing_introspection PRIVATE cxx_std_17)
target_compile_definitions(unit_heartbeat_B11_wellbeing_introspection PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_heartbeat_B11_wellbeing_introspection
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;heartbeat;B11;wellbeing_introspection"
        TIMEOUT 120
)

# Heartbeat B12 Neuro-Symbolic/JEPA Unit Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(unit_heartbeat_B12_neuro_symbolic_jepa
    test_heartbeat_B12_neuro_symbolic_jepa_functions.cpp
)

target_include_directories(unit_heartbeat_B12_neuro_symbolic_jepa PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_heartbeat_B12_neuro_symbolic_jepa
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_heartbeat_B12_neuro_symbolic_jepa PRIVATE cxx_std_17)
target_compile_definitions(unit_heartbeat_B12_neuro_symbolic_jepa PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_heartbeat_B12_neuro_symbolic_jepa
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;heartbeat;B12;neuro_symbolic_jepa"
        TIMEOUT 120
)

# Heartbeat B13 Knowledge/MentalHealth/Imagination Unit Tests (Phase 8: Per-Batch Heartbeat Testing)
add_executable(unit_heartbeat_B13_knowledge_mentalhealth_imagination
    test_heartbeat_B13_knowledge_mentalhealth_imagination_functions.cpp
)

target_include_directories(unit_heartbeat_B13_knowledge_mentalhealth_imagination PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_heartbeat_B13_knowledge_mentalhealth_imagination
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_heartbeat_B13_knowledge_mentalhealth_imagination PRIVATE cxx_std_17)
target_compile_definitions(unit_heartbeat_B13_knowledge_mentalhealth_imagination PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_heartbeat_B13_knowledge_mentalhealth_imagination
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;heartbeat;B13;knowledge_mentalhealth_imagination"
        TIMEOUT 120
)

# B14: cognitive/curiosity + cognitive/collective_cognition + cognitive/logic
add_executable(unit_heartbeat_B14_curiosity_collective_logic
    test_heartbeat_B14_curiosity_collective_logic_functions.cpp
)

target_include_directories(unit_heartbeat_B14_curiosity_collective_logic PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_heartbeat_B14_curiosity_collective_logic
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_heartbeat_B14_curiosity_collective_logic PRIVATE cxx_std_17)
target_compile_definitions(unit_heartbeat_B14_curiosity_collective_logic PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_heartbeat_B14_curiosity_collective_logic
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;heartbeat;B14;curiosity_collective_logic"
        TIMEOUT 120
)

# B15: cognitive/theory_of_mind + cognitive/symbolic_logic + cognitive/self_model + cognitive/predictive
add_executable(unit_heartbeat_B15_tom_symboliclogic_selfmodel_predictive
    test_heartbeat_B15_tom_symboliclogic_selfmodel_predictive_functions.cpp
)

target_link_libraries(unit_heartbeat_B15_tom_symboliclogic_selfmodel_predictive
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(unit_heartbeat_B15_tom_symboliclogic_selfmodel_predictive
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_heartbeat_B15_tom_symboliclogic_selfmodel_predictive PRIVATE cxx_std_17)
target_compile_definitions(unit_heartbeat_B15_tom_symboliclogic_selfmodel_predictive PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_heartbeat_B15_tom_symboliclogic_selfmodel_predictive
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;heartbeat;B15;tom_symboliclogic_selfmodel_predictive"
        TIMEOUT 120
)

# B16: cognitive/executive + cognitive/consolidation + cognitive/autobiographical_memory + cognitive/attention
add_executable(unit_heartbeat_B16_executive_consolidation_autobio_attention
    test_heartbeat_B16_executive_consolidation_autobio_attention_functions.cpp
)

target_link_libraries(unit_heartbeat_B16_executive_consolidation_autobio_attention
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(unit_heartbeat_B16_executive_consolidation_autobio_attention
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_heartbeat_B16_executive_consolidation_autobio_attention PRIVATE cxx_std_17)
target_compile_definitions(unit_heartbeat_B16_executive_consolidation_autobio_attention PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_heartbeat_B16_executive_consolidation_autobio_attention
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;heartbeat;B16;executive_consolidation_autobio_attention"
        TIMEOUT 120
)

# B17: cognitive/emotion + emotional_tagging + emotion_recognition + emotions + emotion_tensor + grief + joy + remorse
add_executable(unit_heartbeat_B17_emotion_cluster
    test_heartbeat_B17_emotion_cluster_functions.cpp
)

target_link_libraries(unit_heartbeat_B17_emotion_cluster
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(unit_heartbeat_B17_emotion_cluster
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_heartbeat_B17_emotion_cluster PRIVATE cxx_std_17)
target_compile_definitions(unit_heartbeat_B17_emotion_cluster PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_heartbeat_B17_emotion_cluster
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;heartbeat;B17;emotion_cluster"
        TIMEOUT 120
)

# B18: cognitive/empathetic_response + cognitive/ethics + cognitive/epistemic
add_executable(unit_heartbeat_B18_empathy_ethics_epistemic
    test_heartbeat_B18_empathy_ethics_epistemic_functions.cpp
)

target_link_libraries(unit_heartbeat_B18_empathy_ethics_epistemic
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(unit_heartbeat_B18_empathy_ethics_epistemic
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_heartbeat_B18_empathy_ethics_epistemic PRIVATE cxx_std_17)
target_compile_definitions(unit_heartbeat_B18_empathy_ethics_epistemic PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_heartbeat_B18_empathy_ethics_epistemic
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;heartbeat;B18;empathy_ethics_epistemic"
        TIMEOUT 120
)

# B19: All remaining 89 uncovered modules (final batch for 100% coverage)
add_executable(unit_heartbeat_B19_remaining_modules
    test_heartbeat_B19_remaining_modules_functions.cpp
)

target_link_libraries(unit_heartbeat_B19_remaining_modules
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(unit_heartbeat_B19_remaining_modules
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_heartbeat_B19_remaining_modules PRIVATE cxx_std_17)
target_compile_definitions(unit_heartbeat_B19_remaining_modules PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_heartbeat_B19_remaining_modules
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;heartbeat;B19;remaining_modules"
        TIMEOUT 120
)

# B20: cognitive/salience surprise bridges (plasticity, SNN, substrate, thalamic, pink_noise, imagination, self_model)
add_executable(unit_heartbeat_B20_surprise_bridges
    test_surprise_bridges_B20_functions.cpp
)

target_link_libraries(unit_heartbeat_B20_surprise_bridges
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(unit_heartbeat_B20_surprise_bridges
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_heartbeat_B20_surprise_bridges PRIVATE cxx_std_17)
target_compile_definitions(unit_heartbeat_B20_surprise_bridges PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_heartbeat_B20_surprise_bridges
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;heartbeat;B20;surprise_bridges"
        TIMEOUT 120
)

# B22: Mirror neuron bridge heartbeat/utility integration tests
add_executable(unit_heartbeat_B22_mirror_bridges
    test_mirror_bridges_B22_functions.cpp
)
target_link_libraries(unit_heartbeat_B22_mirror_bridges
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(unit_heartbeat_B22_mirror_bridges
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_heartbeat_B22_mirror_bridges PRIVATE cxx_std_17)
target_compile_definitions(unit_heartbeat_B22_mirror_bridges PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_heartbeat_B22_mirror_bridges
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;heartbeat;B22;mirror_bridges"
        TIMEOUT 120
)

# B23: Parietal bridge heartbeat/utility integration tests
add_executable(unit_heartbeat_B23_parietal_bridges
    test_parietal_bridges_B23_functions.cpp
)
target_link_libraries(unit_heartbeat_B23_parietal_bridges
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(unit_heartbeat_B23_parietal_bridges
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_heartbeat_B23_parietal_bridges PRIVATE cxx_std_17)
target_compile_definitions(unit_heartbeat_B23_parietal_bridges PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_heartbeat_B23_parietal_bridges
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;heartbeat;B23;parietal_bridges"
        TIMEOUT 120
)

# B24: Predictive+Personality+Self-Awareness bridge heartbeat/utility integration tests
add_executable(unit_heartbeat_B24_predictive_personality_selfawareness_bridges
    test_predictive_personality_selfawareness_bridges_B24_functions.cpp
)
target_link_libraries(unit_heartbeat_B24_predictive_personality_selfawareness_bridges
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(unit_heartbeat_B24_predictive_personality_selfawareness_bridges
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_heartbeat_B24_predictive_personality_selfawareness_bridges PRIVATE cxx_std_17)
target_compile_definitions(unit_heartbeat_B24_predictive_personality_selfawareness_bridges PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_heartbeat_B24_predictive_personality_selfawareness_bridges
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;heartbeat;B24;predictive_personality_selfawareness_bridges"
        TIMEOUT 120
)

# B25: Memory/Core bridge heartbeat/utility integration tests
add_executable(unit_heartbeat_B25_memory_core_bridges
    test_memory_core_bridges_B25_functions.cpp
)
target_link_libraries(unit_heartbeat_B25_memory_core_bridges
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(unit_heartbeat_B25_memory_core_bridges
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_heartbeat_B25_memory_core_bridges PRIVATE cxx_std_17)
target_compile_definitions(unit_heartbeat_B25_memory_core_bridges PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_heartbeat_B25_memory_core_bridges
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;heartbeat;B25;memory_core_bridges"
        TIMEOUT 120
)

# B26: Emotion cluster bridge heartbeat/utility unit tests
add_executable(unit_heartbeat_B26_emotion_cluster_bridges
    test_emotion_cluster_bridges_B26_functions.cpp
)
target_link_libraries(unit_heartbeat_B26_emotion_cluster_bridges
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(unit_heartbeat_B26_emotion_cluster_bridges
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_heartbeat_B26_emotion_cluster_bridges PRIVATE cxx_std_17)
target_compile_definitions(unit_heartbeat_B26_emotion_cluster_bridges PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_heartbeat_B26_emotion_cluster_bridges
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;heartbeat;B26;emotion_cluster_bridges"
        TIMEOUT 120
)

# B25nb: Memory/Core non-bridge module heartbeat/utility unit tests
add_executable(unit_heartbeat_B25nb_memory_core_nonbridge
    test_memory_core_nonbridge_B25nb_functions.cpp
)
target_link_libraries(unit_heartbeat_B25nb_memory_core_nonbridge
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        pthread
)

target_include_directories(unit_heartbeat_B25nb_memory_core_nonbridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_heartbeat_B25nb_memory_core_nonbridge PRIVATE cxx_std_17)
target_compile_definitions(unit_heartbeat_B25nb_memory_core_nonbridge PRIVATE NIMCP_TESTING)

gtest_discover_tests(unit_heartbeat_B25nb_memory_core_nonbridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;fault_tolerance;heartbeat;B25nb;memory_core_nonbridge"
        TIMEOUT 120
)

message(STATUS "Added fault tolerance unit tests")
