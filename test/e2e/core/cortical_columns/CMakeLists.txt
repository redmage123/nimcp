#=============================================================================
# CMakeLists.txt - End-to-End Tests for Cortical Columns Module
#=============================================================================
#
# WHAT: Build configuration for cortical columns e2e tests
# WHY:  Comprehensive testing of cortical column hierarchies, visual processing,
#       sensory integration, learning mechanisms, and pathology/recovery
# HOW:  Link against cortical column APIs and GTest framework
#
# TEST FILES:
# - test_visual_processing_e2e.cpp (~15 tests)
# - test_sensory_hierarchy_e2e.cpp (~15 tests)
# - test_cortical_learning_e2e.cpp (~15 tests)
# - test_cortical_pathology_e2e.cpp (~15 tests)
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

set(CORTICAL_E2E_LIBS
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)

set(CORTICAL_E2E_INCLUDES
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/core/cortical_columns
    ${CMAKE_SOURCE_DIR}/include/core/brain
    ${CMAKE_SOURCE_DIR}/include/async
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/test
    ${CMAKE_SOURCE_DIR}/test/e2e
    ${CMAKE_SOURCE_DIR}/test/utils
)

#=============================================================================
# Visual Processing E2E Tests
#=============================================================================
# WHAT: Test edge detection, feature hierarchy, object recognition
# WHY:  Verify complete visual processing pathways through cortical columns
# HOW:  Test orientation columns, V1->V2->V4 hierarchy, attention, predictive coding
#=============================================================================

add_executable(test_visual_processing_e2e
    test_visual_processing_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)

target_link_libraries(test_visual_processing_e2e
    PRIVATE
        ${CORTICAL_E2E_LIBS}
)

target_include_directories(test_visual_processing_e2e
    PRIVATE
        ${CORTICAL_E2E_INCLUDES}
)

target_compile_features(test_visual_processing_e2e PRIVATE cxx_std_17)

