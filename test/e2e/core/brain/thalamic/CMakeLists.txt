#=============================================================================
# CMakeLists.txt - End-to-End Tests for Thalamic Layer
#=============================================================================
#
# WHAT: Build configuration for thalamic layer e2e tests
# WHY:  Comprehensive testing of thalamic relay, routing, and immune integration
# HOW:  Link against thalamic APIs and GTest framework
#
# TEST FILES:
# - test_sensory_relay_e2e.cpp (~15 tests)
# - test_thalamocortical_loop_e2e.cpp (~15 tests)
# - test_basal_ganglia_thalamus_e2e.cpp (~15 tests)
# - test_thalamic_pathology_e2e.cpp (~15 tests)
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

#=============================================================================
# Common Configuration
#=============================================================================

set(THALAMIC_E2E_LIBS
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)

set(THALAMIC_E2E_INCLUDES
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/core/brain/subcortical
    ${CMAKE_SOURCE_DIR}/include/middleware/routing
    ${CMAKE_SOURCE_DIR}/include/middleware/immune
    ${CMAKE_SOURCE_DIR}/test
    ${CMAKE_SOURCE_DIR}/test/e2e
    ${CMAKE_SOURCE_DIR}/test/utils
)

#=============================================================================
# Sensory Relay E2E Tests
#=============================================================================
# WHAT: Test visual, auditory, somatosensory pathways through thalamus
# WHY:  Verify complete sensory relay with attention and TRN gating
# HOW:  Full pathway tests with realistic stimuli
#=============================================================================

add_executable(test_sensory_relay_e2e
    test_sensory_relay_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)

target_link_libraries(test_sensory_relay_e2e
    PRIVATE
        ${THALAMIC_E2E_LIBS}
)

target_include_directories(test_sensory_relay_e2e
    PRIVATE
        ${THALAMIC_E2E_INCLUDES}
)

target_compile_features(test_sensory_relay_e2e PRIVATE cxx_std_17)

