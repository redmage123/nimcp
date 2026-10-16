# Unit tests for Dragonfly Interception Module
# Bio-inspired target tracking and interception based on dragonfly neuroscience

# TSDN Population Vector Tests
add_executable(unit_dragonfly_test_tsdn
    test_dragonfly_tsdn.cpp
)

target_link_libraries(unit_dragonfly_test_tsdn
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_tsdn
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_tsdn PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_tsdn
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;tsdn"
        TIMEOUT 60
)

# Target Tracking Tests (CSTMD1-inspired)
add_executable(unit_dragonfly_test_tracking
    test_dragonfly_tracking.cpp
)

target_link_libraries(unit_dragonfly_test_tracking
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_tracking
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_tracking PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_tracking
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;tracking"
        TIMEOUT 60
)

# Trajectory Prediction Tests (IMM filter, evasion detection)
add_executable(unit_dragonfly_test_prediction
    test_dragonfly_prediction.cpp
)

target_link_libraries(unit_dragonfly_test_prediction
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_prediction
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_prediction PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_prediction
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;prediction"
        TIMEOUT 60
)

# Interception Planning Tests (Proportional Navigation)
add_executable(unit_dragonfly_test_intercept
    test_dragonfly_intercept.cpp
)

target_link_libraries(unit_dragonfly_test_intercept
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_intercept
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_intercept PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_intercept
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;intercept"
        TIMEOUT 60
)

# Main Dragonfly System Tests (Coordinator)
add_executable(unit_dragonfly_test_system
    test_dragonfly_system.cpp
)

target_link_libraries(unit_dragonfly_test_system
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_system
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_system PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_system
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;system"
        TIMEOUT 60
)

# Visual Cortex Bridge Tests
add_executable(unit_dragonfly_test_visual_bridge
    test_dragonfly_visual_bridge.cpp
)

target_link_libraries(unit_dragonfly_test_visual_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_visual_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_visual_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_visual_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;visual_bridge"
        TIMEOUT 60
)

# Audio Cortex Bridge Tests (Directional Cueing)
add_executable(unit_dragonfly_test_audio_bridge
    test_dragonfly_audio_bridge.cpp
)

target_link_libraries(unit_dragonfly_test_audio_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_audio_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_audio_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_audio_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;audio_bridge"
        TIMEOUT 60
)

# Parietal Lobe Bridge Tests (Spatial/Visuomotor)
add_executable(unit_dragonfly_test_parietal_bridge
    test_dragonfly_parietal_bridge.cpp
)

target_link_libraries(unit_dragonfly_test_parietal_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_parietal_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_parietal_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_parietal_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;parietal_bridge"
        TIMEOUT 60
)

# Cortical Column Bridge Tests (TSDN as hypercolumn)
add_executable(unit_dragonfly_test_cortical_bridge
    test_dragonfly_cortical_bridge.cpp
)

target_link_libraries(unit_dragonfly_test_cortical_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_cortical_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_cortical_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_cortical_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;cortical_bridge"
        TIMEOUT 60
)

# Cognitive Bridge Tests (Attention, WM, Executive, Salience)
add_executable(unit_dragonfly_test_cognitive_bridge
    test_dragonfly_cognitive_bridge.cpp
)

target_link_libraries(unit_dragonfly_test_cognitive_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_cognitive_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_cognitive_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_cognitive_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;cognitive_bridge"
        TIMEOUT 60
)

# Thalamic Bridge Tests (Signal routing through thalamic nuclei)
add_executable(unit_dragonfly_test_thalamic_bridge
    test_dragonfly_thalamic_bridge.cpp
)

target_link_libraries(unit_dragonfly_test_thalamic_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_thalamic_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_thalamic_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_thalamic_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;thalamic_bridge"
        TIMEOUT 60
)

# Substrate Bridge Tests (Metabolic costs and fatigue modeling)
add_executable(unit_dragonfly_test_substrate_bridge
    test_dragonfly_substrate_bridge.cpp
)

target_link_libraries(unit_dragonfly_test_substrate_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_substrate_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_substrate_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_substrate_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;substrate_bridge"
        TIMEOUT 60
)

# CNN Training Bridge Tests
add_executable(unit_dragonfly_test_cnn_bridge
    test_dragonfly_cnn_bridge.cpp
)

target_link_libraries(unit_dragonfly_test_cnn_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_cnn_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_cnn_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_cnn_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;cnn_bridge"
        TIMEOUT 60
)

