#=============================================================================
# CMakeLists.txt - End-to-End Tests for Subcortical Structures
#=============================================================================
#
# WHAT: Build configuration for subcortical e2e tests
# WHY:  Comprehensive testing of basal ganglia pipeline
# HOW:  Link against subcortical APIs and GTest framework
#
# TEST FILES:
# - test_basal_ganglia_pipeline_e2e.cpp (~15 tests)
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

set(SUBCORTICAL_E2E_LIBS
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)

set(SUBCORTICAL_E2E_INCLUDES
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/core/brain/subcortical
    ${CMAKE_SOURCE_DIR}/test
    ${CMAKE_SOURCE_DIR}/test/e2e
    ${CMAKE_SOURCE_DIR}/test/utils
)

#=============================================================================
# Basal Ganglia Pipeline E2E Tests
#=============================================================================
# WHAT: Test complete BG pipeline with all enhancements
# WHY:  Verify striosome-matrix, chunking, vigor integration
# HOW:  Full pipeline tests with realistic scenarios
#=============================================================================

add_executable(test_basal_ganglia_pipeline_e2e
    test_basal_ganglia_pipeline_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)

target_link_libraries(test_basal_ganglia_pipeline_e2e
    PRIVATE
        ${SUBCORTICAL_E2E_LIBS}
)

target_include_directories(test_basal_ganglia_pipeline_e2e
    PRIVATE
        ${SUBCORTICAL_E2E_INCLUDES}
)

target_compile_features(test_basal_ganglia_pipeline_e2e PRIVATE cxx_std_17)

add_test(NAME test_basal_ganglia_pipeline_e2e
    COMMAND test_basal_ganglia_pipeline_e2e
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_tests_properties(test_basal_ganglia_pipeline_e2e PROPERTIES
    LABELS "e2e;subcortical;basal_ganglia;pipeline"
    TIMEOUT 600
    RUN_SERIAL TRUE
)

#=============================================================================
# Custom Target for Running Subcortical E2E Tests
#=============================================================================

add_custom_target(run_subcortical_e2e_tests
    COMMAND ${CMAKE_CTEST_COMMAND} -L subcortical --output-on-failure
    DEPENDS
        test_basal_ganglia_pipeline_e2e
    COMMENT "Running all subcortical E2E tests"
)

#=============================================================================
# Installation (optional)
#=============================================================================

install(TARGETS
    test_basal_ganglia_pipeline_e2e
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/tests/e2e/subcortical
    OPTIONAL
)

#=============================================================================
# Summary Message
#=============================================================================

message(STATUS "")
message(STATUS "  Configured Subcortical E2E Tests:")
message(STATUS "    - test_basal_ganglia_pipeline_e2e (~15 tests, 600s timeout)")
message(STATUS "")
