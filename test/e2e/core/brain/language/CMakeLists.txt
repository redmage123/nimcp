#=============================================================================
# CMakeLists.txt - End-to-End Tests for Language Processing (Broca/Wernicke)
#=============================================================================
#
# WHAT: Build configuration for language processing e2e tests
# WHY:  Comprehensive testing of Broca's and Wernicke's area integration
# HOW:  Link against language region APIs and GTest framework
#
# TEST FILES:
# - test_speech_production_e2e.cpp (~15 tests) - Full production pipeline
# - test_language_comprehension_e2e.cpp (~15 tests) - Full comprehension pipeline
# - test_language_loop_e2e.cpp (~15 tests) - Wernicke-Broca loop
# - test_language_pathology_e2e.cpp (~15 tests) - Aphasia simulation
#
# TOTAL: ~60 tests
#
#=============================================================================

# Require minimum CMake version
cmake_minimum_required(VERSION 3.14)

# Get parent e2e test include directories
include_directories(${CMAKE_SOURCE_DIR}/test/e2e)
include_directories(${CMAKE_SOURCE_DIR}/test)
include_directories(${CMAKE_SOURCE_DIR}/include)

message(STATUS "")
message(STATUS "-- Configuring Language Processing E2E Tests")

#=============================================================================
# Speech Production E2E Tests
#=============================================================================

add_executable(test_speech_production_e2e
    test_speech_production_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)

target_link_libraries(test_speech_production_e2e
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(test_speech_production_e2e
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/e2e
        ${CMAKE_SOURCE_DIR}/test/utils
)

add_test(NAME test_speech_production_e2e
    COMMAND test_speech_production_e2e
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_tests_properties(test_speech_production_e2e PROPERTIES
    LABELS "e2e;language;broca;production"
    TIMEOUT 300
    RUN_SERIAL TRUE
)

#=============================================================================
# Language Comprehension E2E Tests
#=============================================================================

add_executable(test_language_comprehension_e2e
    test_language_comprehension_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)

target_link_libraries(test_language_comprehension_e2e
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(test_language_comprehension_e2e
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/e2e
        ${CMAKE_SOURCE_DIR}/test/utils
)

add_test(NAME test_language_comprehension_e2e
    COMMAND test_language_comprehension_e2e
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_tests_properties(test_language_comprehension_e2e PROPERTIES
    LABELS "e2e;language;wernicke;comprehension"
    TIMEOUT 300
    RUN_SERIAL TRUE
)

#=============================================================================
# Language Loop E2E Tests (Wernicke-Broca Connection)
#=============================================================================

add_executable(test_language_loop_e2e
    test_language_loop_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)

target_link_libraries(test_language_loop_e2e
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(test_language_loop_e2e
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/e2e
        ${CMAKE_SOURCE_DIR}/test/utils
)

add_test(NAME test_language_loop_e2e
    COMMAND test_language_loop_e2e
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_tests_properties(test_language_loop_e2e PROPERTIES
    LABELS "e2e;language;loop;arcuate"
    TIMEOUT 600
    RUN_SERIAL TRUE
)

#=============================================================================
# Language Pathology E2E Tests (Aphasia Simulation)
#=============================================================================

add_executable(test_language_pathology_e2e
    test_language_pathology_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)

target_link_libraries(test_language_pathology_e2e
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(test_language_pathology_e2e
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/e2e
        ${CMAKE_SOURCE_DIR}/test/utils
)

add_test(NAME test_language_pathology_e2e
    COMMAND test_language_pathology_e2e
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_tests_properties(test_language_pathology_e2e PROPERTIES
    LABELS "e2e;language;pathology;aphasia"
    TIMEOUT 600
    RUN_SERIAL TRUE
)

#=============================================================================
# Combined Language E2E Test Target
#=============================================================================

add_executable(test_language_e2e_all
    test_speech_production_e2e.cpp
    test_language_comprehension_e2e.cpp
    test_language_loop_e2e.cpp
    test_language_pathology_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)

target_link_libraries(test_language_e2e_all
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(test_language_e2e_all
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/e2e
        ${CMAKE_SOURCE_DIR}/test/utils
)

add_test(NAME test_language_e2e_all
    COMMAND test_language_e2e_all
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_tests_properties(test_language_e2e_all PROPERTIES
    LABELS "e2e;language;all"
    TIMEOUT 1200
    RUN_SERIAL TRUE
)

#=============================================================================
# Custom Target for Running All Language E2E Tests
#=============================================================================

add_custom_target(run_language_e2e_tests
    COMMAND ${CMAKE_CTEST_COMMAND} -L language --output-on-failure
    DEPENDS
        test_speech_production_e2e
        test_language_comprehension_e2e
        test_language_loop_e2e
        test_language_pathology_e2e
    COMMENT "Running all language processing E2E tests"
)

#=============================================================================
# Installation (optional)
#=============================================================================

# Install test binaries if needed for CI/CD
install(TARGETS
    test_speech_production_e2e
    test_language_comprehension_e2e
    test_language_loop_e2e
    test_language_pathology_e2e
    test_language_e2e_all
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/tests/e2e/language
    OPTIONAL
)