# SNN Backprop Bridge Tests
add_executable(unit_dragonfly_test_snn_bridge
    test_dragonfly_snn_bridge.cpp
)

target_link_libraries(unit_dragonfly_test_snn_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_snn_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_snn_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_snn_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;snn_bridge"
        TIMEOUT 60
)

# Bio-Async Bridge Tests
add_executable(unit_dragonfly_test_bio_async_bridge
    test_dragonfly_bio_async_bridge.cpp
)

target_link_libraries(unit_dragonfly_test_bio_async_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_bio_async_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_bio_async_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_bio_async_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;bio_async_bridge"
        TIMEOUT 60
)

# Global Workspace Bridge Tests
add_executable(unit_dragonfly_test_workspace_bridge
    test_dragonfly_workspace_bridge.cpp
)

target_link_libraries(unit_dragonfly_test_workspace_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_workspace_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_workspace_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_workspace_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;workspace_bridge"
        TIMEOUT 60
)

# FEP Bridge Tests (Free Energy Principle)
add_executable(unit_dragonfly_test_fep_bridge
    test_dragonfly_fep_bridge.cpp
)

target_link_libraries(unit_dragonfly_test_fep_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_fep_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_fep_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_fep_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;fep_bridge"
        TIMEOUT 60
)

# Multi-Target Priority Queue Tests
add_executable(unit_dragonfly_test_multi_target
    test_dragonfly_multi_target.cpp
)

target_link_libraries(unit_dragonfly_test_multi_target
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_multi_target
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_multi_target PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_multi_target
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;multi_target"
        TIMEOUT 60
)

# Energy-Optimal Pursuit Tests
add_executable(unit_dragonfly_test_energy
    test_dragonfly_energy.cpp
)

target_link_libraries(unit_dragonfly_test_energy
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_energy
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_energy PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_energy
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;energy"
        TIMEOUT 60
)

# Collision Avoidance Tests
add_executable(unit_dragonfly_test_collision
    test_dragonfly_collision.cpp
)

target_link_libraries(unit_dragonfly_test_collision
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_collision
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_collision PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_collision
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;collision"
        TIMEOUT 60
)

# Learning from Hunt Outcomes Tests
add_executable(unit_dragonfly_test_learning
    test_dragonfly_learning.cpp
)

target_link_libraries(unit_dragonfly_test_learning
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_learning
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_learning PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_learning
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;learning"
        TIMEOUT 60
)

# Gaze Stabilization Tests
add_executable(unit_dragonfly_test_gaze
    test_dragonfly_gaze.cpp
)

target_link_libraries(unit_dragonfly_test_gaze
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_gaze
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_gaze PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_gaze
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;gaze"
        TIMEOUT 60
)

# Sleep Bridge Tests (Circadian modulation and memory consolidation)
add_executable(unit_dragonfly_test_sleep_bridge
    test_dragonfly_sleep_bridge.cpp
)

target_link_libraries(unit_dragonfly_test_sleep_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_sleep_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_sleep_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_sleep_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;sleep_bridge"
        TIMEOUT 60
)

# Emotion Bridge Tests (Motivational drives and arousal)
add_executable(unit_dragonfly_test_emotion_bridge
    test_dragonfly_emotion_bridge.cpp
)

target_link_libraries(unit_dragonfly_test_emotion_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_emotion_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_emotion_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_emotion_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;emotion_bridge"
        TIMEOUT 60
)

# Immune Bridge Tests (Health and stress modulation)
add_executable(unit_dragonfly_test_immune_bridge
    test_dragonfly_immune_bridge.cpp
)

target_link_libraries(unit_dragonfly_test_immune_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_immune_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_immune_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_immune_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;immune_bridge"
        TIMEOUT 60
)

# Environment Tests (Clutter, background, terrain)
add_executable(unit_dragonfly_test_environment
    test_dragonfly_environment.cpp
)

target_link_libraries(unit_dragonfly_test_environment
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_environment
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_environment PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_environment
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;environment"
        TIMEOUT 60
)

# Coordinator Tests (Full system coordination)
add_executable(unit_dragonfly_test_coordinator
    test_dragonfly_coordinator.cpp
)

target_link_libraries(unit_dragonfly_test_coordinator
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_dragonfly_test_coordinator
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_dragonfly_test_coordinator PRIVATE cxx_std_17)

gtest_discover_tests(unit_dragonfly_test_coordinator
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dragonfly;coordinator"
        TIMEOUT 120
)

# Exception handling tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/exception/CMakeLists.txt")
    add_subdirectory(exception)
endif()
