# ============================================================================
# Cognitive Integration Bridge Tests
# ============================================================================
# WHAT: Unit tests for Cognitive Integration bridges
# WHY:  Validate cross-module integration bridges
# HOW:  Test lifecycle, configuration, bidirectional communication, and statistics
# ============================================================================

# Emotion-Executive Bridge Tests
add_executable(unit_cognitive_test_emotion_executive_bridge
    test_emotion_executive_bridge.cpp
)

target_link_libraries(unit_cognitive_test_emotion_executive_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_emotion_executive_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_emotion_executive_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_emotion_executive_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;integration;emotion;executive"
        TIMEOUT 120
)

# GW-Cognitive Bridge Tests
add_executable(unit_cognitive_test_gw_cognitive_bridge
    test_gw_cognitive_bridge.cpp
)

target_link_libraries(unit_cognitive_test_gw_cognitive_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_gw_cognitive_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_gw_cognitive_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_gw_cognitive_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;integration;gw;cognitive"
        TIMEOUT 120
)

# Attention-Working Memory Bridge Tests
add_executable(unit_cognitive_test_attention_wm_bridge
    test_attention_wm_bridge.cpp
)

target_link_libraries(unit_cognitive_test_attention_wm_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_attention_wm_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_attention_wm_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_attention_wm_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;integration;attention;wm"
        TIMEOUT 120
)

# Emotion-Memory Bridge Tests
add_executable(unit_cognitive_test_emotion_memory_bridge
    test_emotion_memory_bridge.cpp
)

target_link_libraries(unit_cognitive_test_emotion_memory_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_emotion_memory_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_emotion_memory_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_emotion_memory_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;integration;emotion;memory"
        TIMEOUT 120
)

# Curiosity-Reasoning Bridge Tests
add_executable(unit_cognitive_test_curiosity_reasoning_bridge
    test_curiosity_reasoning_bridge.cpp
)

target_link_libraries(unit_cognitive_test_curiosity_reasoning_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_curiosity_reasoning_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_curiosity_reasoning_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_curiosity_reasoning_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;integration;curiosity;reasoning"
        TIMEOUT 120
)

# Cognitive Integration Hub Tests
add_executable(unit_cognitive_test_cognitive_integration_hub
    test_cognitive_integration_hub.cpp
)

target_link_libraries(unit_cognitive_test_cognitive_integration_hub
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_cognitive_integration_hub
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_cognitive_integration_hub PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_cognitive_integration_hub
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;integration;hub"
        TIMEOUT 180
)

# Ethics-Executive Bridge Tests
add_executable(unit_cognitive_test_ethics_executive_bridge
    test_ethics_executive_bridge.cpp
)

target_link_libraries(unit_cognitive_test_ethics_executive_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_ethics_executive_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_ethics_executive_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_ethics_executive_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;integration;ethics;executive"
        TIMEOUT 120
)

# ToM-Social Bridge Tests
add_executable(unit_cognitive_test_tom_social_bridge
    test_tom_social_bridge.cpp
)

target_link_libraries(unit_cognitive_test_tom_social_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_tom_social_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_tom_social_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_tom_social_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;integration;tom;social"
        TIMEOUT 120
)

# Self-Introspection Bridge Tests
add_executable(unit_cognitive_test_self_introspection_bridge
    test_self_introspection_bridge.cpp
)

target_link_libraries(unit_cognitive_test_self_introspection_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_self_introspection_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_self_introspection_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_self_introspection_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;integration;self;introspection"
        TIMEOUT 120
)

# Collective-Hub Bridge Tests
add_executable(unit_cognitive_test_collective_hub_bridge
    test_collective_hub_bridge.cpp
)

target_link_libraries(unit_cognitive_test_collective_hub_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_collective_hub_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_collective_hub_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_collective_hub_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;integration;collective;hub"
        TIMEOUT 120
)

# Rcog-Hub Bridge Tests
add_executable(unit_cognitive_test_rcog_hub_bridge
    test_rcog_hub_bridge.cpp
)

target_link_libraries(unit_cognitive_test_rcog_hub_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_rcog_hub_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_rcog_hub_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_rcog_hub_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;integration;rcog;hub"
        TIMEOUT 120
)

# Salience-Attention Bridge Tests
add_executable(unit_cognitive_test_salience_attention_bridge
    test_salience_attention_bridge.cpp
)

