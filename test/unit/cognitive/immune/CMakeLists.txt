# Brain Immune System Unit Tests

# Brain Immune Unit Tests
add_executable(unit_cognitive_immune_brain_immune
    test_brain_immune.cpp
)
target_link_libraries(unit_cognitive_immune_brain_immune
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_immune_brain_immune
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
)
add_test(NAME unit_cognitive_immune_brain_immune
         COMMAND unit_cognitive_immune_brain_immune)
set_tests_properties(unit_cognitive_immune_brain_immune PROPERTIES LABELS "unit")

# Perception Immune Integration Unit Tests
add_executable(unit_cognitive_immune_perception_immune
    test_perception_immune.cpp
)
target_link_libraries(unit_cognitive_immune_perception_immune
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_immune_perception_immune
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/perception
)
add_test(NAME unit_cognitive_immune_perception_immune
         COMMAND unit_cognitive_immune_perception_immune)
set_tests_properties(unit_cognitive_immune_perception_immune PROPERTIES LABELS "unit")

# Immune-Plasticity Integration Unit Tests
add_executable(unit_cognitive_immune_plasticity_modulation
    unit_cognitive_immune_plasticity_modulation.cpp
)
target_link_libraries(unit_cognitive_immune_plasticity_modulation
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_immune_plasticity_modulation
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/plasticity
)
add_test(NAME unit_cognitive_immune_plasticity_modulation
         COMMAND unit_cognitive_immune_plasticity_modulation)
set_tests_properties(unit_cognitive_immune_plasticity_modulation PROPERTIES LABELS "unit")

# Memory-Immune Integration Unit Tests
add_executable(unit_cognitive_immune_memory_integration
    unit_cognitive_immune_memory_integration.cpp
)
target_link_libraries(unit_cognitive_immune_memory_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_immune_memory_integration
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/cognitive
)
add_test(NAME unit_cognitive_immune_memory_integration
         COMMAND unit_cognitive_immune_memory_integration)
set_tests_properties(unit_cognitive_immune_memory_integration PROPERTIES LABELS "unit")

# Emotion-Immune Bridge Unit Tests
add_executable(unit_cognitive_immune_emotion_bridge
    unit_cognitive_immune_emotion_bridge.cpp
)
target_link_libraries(unit_cognitive_immune_emotion_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_immune_emotion_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/cognitive
)
add_test(NAME unit_cognitive_immune_emotion_bridge
         COMMAND unit_cognitive_immune_emotion_bridge)
set_tests_properties(unit_cognitive_immune_emotion_bridge PROPERTIES LABELS "unit")

# Memory-Immune Subsystem Integration Unit Tests
add_executable(unit_cognitive_immune_memory_subsystem_integration
    test_memory_immune_subsystem_integration.cpp
)
target_link_libraries(unit_cognitive_immune_memory_subsystem_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_immune_memory_subsystem_integration
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/cognitive/memory
        ${CMAKE_SOURCE_DIR}/src/cognitive
)
add_test(NAME unit_cognitive_immune_memory_subsystem_integration
         COMMAND unit_cognitive_immune_memory_subsystem_integration)
set_tests_properties(unit_cognitive_immune_memory_subsystem_integration PROPERTIES LABELS "unit")

# Full Immune-Plasticity Integration Unit Tests (All Plasticity Modules)
add_executable(unit_cognitive_immune_plasticity_full
    test_immune_plasticity_full.cpp
)
target_link_libraries(unit_cognitive_immune_plasticity_full
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_immune_plasticity_full
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/plasticity
        ${CMAKE_SOURCE_DIR}/src/plasticity/bcm
        ${CMAKE_SOURCE_DIR}/src/plasticity/stdp
        ${CMAKE_SOURCE_DIR}/src/plasticity/stp
        ${CMAKE_SOURCE_DIR}/src/plasticity/homeostatic
        ${CMAKE_SOURCE_DIR}/src/plasticity/dendritic
        ${CMAKE_SOURCE_DIR}/src/plasticity/adaptive
        ${CMAKE_SOURCE_DIR}/src/plasticity/eligibility
        ${CMAKE_SOURCE_DIR}/src/plasticity/predictive
)
add_test(NAME unit_cognitive_immune_plasticity_full
         COMMAND unit_cognitive_immune_plasticity_full)
set_tests_properties(unit_cognitive_immune_plasticity_full PROPERTIES LABELS "unit")

# Attention-Immune Bridge Unit Tests
add_executable(unit_cognitive_immune_attention_bridge
    test_attention_immune_integration.cpp
)
target_link_libraries(unit_cognitive_immune_attention_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_immune_attention_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/cognitive/attention
        ${CMAKE_SOURCE_DIR}/src/plasticity/attention
)
add_test(NAME unit_cognitive_immune_attention_bridge
         COMMAND unit_cognitive_immune_attention_bridge)