add_test(NAME test_visual_processing_e2e
    COMMAND test_visual_processing_e2e
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_tests_properties(test_visual_processing_e2e PROPERTIES
    LABELS "e2e;cortical;visual;orientation;hierarchy;predictive"
    TIMEOUT 300
    RUN_SERIAL TRUE
)

#=============================================================================
# Sensory Hierarchy E2E Tests
#=============================================================================
# WHAT: Test multi-level sensory processing with feedforward/feedback
# WHY:  Verify hierarchical sensory integration and attention mechanisms
# HOW:  Test bottom-up extraction, top-down prediction, cross-modal integration
#=============================================================================

add_executable(test_sensory_hierarchy_e2e
    test_sensory_hierarchy_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)

target_link_libraries(test_sensory_hierarchy_e2e
    PRIVATE
        ${CORTICAL_E2E_LIBS}
)

target_include_directories(test_sensory_hierarchy_e2e
    PRIVATE
        ${CORTICAL_E2E_INCLUDES}
)

target_compile_features(test_sensory_hierarchy_e2e PRIVATE cxx_std_17)

add_test(NAME test_sensory_hierarchy_e2e
    COMMAND test_sensory_hierarchy_e2e
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_tests_properties(test_sensory_hierarchy_e2e PROPERTIES
    LABELS "e2e;cortical;sensory;hierarchy;attention;temporal"
    TIMEOUT 300
    RUN_SERIAL TRUE
)

#=============================================================================
# Cortical Learning E2E Tests
#=============================================================================
# WHAT: Test Hebbian learning, competitive learning, plasticity
# WHY:  Verify learning mechanisms in cortical column architecture
# HOW:  Test synaptic plasticity, sparse coding, experience-dependent tuning
#=============================================================================

add_executable(test_cortical_learning_e2e
    test_cortical_learning_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)

target_link_libraries(test_cortical_learning_e2e
    PRIVATE
        ${CORTICAL_E2E_LIBS}
)

target_include_directories(test_cortical_learning_e2e
    PRIVATE
        ${CORTICAL_E2E_INCLUDES}
)

target_compile_features(test_cortical_learning_e2e PRIVATE cxx_std_17)

add_test(NAME test_cortical_learning_e2e
    COMMAND test_cortical_learning_e2e
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_tests_properties(test_cortical_learning_e2e PROPERTIES
    LABELS "e2e;cortical;learning;hebbian;plasticity;competitive"
    TIMEOUT 600
    RUN_SERIAL TRUE
)

#=============================================================================
# Cortical Pathology E2E Tests
#=============================================================================
# WHAT: Test cortical dysfunction and recovery mechanisms
# WHY:  Verify system behavior under pathological conditions
# HOW:  Test lesion simulation, hyperexcitability, recovery strategies
#=============================================================================

add_executable(test_cortical_pathology_e2e
    test_cortical_pathology_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)

target_link_libraries(test_cortical_pathology_e2e
    PRIVATE
        ${CORTICAL_E2E_LIBS}
)

target_include_directories(test_cortical_pathology_e2e
    PRIVATE
        ${CORTICAL_E2E_INCLUDES}
)

target_compile_features(test_cortical_pathology_e2e PRIVATE cxx_std_17)

add_test(NAME test_cortical_pathology_e2e
    COMMAND test_cortical_pathology_e2e
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_tests_properties(test_cortical_pathology_e2e PROPERTIES
    LABELS "e2e;cortical;pathology;lesion;recovery;compensation"
    TIMEOUT 600
    RUN_SERIAL TRUE
)

#=============================================================================
# Combined Cortical Columns E2E Test Target
#=============================================================================

add_executable(test_cortical_columns_e2e_all
    test_visual_processing_e2e.cpp
    test_sensory_hierarchy_e2e.cpp
    test_cortical_learning_e2e.cpp
    test_cortical_pathology_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)

target_link_libraries(test_cortical_columns_e2e_all
    PRIVATE
        ${CORTICAL_E2E_LIBS}
)

target_include_directories(test_cortical_columns_e2e_all
    PRIVATE
        ${CORTICAL_E2E_INCLUDES}
)

target_compile_features(test_cortical_columns_e2e_all PRIVATE cxx_std_17)

add_test(NAME test_cortical_columns_e2e_all
    COMMAND test_cortical_columns_e2e_all
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_tests_properties(test_cortical_columns_e2e_all PROPERTIES
    LABELS "e2e;cortical;all"
    TIMEOUT 1800
    RUN_SERIAL TRUE
)

#=============================================================================
# Custom Target for Running All Cortical Columns E2E Tests
#=============================================================================

add_custom_target(run_cortical_columns_e2e_tests
    COMMAND ${CMAKE_CTEST_COMMAND} -L cortical --output-on-failure
    DEPENDS
        test_visual_processing_e2e
        test_sensory_hierarchy_e2e
        test_cortical_learning_e2e
        test_cortical_pathology_e2e
    COMMENT "Running all cortical columns E2E tests"
)

#=============================================================================
# Installation (optional)
#=============================================================================

# Install test binaries if needed for CI/CD
install(TARGETS
    test_visual_processing_e2e
    test_sensory_hierarchy_e2e
    test_cortical_learning_e2e
    test_cortical_pathology_e2e
    test_cortical_columns_e2e_all
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/tests/e2e/cortical_columns
    OPTIONAL
)

#=============================================================================
# Summary Message
#=============================================================================

message(STATUS "")
message(STATUS "  Configured Cortical Columns E2E Tests:")
message(STATUS "    - test_visual_processing_e2e    (~15 tests, 300s timeout)")
message(STATUS "    - test_sensory_hierarchy_e2e    (~15 tests, 300s timeout)")
message(STATUS "    - test_cortical_learning_e2e    (~15 tests, 600s timeout)")
message(STATUS "    - test_cortical_pathology_e2e   (~15 tests, 600s timeout)")
message(STATUS "    - test_cortical_columns_e2e_all (all tests, 1800s timeout)")
message(STATUS "")