add_test(NAME test_sensory_relay_e2e
    COMMAND test_sensory_relay_e2e
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_tests_properties(test_sensory_relay_e2e PROPERTIES
    LABELS "e2e;thalamic;sensory;relay"
    TIMEOUT 300
    RUN_SERIAL TRUE
)

#=============================================================================
# Thalamocortical Loop E2E Tests
#=============================================================================
# WHAT: Test corticothalamic feedback and oscillatory dynamics
# WHY:  Verify bidirectional thalamus-cortex communication
# HOW:  Test sleep/wake transitions, attention, oscillations
#=============================================================================

add_executable(test_thalamocortical_loop_e2e
    test_thalamocortical_loop_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)

target_link_libraries(test_thalamocortical_loop_e2e
    PRIVATE
        ${THALAMIC_E2E_LIBS}
)

target_include_directories(test_thalamocortical_loop_e2e
    PRIVATE
        ${THALAMIC_E2E_INCLUDES}
)

target_compile_features(test_thalamocortical_loop_e2e PRIVATE cxx_std_17)

add_test(NAME test_thalamocortical_loop_e2e
    COMMAND test_thalamocortical_loop_e2e
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_tests_properties(test_thalamocortical_loop_e2e PROPERTIES
    LABELS "e2e;thalamic;thalamocortical;loop;oscillations"
    TIMEOUT 300
    RUN_SERIAL TRUE
)

#=============================================================================
# Basal Ganglia-Thalamus E2E Tests
#=============================================================================
# WHAT: Test BG-thalamus motor pathway integration
# WHY:  Verify action selection through complete motor circuit
# HOW:  Test direct/indirect pathways, STN, dopamine effects
#=============================================================================

add_executable(test_basal_ganglia_thalamus_e2e
    test_basal_ganglia_thalamus_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)

target_link_libraries(test_basal_ganglia_thalamus_e2e
    PRIVATE
        ${THALAMIC_E2E_LIBS}
)

target_include_directories(test_basal_ganglia_thalamus_e2e
    PRIVATE
        ${THALAMIC_E2E_INCLUDES}
)

target_compile_features(test_basal_ganglia_thalamus_e2e PRIVATE cxx_std_17)

add_test(NAME test_basal_ganglia_thalamus_e2e
    COMMAND test_basal_ganglia_thalamus_e2e
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_tests_properties(test_basal_ganglia_thalamus_e2e PROPERTIES
    LABELS "e2e;thalamic;basal_ganglia;motor;dopamine"
    TIMEOUT 600
    RUN_SERIAL TRUE
)

#=============================================================================
# Thalamic Pathology E2E Tests
#=============================================================================
# WHAT: Test thalamic dysfunction and recovery scenarios
# WHY:  Verify pathological conditions and therapeutic interventions
# HOW:  Test lesions, seizures, inflammation, stroke recovery
#=============================================================================

add_executable(test_thalamic_pathology_e2e
    test_thalamic_pathology_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)

target_link_libraries(test_thalamic_pathology_e2e
    PRIVATE
        ${THALAMIC_E2E_LIBS}
)

target_include_directories(test_thalamic_pathology_e2e
    PRIVATE
        ${THALAMIC_E2E_INCLUDES}
)

target_compile_features(test_thalamic_pathology_e2e PRIVATE cxx_std_17)

add_test(NAME test_thalamic_pathology_e2e
    COMMAND test_thalamic_pathology_e2e
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_tests_properties(test_thalamic_pathology_e2e PROPERTIES
    LABELS "e2e;thalamic;pathology;lesion;seizure;inflammation"
    TIMEOUT 600
    RUN_SERIAL TRUE
)

#=============================================================================
# Combined Thalamic E2E Test Target
#=============================================================================

add_executable(test_thalamic_e2e_all
    test_sensory_relay_e2e.cpp
    test_thalamocortical_loop_e2e.cpp
    test_basal_ganglia_thalamus_e2e.cpp
    test_thalamic_pathology_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)

target_link_libraries(test_thalamic_e2e_all
    PRIVATE
        ${THALAMIC_E2E_LIBS}
)

target_include_directories(test_thalamic_e2e_all
    PRIVATE
        ${THALAMIC_E2E_INCLUDES}
)

target_compile_features(test_thalamic_e2e_all PRIVATE cxx_std_17)

add_test(NAME test_thalamic_e2e_all
    COMMAND test_thalamic_e2e_all
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_tests_properties(test_thalamic_e2e_all PROPERTIES
    LABELS "e2e;thalamic;all"
    TIMEOUT 1200
    RUN_SERIAL TRUE
)

#=============================================================================
# Custom Target for Running All Thalamic E2E Tests
#=============================================================================

add_custom_target(run_thalamic_e2e_tests
    COMMAND ${CMAKE_CTEST_COMMAND} -L thalamic --output-on-failure
    DEPENDS
        test_sensory_relay_e2e
        test_thalamocortical_loop_e2e
        test_basal_ganglia_thalamus_e2e
        test_thalamic_pathology_e2e
    COMMENT "Running all thalamic layer E2E tests"
)

#=============================================================================
# Installation (optional)
#=============================================================================

# Install test binaries if needed for CI/CD
install(TARGETS
    test_sensory_relay_e2e
    test_thalamocortical_loop_e2e
    test_basal_ganglia_thalamus_e2e
    test_thalamic_pathology_e2e
    test_thalamic_e2e_all
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/tests/e2e/thalamic
    OPTIONAL
)

#=============================================================================
# Summary Message
#=============================================================================

message(STATUS "")
message(STATUS "  Configured Thalamic E2E Tests:")
message(STATUS "    - test_sensory_relay_e2e        (~15 tests, 300s timeout)")
message(STATUS "    - test_thalamocortical_loop_e2e (~15 tests, 300s timeout)")
message(STATUS "    - test_basal_ganglia_thalamus_e2e (~15 tests, 600s timeout)")
message(STATUS "    - test_thalamic_pathology_e2e   (~15 tests, 600s timeout)")
message(STATUS "    - test_thalamic_e2e_all         (all tests, 1200s timeout)")
message(STATUS "")