set_tests_properties(unit_cognitive_immune_attention_bridge PROPERTIES LABELS "unit")

# Reasoning-Immune Bridge Unit Tests
add_executable(unit_cognitive_immune_reasoning_bridge
    test_reasoning_immune.cpp
)
target_link_libraries(unit_cognitive_immune_reasoning_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_immune_reasoning_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/cognitive/reasoning
)
add_test(NAME unit_cognitive_immune_reasoning_bridge
         COMMAND unit_cognitive_immune_reasoning_bridge)
set_tests_properties(unit_cognitive_immune_reasoning_bridge PROPERTIES LABELS "unit")

# ============================================================================
# New Immune Enhancement Module Unit Tests
# ============================================================================

# Trained Immunity Unit Tests
add_executable(unit_cognitive_immune_trained_immunity
    test_trained_immunity.cpp
)
target_link_libraries(unit_cognitive_immune_trained_immunity
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_immune_trained_immunity
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
)
add_test(NAME unit_cognitive_immune_trained_immunity
         COMMAND unit_cognitive_immune_trained_immunity)
set_tests_properties(unit_cognitive_immune_trained_immunity PROPERTIES LABELS "unit")

# Complement System Unit Tests
add_executable(unit_cognitive_immune_complement_system
    test_complement_system.cpp
)
target_link_libraries(unit_cognitive_immune_complement_system
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_immune_complement_system
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
)
add_test(NAME unit_cognitive_immune_complement_system
         COMMAND unit_cognitive_immune_complement_system)
set_tests_properties(unit_cognitive_immune_complement_system PROPERTIES LABELS "unit")

# Regulatory T Cells Unit Tests - DISABLED: requires treg_ functions not implemented
# add_executable(unit_cognitive_immune_regulatory_tcells
#     test_regulatory_tcells.cpp
# )
# target_link_libraries(unit_cognitive_immune_regulatory_tcells
#     PRIVATE
#         GTest::GTest
#         GTest::Main
#         nimcp
# )
# target_include_directories(unit_cognitive_immune_regulatory_tcells
#     PRIVATE
#         ${CMAKE_SOURCE_DIR}/include
#         ${CMAKE_SOURCE_DIR}/src
#         ${CMAKE_SOURCE_DIR}/src/utils/memory
#         ${CMAKE_SOURCE_DIR}/src/utils/logging
#         ${CMAKE_SOURCE_DIR}/src/cognitive/immune
# )
# add_test(NAME unit_cognitive_immune_regulatory_tcells
#          COMMAND unit_cognitive_immune_regulatory_tcells)
# set_tests_properties(unit_cognitive_immune_regulatory_tcells PROPERTIES LABELS "unit")

# Immune Exhaustion Unit Tests
add_executable(unit_cognitive_immune_exhaustion
    test_immune_exhaustion.cpp
)
target_link_libraries(unit_cognitive_immune_exhaustion
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_immune_exhaustion
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
)
add_test(NAME unit_cognitive_immune_exhaustion
         COMMAND unit_cognitive_immune_exhaustion)
set_tests_properties(unit_cognitive_immune_exhaustion PROPERTIES LABELS "unit")

# Immune Tolerance Unit Tests
add_executable(unit_cognitive_immune_tolerance
    test_immune_tolerance.cpp
)
target_link_libraries(unit_cognitive_immune_tolerance
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_immune_tolerance
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
)
add_test(NAME unit_cognitive_immune_tolerance
         COMMAND unit_cognitive_immune_tolerance)
set_tests_properties(unit_cognitive_immune_tolerance PROPERTIES LABELS "unit")

# Immune Vaccine Unit Tests
add_executable(unit_cognitive_immune_vaccine
    test_immune_vaccine.cpp
)
target_link_libraries(unit_cognitive_immune_vaccine
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_immune_vaccine
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
)
add_test(NAME unit_cognitive_immune_vaccine
         COMMAND unit_cognitive_immune_vaccine)
set_tests_properties(unit_cognitive_immune_vaccine PROPERTIES LABELS "unit")

# Mucosal Immunity Unit Tests
add_executable(unit_cognitive_immune_mucosal
    test_mucosal_immunity.cpp
)
target_link_libraries(unit_cognitive_immune_mucosal
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_immune_mucosal
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
)
add_test(NAME unit_cognitive_immune_mucosal
         COMMAND unit_cognitive_immune_mucosal)
set_tests_properties(unit_cognitive_immune_mucosal PROPERTIES LABELS "unit")

# Immune Persistence Unit Tests
add_executable(unit_cognitive_immune_persistence
    test_immune_persistence.cpp
)
target_link_libraries(unit_cognitive_immune_persistence
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_immune_persistence
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
)
add_test(NAME unit_cognitive_immune_persistence
         COMMAND unit_cognitive_immune_persistence)
set_tests_properties(unit_cognitive_immune_persistence PROPERTIES LABELS "unit")