target_link_libraries(unit_cognitive_test_salience_attention_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_salience_attention_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_salience_attention_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_salience_attention_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;integration;salience;attention"
        TIMEOUT 120
)

# Predictive-Attention Bridge Tests
add_executable(unit_cognitive_test_predictive_attention_bridge
    test_predictive_attention_bridge.cpp
)

target_link_libraries(unit_cognitive_test_predictive_attention_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_predictive_attention_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_predictive_attention_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_predictive_attention_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;integration;predictive;attention"
        TIMEOUT 120
)

# Mirror-Empathy Bridge Tests
add_executable(unit_cognitive_test_mirror_empathy_bridge
    test_mirror_empathy_bridge.cpp
)

target_link_libraries(unit_cognitive_test_mirror_empathy_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_mirror_empathy_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_mirror_empathy_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_mirror_empathy_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;integration;mirror;empathy"
        TIMEOUT 120
)

# Game Theory-Executive Bridge Tests
add_executable(unit_cognitive_test_game_theory_executive_bridge
    test_game_theory_executive_bridge.cpp
)

target_link_libraries(unit_cognitive_test_game_theory_executive_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_game_theory_executive_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_game_theory_executive_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_game_theory_executive_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;integration;game_theory;executive"
        TIMEOUT 120
)

# Imagination-Reasoning Bridge Tests
add_executable(unit_cognitive_test_imagination_reasoning_bridge
    test_imagination_reasoning_bridge.cpp
)

target_link_libraries(unit_cognitive_test_imagination_reasoning_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_imagination_reasoning_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_imagination_reasoning_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_imagination_reasoning_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;integration;imagination;reasoning"
        TIMEOUT 120
)

# Salience-Attention FEP Bridge Tests
add_executable(unit_cognitive_test_salience_attention_fep_bridge
    test_salience_attention_fep_bridge.cpp
)

target_link_libraries(unit_cognitive_test_salience_attention_fep_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_salience_attention_fep_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_salience_attention_fep_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_salience_attention_fep_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;integration;salience;attention;fep"
        TIMEOUT 120
)

# Mirror-Empathy FEP Bridge Tests
add_executable(unit_cognitive_test_mirror_empathy_fep_bridge
    test_mirror_empathy_fep_bridge.cpp
)

target_link_libraries(unit_cognitive_test_mirror_empathy_fep_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_mirror_empathy_fep_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_mirror_empathy_fep_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_mirror_empathy_fep_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;integration;mirror;empathy;fep"
        TIMEOUT 120
)

# Game Theory-Executive FEP Bridge Tests
add_executable(unit_cognitive_test_game_theory_executive_fep_bridge
    test_game_theory_executive_fep_bridge.cpp
)

target_link_libraries(unit_cognitive_test_game_theory_executive_fep_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_game_theory_executive_fep_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_game_theory_executive_fep_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_game_theory_executive_fep_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;integration;game_theory;executive;fep"
        TIMEOUT 120
)

# Predictive-Attention FEP Bridge Tests
add_executable(unit_cognitive_test_predictive_attention_fep_bridge
    test_predictive_attention_fep_bridge.cpp
)

target_link_libraries(unit_cognitive_test_predictive_attention_fep_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_predictive_attention_fep_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_predictive_attention_fep_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_predictive_attention_fep_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;integration;predictive;attention;fep"
        TIMEOUT 120
)

# Imagination-Reasoning FEP Bridge Tests
add_executable(unit_cognitive_test_imagination_reasoning_fep_bridge
    test_imagination_reasoning_fep_bridge.cpp
)

target_link_libraries(unit_cognitive_test_imagination_reasoning_fep_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_imagination_reasoning_fep_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_imagination_reasoning_fep_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_imagination_reasoning_fep_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;integration;imagination;reasoning;fep"
        TIMEOUT 120
)

# Security-Cognitive Hub Bridge Tests
add_executable(unit_cognitive_test_security_cognitive_hub_bridge
    test_security_cognitive_hub_bridge.cpp
)

target_link_libraries(unit_cognitive_test_security_cognitive_hub_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_security_cognitive_hub_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_security_cognitive_hub_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_security_cognitive_hub_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;integration;security;cognitive;hub"
        TIMEOUT 120
)
