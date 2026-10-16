# Brain Immune System Integration Tests

# Integration tests for brain immune with FT/Security modules
add_executable(integration_cognitive_immune_brain_immune
    test_brain_immune_integration.cpp
)
target_link_libraries(integration_cognitive_immune_brain_immune
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(integration_cognitive_immune_brain_immune
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/utils/fault_tolerance
        ${CMAKE_SOURCE_DIR}/src/swarm
        ${CMAKE_SOURCE_DIR}/src/async
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
)
add_test(NAME integration_cognitive_immune_brain_immune
         COMMAND integration_cognitive_immune_brain_immune)
set_tests_properties(integration_cognitive_immune_brain_immune PROPERTIES LABELS "integration")

# BBB-Immune Integration Tests
add_executable(integration_cognitive_immune_bbb
    test_bbb_integration.cpp
)
target_link_libraries(integration_cognitive_immune_bbb
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(integration_cognitive_immune_bbb
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
)
add_test(NAME integration_cognitive_immune_bbb
         COMMAND integration_cognitive_immune_bbb)
set_tests_properties(integration_cognitive_immune_bbb PROPERTIES LABELS "integration")

# Perception-Immune Integration Tests
add_executable(integration_cognitive_immune_perception_immune
    test_perception_immune_integration.cpp
)
target_link_libraries(integration_cognitive_immune_perception_immune
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(integration_cognitive_immune_perception_immune
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/perception
)
add_test(NAME integration_cognitive_immune_perception_immune
         COMMAND integration_cognitive_immune_perception_immune)
set_tests_properties(integration_cognitive_immune_perception_immune PROPERTIES LABELS "integration")

# Memory-Immune Integration Tests
add_executable(integration_cognitive_immune_memory
    integration_cognitive_immune_memory.cpp
)
target_link_libraries(integration_cognitive_immune_memory
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(integration_cognitive_immune_memory
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/cognitive
        ${CMAKE_SOURCE_DIR}/src/cognitive/working_memory
        ${CMAKE_SOURCE_DIR}/src/cognitive/consolidation
)
add_test(NAME integration_cognitive_immune_memory
         COMMAND integration_cognitive_immune_memory)
set_tests_properties(integration_cognitive_immune_memory PROPERTIES LABELS "integration")

# Emotion-Immune Pipeline Integration Tests
add_executable(integration_emotion_immune_pipeline
    integration_emotion_immune_pipeline.cpp
)
target_link_libraries(integration_emotion_immune_pipeline
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(integration_emotion_immune_pipeline
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/cognitive
)
add_test(NAME integration_emotion_immune_pipeline
         COMMAND integration_emotion_immune_pipeline)
set_tests_properties(integration_emotion_immune_pipeline PROPERTIES LABELS "integration")

# ============================================================================
# New Immune Enhancement Module Integration Tests
# ============================================================================

# Immune Modules Integration Tests (all 8 new modules)
add_executable(integration_cognitive_immune_modules
    test_immune_modules_integration.cpp
)
target_link_libraries(integration_cognitive_immune_modules
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(integration_cognitive_immune_modules
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
)
add_test(NAME integration_cognitive_immune_modules
         COMMAND integration_cognitive_immune_modules)
set_tests_properties(integration_cognitive_immune_modules PROPERTIES LABELS "integration")

# ============================================================================
# Code Immune (Self-Healing) System Integration Tests
# ============================================================================

# Code Immune Integration Tests (signal handler, self-heal, DWARF symbols)
add_executable(integration_cognitive_immune_code_immune
    test_code_immune_integration.cpp
)
target_link_libraries(integration_cognitive_immune_code_immune
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(integration_cognitive_immune_code_immune
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/cognitive/immune
        ${CMAKE_SOURCE_DIR}/include/utils/signal
        ${CMAKE_SOURCE_DIR}/include/utils/code
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/utils/signal
        ${CMAKE_SOURCE_DIR}/src/utils/code
        ${CMAKE_SOURCE_DIR}/src/lnn
)
add_test(NAME integration_cognitive_immune_code_immune
         COMMAND integration_cognitive_immune_code_immune)
set_tests_properties(integration_cognitive_immune_code_immune PROPERTIES LABELS "integration")

# Heal Bridge Integration Tests (self-heal, code-immune, pattern evolution)
add_executable(integration_cognitive_immune_heal_bridge
    test_heal_bridge_integration.cpp
)
target_link_libraries(integration_cognitive_immune_heal_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(integration_cognitive_immune_heal_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/lnn
)
add_test(NAME integration_cognitive_immune_heal_bridge
         COMMAND integration_cognitive_immune_heal_bridge)
set_tests_properties(integration_cognitive_immune_heal_bridge PROPERTIES LABELS "integration")

# Code Immune Self-Repair Integration Tests
add_executable(integration_cognitive_immune_code_immune_self_repair
    test_code_immune_self_repair_integration.cpp
)
target_link_libraries(integration_cognitive_immune_code_immune_self_repair
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(integration_cognitive_immune_code_immune_self_repair
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)
add_test(NAME integration_cognitive_immune_code_immune_self_repair
         COMMAND integration_cognitive_immune_code_immune_self_repair)
set_tests_properties(integration_cognitive_immune_code_immune_self_repair PROPERTIES LABELS "integration")

# ============================================================================
# Brain Immune Tick Enhanced Integration Tests (Check Framework)
# ============================================================================
# WHAT: Integration tests for brain immune tick NIMCP utility enhancements
# WHY:  Verify MC severity, QA recovery, pattern memory with health agent flow
# HOW:  Check framework tests with proper fixtures

find_package(PkgConfig REQUIRED)
pkg_check_modules(CHECK check)

if(CHECK_FOUND)
    add_executable(integration_cognitive_immune_tick_enhanced
        test_brain_immune_tick_enhanced_integration.c
    )
    target_include_directories(integration_cognitive_immune_tick_enhanced PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CHECK_INCLUDE_DIRS}
    )
    target_link_libraries(integration_cognitive_immune_tick_enhanced
        nimcp
        ${CHECK_LIBRARIES}
        pthread
        m
    )
    add_test(NAME integration_cognitive_immune_tick_enhanced
             COMMAND integration_cognitive_immune_tick_enhanced)
    set_tests_properties(integration_cognitive_immune_tick_enhanced PROPERTIES
        LABELS "integration;cognitive;immune;tick;enhanced;mc;qa;pattern"
        TIMEOUT 180
    )
else()
    message(STATUS "Check library not found - skipping brain immune tick enhanced integration tests")
endif()
