# Cognitive Mental Health Unit Tests

# Basic Mental Health Tests
add_executable(unit_cognitive_mental_health_basic
    test_mental_health.cpp
)
target_link_libraries(unit_cognitive_mental_health_basic
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_compile_definitions(unit_cognitive_mental_health_basic PRIVATE NIMCP_TESTING)
target_include_directories(unit_cognitive_mental_health_basic
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/core/brain
        ${CMAKE_SOURCE_DIR}/src/cognitive/mental_health
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME unit_cognitive_mental_health_basic
         COMMAND unit_cognitive_mental_health_basic --gtest_brief=1)
set_tests_properties(unit_cognitive_mental_health_basic PROPERTIES LABELS "unit;mental_health" TIMEOUT 180)

# Coverage Mental Health Tests
add_executable(unit_cognitive_mental_health_coverage
    test_mental_health_coverage.cpp
)
target_link_libraries(unit_cognitive_mental_health_coverage
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_compile_definitions(unit_cognitive_mental_health_coverage PRIVATE NIMCP_TESTING)
target_include_directories(unit_cognitive_mental_health_coverage
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/core/brain
        ${CMAKE_SOURCE_DIR}/src/cognitive/mental_health
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME unit_cognitive_mental_health_coverage
         COMMAND unit_cognitive_mental_health_coverage --gtest_brief=1)
set_tests_properties(unit_cognitive_mental_health_coverage PROPERTIES LABELS "unit;mental_health" TIMEOUT 180)

# Real Mental Health Tests
add_executable(unit_cognitive_mental_health_real
    test_mental_health_real.cpp
)
target_link_libraries(unit_cognitive_mental_health_real
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_compile_definitions(unit_cognitive_mental_health_real PRIVATE NIMCP_TESTING)
target_include_directories(unit_cognitive_mental_health_real
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/core/brain
        ${CMAKE_SOURCE_DIR}/src/cognitive/mental_health
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME unit_cognitive_mental_health_real
         COMMAND unit_cognitive_mental_health_real --gtest_brief=1)
set_tests_properties(unit_cognitive_mental_health_real PROPERTIES LABELS "unit;mental_health" TIMEOUT 180)

# Disorder Detectors Tests
add_executable(unit_cognitive_mental_health_detectors
    test_disorder_detectors.cpp
)
target_link_libraries(unit_cognitive_mental_health_detectors
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_compile_definitions(unit_cognitive_mental_health_detectors PRIVATE NIMCP_TESTING)
target_include_directories(unit_cognitive_mental_health_detectors
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/core/brain
        ${CMAKE_SOURCE_DIR}/src/cognitive/mental_health
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME unit_cognitive_mental_health_detectors
         COMMAND unit_cognitive_mental_health_detectors --gtest_brief=1)
set_tests_properties(unit_cognitive_mental_health_detectors PROPERTIES LABELS "unit;mental_health" TIMEOUT 180)

# Interventions Tests
add_executable(unit_cognitive_mental_health_interventions
    test_interventions.cpp
)
target_link_libraries(unit_cognitive_mental_health_interventions
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_compile_definitions(unit_cognitive_mental_health_interventions PRIVATE NIMCP_TESTING)
target_include_directories(unit_cognitive_mental_health_interventions
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/core/brain
        ${CMAKE_SOURCE_DIR}/src/cognitive/mental_health
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME unit_cognitive_mental_health_interventions
         COMMAND unit_cognitive_mental_health_interventions --gtest_brief=1)
set_tests_properties(unit_cognitive_mental_health_interventions PROPERTIES LABELS "unit;mental_health" TIMEOUT 180)

# Immune Integration Tests
add_executable(unit_cognitive_mental_health_immune_integration
    test_immune_integration.cpp
)
target_link_libraries(unit_cognitive_mental_health_immune_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_compile_definitions(unit_cognitive_mental_health_immune_integration PRIVATE NIMCP_TESTING)
target_include_directories(unit_cognitive_mental_health_immune_integration
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/core/brain
        ${CMAKE_SOURCE_DIR}/src/cognitive/mental_health
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME unit_cognitive_mental_health_immune_integration
         COMMAND unit_cognitive_mental_health_immune_integration --gtest_brief=1)
set_tests_properties(unit_cognitive_mental_health_immune_integration PROPERTIES
    LABELS "unit;mental_health;immune;integration"
    TIMEOUT 120
)

# ============================================================================
# Mental Health Guardian Tests (Phase 10.5.1)
# ============================================================================
# WHAT: Tests for the independent background monitoring agent
# WHY:  Verify guardian correctly monitors, detects, and intervenes
# HOW:  Test lifecycle, thread control, intervention levels, metrics

add_executable(unit_cognitive_mental_health_guardian
    test_mental_health_guardian.cpp
)
target_link_libraries(unit_cognitive_mental_health_guardian
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_compile_definitions(unit_cognitive_mental_health_guardian PRIVATE NIMCP_TESTING)
target_include_directories(unit_cognitive_mental_health_guardian
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/core/brain
        ${CMAKE_SOURCE_DIR}/src/cognitive/mental_health
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME unit_cognitive_mental_health_guardian
         COMMAND unit_cognitive_mental_health_guardian --gtest_brief=1)
set_tests_properties(unit_cognitive_mental_health_guardian PROPERTIES
    LABELS "unit;mental_health;guardian"
    TIMEOUT 120
)

# ============================================================================
# NIMCP Mental Health Comprehensive Tests (Phase 10.5)
# ============================================================================
# WHAT: Comprehensive tests for the Mental Health Monitoring System
# WHY:  Verify all 23 disorder detectors, intervention system, and monitoring pipeline
# HOW:  Test lifecycle, severity classification, all disorder detection, reporting, stats

add_executable(unit_cognitive_mental_health_nimcp
    test_nimcp_mental_health.cpp
)
target_link_libraries(unit_cognitive_mental_health_nimcp
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_compile_definitions(unit_cognitive_mental_health_nimcp PRIVATE NIMCP_TESTING)
target_include_directories(unit_cognitive_mental_health_nimcp
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/core/brain
        ${CMAKE_SOURCE_DIR}/src/cognitive/mental_health
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME unit_cognitive_mental_health_nimcp
         COMMAND unit_cognitive_mental_health_nimcp --gtest_brief=1)
set_tests_properties(unit_cognitive_mental_health_nimcp PROPERTIES
    LABELS "unit;mental_health;comprehensive"
    TIMEOUT 120
)