# Immune Bridge Coordinator Unit Tests
add_executable(unit_cognitive_immune_bridge_coordinator
    test_immune_bridge_coordinator.cpp
)
target_link_libraries(unit_cognitive_immune_bridge_coordinator
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_immune_bridge_coordinator
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/async
)
add_test(NAME unit_cognitive_immune_bridge_coordinator
         COMMAND unit_cognitive_immune_bridge_coordinator)
set_tests_properties(unit_cognitive_immune_bridge_coordinator PROPERTIES LABELS "unit")

# Code Immune System (Self-Healing) Unit Tests
add_executable(unit_cognitive_immune_code_immune
    test_code_immune.cpp
)
target_link_libraries(unit_cognitive_immune_code_immune
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_immune_code_immune
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/utils/signal
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
)
add_test(NAME unit_cognitive_immune_code_immune
         COMMAND unit_cognitive_immune_code_immune)
set_tests_properties(unit_cognitive_immune_code_immune PROPERTIES LABELS "unit")

# Enhanced Self-Healing Bridge Unit Tests
add_executable(unit_cognitive_immune_heal_bridge
    test_heal_bridge.cpp
)
target_link_libraries(unit_cognitive_immune_heal_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_immune_heal_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
)
add_test(NAME unit_cognitive_immune_heal_bridge
         COMMAND unit_cognitive_immune_heal_bridge)
set_tests_properties(unit_cognitive_immune_heal_bridge PROPERTIES LABELS "unit")

# Code Immune Integrated Self-Repair Unit Tests
add_executable(unit_cognitive_immune_integrated_self_repair
    test_code_immune_integrated_self_repair.cpp
)
target_link_libraries(unit_cognitive_immune_integrated_self_repair
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_immune_integrated_self_repair
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/utils/fault_tolerance
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/cognitive/fault_tolerance
)
add_test(NAME unit_cognitive_immune_integrated_self_repair
         COMMAND unit_cognitive_immune_integrated_self_repair)
set_tests_properties(unit_cognitive_immune_integrated_self_repair PROPERTIES LABELS "unit")

# Code Immune Self-Repair Bridge Unit Tests
add_executable(unit_cognitive_immune_self_repair_bridge
    test_code_immune_self_repair.cpp
)
target_link_libraries(unit_cognitive_immune_self_repair_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_immune_self_repair_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/utils/fault_tolerance
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/cognitive/fault_tolerance
)
add_test(NAME unit_cognitive_immune_self_repair_bridge
         COMMAND unit_cognitive_immune_self_repair_bridge)
set_tests_properties(unit_cognitive_immune_self_repair_bridge PROPERTIES LABELS "unit")

# ============================================================================
# Brain Immune Tick Enhanced Unit Tests (Check Framework)
# ============================================================================
# WHAT: Unit tests for brain immune tick NIMCP utility enhancements
# WHY:  Verify MC severity, QA recovery, pattern memory, and numerical integration
# HOW:  Check framework tests with proper fixtures

find_package(PkgConfig REQUIRED)
pkg_check_modules(CHECK check)

if(CHECK_FOUND)
    add_executable(unit_cognitive_immune_tick_enhanced
        test_brain_immune_tick_enhanced.c
    )
    target_include_directories(unit_cognitive_immune_tick_enhanced PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CHECK_INCLUDE_DIRS}
    )
    target_link_libraries(unit_cognitive_immune_tick_enhanced
        nimcp
        ${CHECK_LIBRARIES}
        pthread
        m
    )
    add_test(NAME unit_cognitive_immune_tick_enhanced
             COMMAND unit_cognitive_immune_tick_enhanced)
    set_tests_properties(unit_cognitive_immune_tick_enhanced PROPERTIES
        LABELS "unit;cognitive;immune;tick;enhanced;mc;qa;pattern"
        TIMEOUT 120
    )
    # Brain Immune Integration Helper Unit Tests
    # WHAT: Test the nimcp_immune_integration_* helper functions
    # WHY:  Verify the full exception-immune setup helper works correctly
    # HOW:  Check framework tests for create, tick, stats, diagnostics

    add_executable(unit_cognitive_immune_integration_helper
        test_brain_immune_integration.c
    )
    target_include_directories(unit_cognitive_immune_integration_helper PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CHECK_INCLUDE_DIRS}
    )
    target_link_libraries(unit_cognitive_immune_integration_helper
        nimcp
        ${CHECK_LIBRARIES}
        pthread
        m
    )
    add_test(NAME unit_cognitive_immune_integration_helper
             COMMAND unit_cognitive_immune_integration_helper)
    set_tests_properties(unit_cognitive_immune_integration_helper PROPERTIES
        LABELS "unit;cognitive;immune;integration;helper"
        TIMEOUT 120
    )
else()
    message(STATUS "Check library not found - skipping brain immune tick enhanced unit tests")
endif()
