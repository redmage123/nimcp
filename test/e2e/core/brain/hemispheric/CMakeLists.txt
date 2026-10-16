#=============================================================================
# CMakeLists.txt - End-to-End Tests for Hemispheric Brain Layer
#=============================================================================
#
# WHAT: Build configuration for hemispheric brain e2e tests
# WHY:  Comprehensive testing of bilateral brain architecture
# HOW:  Link against hemispheric brain APIs and GTest framework
#
# TEST FILES:
# - test_language_lateralization_e2e.cpp (~15 tests)
# - test_spatial_processing_e2e.cpp (~15 tests)
# - test_split_brain_e2e.cpp (~20 tests)
# - test_injury_recovery_e2e.cpp (~15 tests)
#
# TOTAL: ~65 tests
#
#=============================================================================

# Require minimum CMake version
cmake_minimum_required(VERSION 3.14)

# Get parent e2e test include directories
include_directories(${CMAKE_SOURCE_DIR}/test/e2e)
include_directories(${CMAKE_SOURCE_DIR}/test)
include_directories(${CMAKE_SOURCE_DIR}/include)

#=============================================================================
# Test Source Files
#=============================================================================

set(HEMISPHERIC_E2E_SOURCES
    test_language_lateralization_e2e.cpp
    test_spatial_processing_e2e.cpp
    test_split_brain_e2e.cpp
    test_injury_recovery_e2e.cpp
)

#=============================================================================
# Language Lateralization E2E Tests
#=============================================================================

add_executable(test_language_lateralization_e2e
    test_language_lateralization_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)

target_link_libraries(test_language_lateralization_e2e
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
)

target_include_directories(test_language_lateralization_e2e
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/e2e
        ${CMAKE_SOURCE_DIR}/test/utils
)

add_test(NAME test_language_lateralization_e2e
    COMMAND test_language_lateralization_e2e
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_tests_properties(test_language_lateralization_e2e PROPERTIES
    LABELS "e2e;hemispheric;language"
    TIMEOUT 300
)

#=============================================================================
# Spatial Processing E2E Tests
#=============================================================================

add_executable(test_spatial_processing_e2e
    test_spatial_processing_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)

target_link_libraries(test_spatial_processing_e2e
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
)

target_include_directories(test_spatial_processing_e2e
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/e2e
        ${CMAKE_SOURCE_DIR}/test/utils
)

add_test(NAME test_spatial_processing_e2e
    COMMAND test_spatial_processing_e2e
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_tests_properties(test_spatial_processing_e2e PROPERTIES
    LABELS "e2e;hemispheric;spatial"
    TIMEOUT 300
)

#=============================================================================
# Split Brain E2E Tests
#=============================================================================

add_executable(test_split_brain_e2e
    test_split_brain_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)

target_link_libraries(test_split_brain_e2e
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
)

target_include_directories(test_split_brain_e2e
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/e2e
        ${CMAKE_SOURCE_DIR}/test/utils
)

add_test(NAME test_split_brain_e2e
    COMMAND test_split_brain_e2e
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_tests_properties(test_split_brain_e2e PROPERTIES
    LABELS "e2e;hemispheric;split_brain"
    TIMEOUT 600
)

#=============================================================================
# Injury Recovery E2E Tests
#=============================================================================

add_executable(test_injury_recovery_e2e
    test_injury_recovery_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)

target_link_libraries(test_injury_recovery_e2e
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
)

target_include_directories(test_injury_recovery_e2e
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/e2e
        ${CMAKE_SOURCE_DIR}/test/utils
)

add_test(NAME test_injury_recovery_e2e
    COMMAND test_injury_recovery_e2e
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_tests_properties(test_injury_recovery_e2e PROPERTIES
    LABELS "e2e;hemispheric;injury;recovery"
    TIMEOUT 600
)

#=============================================================================
# Combined Hemispheric E2E Test Target
#=============================================================================

add_executable(test_hemispheric_e2e_all
    test_language_lateralization_e2e.cpp
    test_spatial_processing_e2e.cpp
    test_split_brain_e2e.cpp
    test_injury_recovery_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)

target_link_libraries(test_hemispheric_e2e_all
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
)

target_include_directories(test_hemispheric_e2e_all
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/e2e
        ${CMAKE_SOURCE_DIR}/test/utils
)

add_test(NAME test_hemispheric_e2e_all
    COMMAND test_hemispheric_e2e_all
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_tests_properties(test_hemispheric_e2e_all PROPERTIES
    LABELS "e2e;hemispheric;all"
    TIMEOUT 1200
)

#=============================================================================
# Custom Target for Running All Hemispheric E2E Tests
#=============================================================================

add_custom_target(run_hemispheric_e2e_tests
    COMMAND ${CMAKE_CTEST_COMMAND} -L hemispheric --output-on-failure
    DEPENDS
        test_language_lateralization_e2e
        test_spatial_processing_e2e
        test_split_brain_e2e
        test_injury_recovery_e2e
    COMMENT "Running all hemispheric brain E2E tests"
)

#=============================================================================
# Installation (optional)
#=============================================================================

# Install test binaries if needed for CI/CD
install(TARGETS
    test_language_lateralization_e2e
    test_spatial_processing_e2e
    test_split_brain_e2e
    test_injury_recovery_e2e
    test_hemispheric_e2e_all
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/tests/e2e/hemispheric
    OPTIONAL
)
