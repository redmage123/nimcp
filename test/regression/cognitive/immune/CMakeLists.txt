# Brain Immune System Regression Tests

# Brain Immune Regression Tests
add_executable(regression_cognitive_immune_brain_immune
    test_brain_immune_regression.cpp
)
target_link_libraries(regression_cognitive_immune_brain_immune
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(regression_cognitive_immune_brain_immune
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
add_test(NAME regression_cognitive_immune_brain_immune
         COMMAND regression_cognitive_immune_brain_immune)
set_tests_properties(regression_cognitive_immune_brain_immune PROPERTIES LABELS "regression")

# ============================================================================
# New Immune Enhancement Module Regression Tests
# ============================================================================

# Immune Modules Regression Tests (all 8 new modules)
add_executable(regression_cognitive_immune_modules
    test_immune_modules_regression.cpp
)
target_link_libraries(regression_cognitive_immune_modules
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(regression_cognitive_immune_modules
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
)
add_test(NAME regression_cognitive_immune_modules
         COMMAND regression_cognitive_immune_modules)
set_tests_properties(regression_cognitive_immune_modules PROPERTIES LABELS "regression")

# ============================================================================
# Code Immune (Self-Healing) System Regression Tests
# ============================================================================

# Code Immune Regression Tests (self-healing engine, patterns, LNN integration)
add_executable(regression_cognitive_immune_code_immune
    test_code_immune_regression.cpp
)
target_link_libraries(regression_cognitive_immune_code_immune
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(regression_cognitive_immune_code_immune
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/lnn
)
add_test(NAME regression_cognitive_immune_code_immune
         COMMAND regression_cognitive_immune_code_immune)
set_tests_properties(regression_cognitive_immune_code_immune PROPERTIES LABELS "regression")

# ============================================================================
# Race Condition + NULL Deref Regression Tests (2026-03-05)
# ============================================================================
# WHAT: Regression tests for C1/H2 (immune antigen race), C3 (sensory race),
#       C2 (SNN surrogate NULL deref)
# WHY:  Verify thread-safety fixes and NULL pointer guards
# HOW:  Multi-threaded antigen tests, concurrent sensory updates, NULL gradient checks

add_executable(regression_race_condition
    test_race_condition_regression.cpp
)
target_link_libraries(regression_race_condition
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
)
target_include_directories(regression_race_condition
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(regression_race_condition PRIVATE cxx_std_17)
add_test(NAME regression_race_condition
         COMMAND regression_race_condition)
set_tests_properties(regression_race_condition PROPERTIES
    LABELS "regression;race;immune;sensory;snn"
    TIMEOUT 120
)

# ============================================================================
# Brain Immune Tick Enhanced Regression Tests (Check Framework)
# ============================================================================
# WHAT: Regression tests for brain immune tick NIMCP utility enhancements
# WHY:  Catch edge cases: MC clamping, hash collisions, QA zero weights, EMA overflow
# HOW:  Check framework tests with edge case coverage

find_package(PkgConfig REQUIRED)
pkg_check_modules(CHECK check)

if(CHECK_FOUND)
    add_executable(regression_cognitive_immune_tick_enhanced
        test_brain_immune_tick_enhanced_regression.c
    )
    target_include_directories(regression_cognitive_immune_tick_enhanced PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CHECK_INCLUDE_DIRS}
    )
    target_link_libraries(regression_cognitive_immune_tick_enhanced
        nimcp
        ${CHECK_LIBRARIES}
        pthread
        m
    )
    add_test(NAME regression_cognitive_immune_tick_enhanced
             COMMAND regression_cognitive_immune_tick_enhanced)
    set_tests_properties(regression_cognitive_immune_tick_enhanced PROPERTIES
        LABELS "regression;cognitive;immune;tick;enhanced;mc;qa;pattern;edge"
        TIMEOUT 180
    )
else()
    message(STATUS "Check library not found - skipping brain immune tick enhanced regression tests")
endif()
