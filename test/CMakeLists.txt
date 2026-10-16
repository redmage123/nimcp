# ============================================================================
# NIMCP Test Framework - Code Surgeon Integration
# ============================================================================
# WHAT: Build system for migrated test framework
# WHY:  Enable Code Surgeon automated testing with parallel execution
# HOW:  Discover all tests by category, build individually for parallelism
#
# STRUCTURE:
# - test/unit/        - Unit tests (per-module testing)
# - test/integration/ - Integration tests (cross-module)
# - test/e2e/         - End-to-end tests (full system)
# - test/regression/  - Regression tests (bug reproduction)
# - test/fuzz/        - Fuzzing tests (property-based)
#
# NIMCP STANDARDS:
# - All functions < 50 lines
# - Guard clauses (early returns)
# - WHAT-WHY-HOW documentation
# ============================================================================

# Guard: Check GTest availability
if(NOT GTest_FOUND)
    message(STATUS "GTest not found - skipping test framework")
    return()
endif()

# ============================================================================
# Code Coverage Configuration (gcov/lcov)
# ============================================================================
# WHAT: Enable code coverage reporting with gcov/lcov
# WHY:  Track test coverage to identify untested code paths
# HOW:  Add compiler flags for coverage instrumentation, provide report targets
#
# USAGE:
#   cmake -DNIMCP_COVERAGE=ON ..
#   make
#   ctest
#   make coverage_report
# ============================================================================

option(NIMCP_COVERAGE "Enable code coverage reporting with gcov/lcov" OFF)

if(NIMCP_COVERAGE)
    message(STATUS "Code coverage enabled")

    # Check for required tools
    find_program(GCOV_PATH gcov)
    find_program(LCOV_PATH lcov)
    find_program(GENHTML_PATH genhtml)

    if(NOT GCOV_PATH)
        message(WARNING "gcov not found - coverage reporting will be incomplete")
    endif()

    if(NOT LCOV_PATH)
        message(WARNING "lcov not found - coverage report generation disabled")
    endif()

    if(NOT GENHTML_PATH)
        message(WARNING "genhtml not found - HTML coverage reports disabled")
    endif()

    # Add coverage flags globally for test builds
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")

    # Coverage output directory
    set(COVERAGE_DIR ${CMAKE_BINARY_DIR}/coverage)

    # Create coverage report target
    if(LCOV_PATH AND GENHTML_PATH)
        add_custom_target(coverage_report
            COMMENT "Generating code coverage report..."

            # Create coverage directory
            COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_DIR}

            # Capture coverage data (baseline)
            COMMAND ${LCOV_PATH} --capture --initial
                --directory ${CMAKE_BINARY_DIR}
                --output-file ${COVERAGE_DIR}/coverage_base.info
                --gcov-tool ${GCOV_PATH}
                --ignore-errors mismatch

            # Run tests
            COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure || true

            # Capture coverage data (after tests)
            COMMAND ${LCOV_PATH} --capture
                --directory ${CMAKE_BINARY_DIR}
                --output-file ${COVERAGE_DIR}/coverage_test.info
                --gcov-tool ${GCOV_PATH}
                --ignore-errors mismatch

            # Combine baseline and test coverage
            COMMAND ${LCOV_PATH}
                --add-tracefile ${COVERAGE_DIR}/coverage_base.info
                --add-tracefile ${COVERAGE_DIR}/coverage_test.info
                --output-file ${COVERAGE_DIR}/coverage_total.info
                --ignore-errors mismatch

            # Remove system and test files from coverage
            COMMAND ${LCOV_PATH} --remove ${COVERAGE_DIR}/coverage_total.info
                '/usr/*'
                '*/test/*'
                '*/gtest/*'
                '*/gmock/*'
                '*/_deps/*'
                --output-file ${COVERAGE_DIR}/coverage_filtered.info
                --ignore-errors mismatch

            # Generate HTML report
            COMMAND ${GENHTML_PATH} ${COVERAGE_DIR}/coverage_filtered.info
                --output-directory ${COVERAGE_DIR}/html
                --title "NIMCP Code Coverage Report"
                --legend
                --show-details
                --highlight

            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

        message(STATUS "  Coverage report target: make coverage_report")
        message(STATUS "  Report location: ${COVERAGE_DIR}/html/index.html")
    endif()

    # Create coverage clean target
    add_custom_target(coverage_clean
        COMMENT "Cleaning coverage data..."
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${COVERAGE_DIR}
        COMMAND find ${CMAKE_BINARY_DIR} -name "*.gcda" -delete || true
        COMMAND find ${CMAKE_BINARY_DIR} -name "*.gcno" -delete || true
        COMMAND find ${CMAKE_BINARY_DIR} -name "*.gcov" -delete || true
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    message(STATUS "  Coverage clean target: make coverage_clean")
endif()

# ============================================================================
# ThreadSanitizer Configuration
# ============================================================================
# WHAT: Enable ThreadSanitizer for race condition detection
# WHY:  Identify concurrency bugs in brain operations
# HOW:  Add TSan compiler flags when enabled
#
# USAGE:
#   cmake -DNIMCP_TSAN=ON ..
#   make
#   ctest
# ============================================================================

option(NIMCP_TSAN "Enable ThreadSanitizer for race detection" OFF)

if(NIMCP_TSAN)
    message(STATUS "ThreadSanitizer enabled")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread -g")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -g")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")

    # TSan is incompatible with ASan
    if(NIMCP_ASAN)
        message(FATAL_ERROR "Cannot enable both ThreadSanitizer and AddressSanitizer")
    endif()
endif()

# ============================================================================
# AddressSanitizer Configuration
# ============================================================================
# WHAT: Enable AddressSanitizer for memory error detection
# WHY:  Identify memory bugs (buffer overflow, use-after-free, etc.)
# HOW:  Add ASan compiler flags when enabled
#
# USAGE:
#   cmake -DNIMCP_ASAN=ON ..
#   make
#   ctest
# ============================================================================

option(NIMCP_ASAN "Enable AddressSanitizer for memory error detection" OFF)

if(NIMCP_ASAN)
    message(STATUS "AddressSanitizer enabled")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer -g")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -g")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
endif()

# ============================================================================
# Common Test Configuration
# ============================================================================

# Define directory variables
set(NIMCP_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
set(NIMCP_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(NIMCP_LIB_DIR ${CMAKE_SOURCE_DIR}/src/lib)

set(COMMON_TEST_INCLUDES
    ${NIMCP_INCLUDE_DIR}
    ${NIMCP_TEST_DIR}
    ${NIMCP_TEST_DIR}/utils
    ${NIMCP_LIB_DIR}
    ${GTEST_INCLUDE_DIRS}
    ${Python3_INCLUDE_DIRS}
)

set(COMMON_TEST_LIBS
    nimcp
    GTest::GTest
    GTest::Main
    Python3::Python
    pthread
)

# ============================================================================
# Helper Function: Create Test Binary
# ============================================================================
# WHAT: Create test executable with standard NIMCP configuration
# WHY:  Ensure consistent build settings across all tests
# HOW:  Configure includes, libs, coverage, CTest registration
#
# PARAMETERS:
#   TARGET_NAME - Name of test executable
#   SOURCE_FILE - Test source file
#   CATEGORY    - Test category (unit/integration/e2e/regression/fuzz)
# ============================================================================
function(add_test_binary TARGET_NAME SOURCE_FILE CATEGORY)
    # Guard: Validate source file exists
    if(NOT EXISTS ${SOURCE_FILE})
        message(WARNING "Test source not found: ${SOURCE_FILE}")
        return()
    endif()

    # Guard: Skip if target already exists (avoid duplicate target errors)
    if(TARGET ${TARGET_NAME})
        return()
    endif()

    # Create executable
    add_executable(${TARGET_NAME} ${SOURCE_FILE})

    # Add explicit dependency on nimcp library
    add_dependencies(${TARGET_NAME} nimcp)

    # Configure includes
    target_include_directories(${TARGET_NAME} PRIVATE ${COMMON_TEST_INCLUDES})

    # Enable C++20 for designated initializers
    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)

    # Enable test-only APIs
    target_compile_definitions(${TARGET_NAME} PRIVATE NIMCP_TESTING)

    # Add Python module source
    target_sources(${TARGET_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/src/python/nimcp_module.c
    )

    # Link libraries
    target_link_libraries(${TARGET_NAME}
    PRIVATE
        nimcp
        nimcp_lnn
        GTest::GTest
        GTest::Main
        Python3::Python
        pthread
)

    # Set properties
    set_target_properties(${TARGET_NAME} PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )

    # Enable coverage for Debug builds
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        if(CMAKE_COMPILER_IS_GNUCXX)
            target_compile_options(${TARGET_NAME} PRIVATE --coverage)
            target_link_options(${TARGET_NAME} PRIVATE --coverage)
        endif()
    endif()

    # Register with CTest
    add_test(NAME ${TARGET_NAME}
             COMMAND ${TARGET_NAME}
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

    # Set test properties
    set_tests_properties(${TARGET_NAME} PROPERTIES
        ENVIRONMENT "PYTHONPATH=${CMAKE_BINARY_DIR}/lib/python;LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/lsan.supp"
        LABELS ${CATEGORY}
    )
endfunction()

# ============================================================================
# Helper Function: Discover Tests in Category (Recursive)
# ============================================================================
# WHAT: Recursively find all test files in a category directory tree
# WHY:  Support modular organization of tests by subdirectory
# HOW:  GLOB_RECURSE for test files, create executable for each
#
# PARAMETERS:
#   CATEGORY - Test category subdirectory name (unit/integration/regression)
# ============================================================================
function(discover_category_tests CATEGORY)
    # Build path to category directory
    set(CATEGORY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${CATEGORY})

    # Guard: Check if category directory exists
    if(NOT EXISTS ${CATEGORY_DIR})
        message(STATUS "Category directory not found: ${CATEGORY}")
        return()
    endif()

    # Recursively find all test files in subdirectories
    file(GLOB_RECURSE TEST_FILES "${CATEGORY_DIR}/*.cpp" "${CATEGORY_DIR}/*.c")

    # Guard: No tests found
    if(NOT TEST_FILES)
        message(STATUS "No tests found in ${CATEGORY}/")
        return()
    endif()

    # Create executable for each test file
    foreach(TEST_FILE ${TEST_FILES})
        # Check if test file's directory has its own CMakeLists.txt
        # If it does, skip this test file (it's managed by the subdirectory CMakeLists.txt)
        get_filename_component(TEST_DIR ${TEST_FILE} DIRECTORY)
        if(EXISTS "${TEST_DIR}/CMakeLists.txt")
            # Skip - this test is managed by subdirectory CMakeLists.txt
            continue()
        endif()

        # Extract test name from filename
        get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)

        # Get relative path from category dir to determine module
        file(RELATIVE_PATH REL_PATH ${CATEGORY_DIR} ${TEST_FILE})
        get_filename_component(MODULE_PATH ${REL_PATH} DIRECTORY)

        # Create unique target name: category_module_testname
        # Replace / with _ in module path for valid target name
        string(REPLACE "/" "_" MODULE_NAME "${MODULE_PATH}")
        if(MODULE_NAME)
            set(TARGET_NAME "${CATEGORY}_${MODULE_NAME}_${TEST_NAME}")
        else()
            set(TARGET_NAME "${CATEGORY}_${TEST_NAME}")
        endif()

        # Create test binary
        add_test_binary(${TARGET_NAME} ${TEST_FILE} ${CATEGORY})
    endforeach()

    # Report
    list(LENGTH TEST_FILES TEST_COUNT)
    message(STATUS "  ${CATEGORY}: ${TEST_COUNT} tests (across all modules)")
endfunction()

# ============================================================================
# Discover and Build All Tests
# ============================================================================

message(STATUS "")
message(STATUS "===============================================================================")
message(STATUS "NIMCP Test Framework - Code Surgeon Integration")
message(STATUS "===============================================================================")

# Discover tests in each category
discover_category_tests(unit)
discover_category_tests(integration)
discover_category_tests(regression)
discover_category_tests(fuzz)

# E2E tests have their own CMakeLists.txt (uses custom build rules)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/e2e")
    message(STATUS "Adding E2E test subdirectory...")
    add_subdirectory(e2e)
endif()

# NOTE: enable_testing() is already called in the top-level CMakeLists.txt.
# Do not call it again here — it only needs to be called once per project.

# ============================================================================
# Timeout Configuration for Slow Tests
# ============================================================================
# WHAT: Extend timeout for computationally intensive tests
# WHY:  Prevent false failures on slower hardware or Debug builds
# HOW:  Set 60s timeout for tests that require extensive computation

# NOTE: Only set properties for tests that actually exist
# Timeout values set based on test complexity and computation requirements
set_tests_properties(
  unit_utils_test_kdtree
  PROPERTIES TIMEOUT 60
)

# Integration tests with heavyweight brain initialization
# Note: These tests may not exist in all configurations - set properties conditionally
# set_tests_properties requires tests to exist, so we use cmake_test_exists check
# Removed: integration_cognitive_executive_test_executive_portia_integration (doesn't exist)
# Removed: integration_cognitive_consolidation_test_systems_consolidation_integration (doesn't exist)
# Removed: integration_cognitive_curiosity_test_curiosity_fep_bridge_integration (doesn't exist)

# Disabled test that doesn't exist
# set_tests_properties(
#   unit_cognitive_knowledge_test_knowledge_comprehensive
#   PROPERTIES TIMEOUT 120
# )

# Set properties only if tests exist
if(TARGET integration_test_brain_oscillations_integration)
  set_tests_properties(
    integration_test_brain_oscillations_integration
    PROPERTIES TIMEOUT 90
  )
endif()

# Brain API tests require extended timeout due to heavyweight brain initialization
# Each brain creation initializes 50+ subsystems (neural network, cognitive systems,
# plasticity mechanisms, immune system, ethics, etc.)
# See: BRAIN_API_TEST_TIMEOUT_ANALYSIS.md for detailed analysis
if(TARGET unit_api_test_api_brain_lifecycle)
  set_tests_properties(
    unit_api_test_api_brain_lifecycle
    unit_api_test_api_brain_cow
    unit_api_test_api_brain_operations
    unit_api_test_api_brain_persistence
    unit_api_test_api_brain_snapshots
    PROPERTIES TIMEOUT 180
  )
endif()

# Additional brain API tests that create brains in setUp/tearDown
# These need extended timeouts due to per-test brain creation overhead
if(TARGET unit_api_test_api_complex)
  set_tests_properties(
    unit_api_test_api_complex
    unit_api_test_api_error_handling
    unit_api_test_api_working_memory
    PROPERTIES TIMEOUT 180
  )
endif()

# API safety fix tests: 7 tests with per-test brain create/destroy + training steps
if(TARGET unit_api_test_api_safety_fixes)
  set_tests_properties(
    unit_api_test_api_safety_fixes
    PROPERTIES TIMEOUT 600
    RESOURCE_LOCK "brain_heavy"
  )
endif()

# Tests using NIMCP_BRAIN_SMALL need even longer timeouts (1K neurons vs 100)
if(TARGET unit_api_test_api_global_workspace)
  set_tests_properties(
    unit_api_test_api_global_workspace
    unit_api_test_api_resizing
    PROPERTIES TIMEOUT 600
  )
endif()

# IO stream tests: 60 tests with heavyweight brain init/teardown per test
# Each test takes 1-5 seconds, total runtime ~300 seconds
if(TARGET unit_io_test_stream)
  set_tests_properties(
    unit_io_test_stream
    PROPERTIES TIMEOUT 600
  )
endif()

# Replication tests: heavyweight brain + cluster init/teardown
if(TARGET unit_networking_replication_test_replication)
  set_tests_properties(
    unit_networking_replication_test_replication
    PROPERTIES TIMEOUT 300
  )
endif()

# Cognitive analysis tests: heavyweight brain initialization
if(TARGET unit_cognitive_analysis_test_network_analysis)
  set_tests_properties(
    unit_cognitive_analysis_test_network_analysis
    PROPERTIES TIMEOUT 180
  )
endif()

if(TARGET unit_cognitive_analysis_test_network_analyzer_integration)
  set_tests_properties(
    unit_cognitive_analysis_test_network_analyzer_integration
    PROPERTIES TIMEOUT 180
  )
endif()

# ============================================================================
# Auto-Discovered Regression Test Overrides (TIMEOUT + RESOURCE_LOCK)
# ============================================================================
# Tests auto-discovered by discover_category_tests() need explicit TIMEOUT
# and RESOURCE_LOCK to prevent OOM/timeout in parallel ctest runs.
# Each brain_create allocates 10-60GB globally; parallel brain tests OOM-kill.

if(TARGET regression_core_test_wm_transfer_backward_compat)
  set_tests_properties(regression_core_test_wm_transfer_backward_compat
    PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy")
endif()
if(TARGET regression_cognitive_consolidation_test_systems_consolidation_backward_compat)
  set_tests_properties(regression_cognitive_consolidation_test_systems_consolidation_backward_compat
    PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy")
endif()
if(TARGET regression_io_test_stream_regression)
  set_tests_properties(regression_io_test_stream_regression
    PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy")
endif()
if(TARGET regression_information_test_shannon_backward_compat)
  set_tests_properties(regression_information_test_shannon_backward_compat
    PROPERTIES TIMEOUT 120 RESOURCE_LOCK "brain_heavy")
endif()
if(TARGET regression_api_test_api_memory_safety)
  set_tests_properties(regression_api_test_api_memory_safety
    PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy")
endif()
if(TARGET regression_core_test_core_immune_regression)
  set_tests_properties(regression_core_test_core_immune_regression
    PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy")
endif()
if(TARGET regression_swarm_test_swarm_scalability_regression)
  set_tests_properties(regression_swarm_test_swarm_scalability_regression
    PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy")
endif()
if(TARGET regression_cognitive_introspection_ensemble_uncertainty)
  set_tests_properties(regression_cognitive_introspection_ensemble_uncertainty
    PROPERTIES TIMEOUT 120 RESOURCE_LOCK "brain_heavy")
endif()
if(TARGET test_mesh_cycle_coordinator_regression)
  set_tests_properties(test_mesh_cycle_coordinator_regression
    PROPERTIES TIMEOUT 120)
endif()
if(TARGET test_ternary_conversion_regression)
  set_tests_properties(test_ternary_conversion_regression
    PROPERTIES TIMEOUT 60)
endif()
if(TARGET regression_core_integration_test_integration_regression)
  set_tests_properties(regression_core_integration_test_integration_regression
    PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy" LABELS "regression;flaky")
endif()
if(TARGET regression_brain_factory)
  set_tests_properties(regression_brain_factory
    PROPERTIES TIMEOUT 600 RESOURCE_LOCK "brain_heavy")
endif()
if(TARGET regression_core_brain_factory_test_factory_regression)
  set_tests_properties(regression_core_brain_factory_test_factory_regression
    PROPERTIES TIMEOUT 600 RESOURCE_LOCK "brain_heavy")
endif()
if(TARGET regression_core_brain_regions_cerebellum_test_cerebellum_regression)
  set_tests_properties(regression_core_brain_regions_cerebellum_test_cerebellum_regression
    PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy")
endif()
if(TARGET regression_core_brain_regions_hypothalamus_test_hypo_fep_bridges_regression)
  set_tests_properties(regression_core_brain_regions_hypothalamus_test_hypo_fep_bridges_regression
    PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy")
endif()
if(TARGET regression_test_kg_module_wiring)
  set_tests_properties(regression_test_kg_module_wiring
    PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy")
endif()
if(TARGET regression_networking_nlp_test_nlp_stability)
  set_tests_properties(regression_networking_nlp_test_nlp_stability
    PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy")
endif()
if(TARGET regression_bridges_test_bridge_memory_leaks)
  set_tests_properties(regression_bridges_test_bridge_memory_leaks
    PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy")
endif()
if(TARGET regression_core_test_oscillation_backward_compat)
  # FLAKY: OscillationDetectorCompatTest - timing-sensitive, pre-existing flaky
  set_tests_properties(regression_core_test_oscillation_backward_compat
    PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy" LABELS "regression;flaky")
endif()
if(TARGET regression_core_brain_oscillation_backward_compat)
  # FLAKY: OscillationDetectorCompatTest - timing-sensitive, pre-existing flaky
  set_tests_properties(regression_core_brain_oscillation_backward_compat
    PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy" LABELS "regression;flaky")
endif()
if(TARGET regression_subthalamic_test)
  set_tests_properties(regression_subthalamic_test
    PROPERTIES TIMEOUT 120 RESOURCE_LOCK "brain_heavy")
endif()
if(TARGET regression_middleware_training_test_security_registration_regression)
  set_tests_properties(regression_middleware_training_test_security_registration_regression
    PROPERTIES TIMEOUT 120 RESOURCE_LOCK "brain_heavy")
endif()
if(TARGET regression_utils_fault_tolerance_test_fault_tolerance_exception_regression)
  set_tests_properties(regression_utils_fault_tolerance_test_fault_tolerance_exception_regression
    PROPERTIES TIMEOUT 120 RESOURCE_LOCK "brain_heavy")
endif()
if(TARGET regression_plasticity_neuromodulators_test_phasic_tonic_backward_compat)
  set_tests_properties(regression_plasticity_neuromodulators_test_phasic_tonic_backward_compat
    PROPERTIES TIMEOUT 120 RESOURCE_LOCK "brain_heavy")
endif()
if(TARGET regression_hemispheric_brain_regression)
  set_tests_properties(regression_hemispheric_brain_regression
    PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy")
endif()
if(TARGET test_swarm_memory_regression)
  set_tests_properties(test_swarm_memory_regression
    PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy")
endif()
if(TARGET regression_brain_regions_test_entorhinal_signal_regression)
  set_tests_properties(regression_brain_regions_test_entorhinal_signal_regression
    PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy")
endif()
if(TARGET integration_plasticity_adaptive_test_adaptive_routing_integration)
  set_tests_properties(integration_plasticity_adaptive_test_adaptive_routing_integration
    PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy")
endif()
if(TARGET regression_exception_performance_regression)
  set_tests_properties(regression_exception_performance_regression
    PROPERTIES TIMEOUT 120 RESOURCE_LOCK "brain_heavy")
endif()
if(TARGET regression_networking_nlp_test_nlp_performance)
  set_tests_properties(regression_networking_nlp_test_nlp_performance
    PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy")
endif()
if(TEST regression_io_test_encryption_regression)
  set_tests_properties(regression_io_test_encryption_regression
    PROPERTIES TIMEOUT 120 RESOURCE_LOCK "brain_heavy")
endif()

# ============================================================================
# Test Execution Instructions
# ============================================================================

message(STATUS "")
message(STATUS "Test Execution:")
message(STATUS "  All tests:       ctest -j\$(nproc)")
message(STATUS "  Unit only:       ctest -L unit -j\$(nproc)")
message(STATUS "  Integration:     ctest -L integration -j\$(nproc)")
message(STATUS "  E2E only:        ctest -L e2e")
message(STATUS "  Regression:      ctest -L regression")
message(STATUS "  Verbose:         ctest -V")
message(STATUS "")
message(STATUS "Code Surgeon Integration:")
message(STATUS "  ./tools/code_surgeon/code_surgeon.py --mode test-only")
message(STATUS "  ./tools/code_surgeon/code_surgeon.py --mode full")
message(STATUS "===============================================================================")
message(STATUS "")

# ============================================================================
# Middleware Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/middleware")
    message(STATUS "Adding middleware unit tests...")
    add_subdirectory(unit/middleware)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/middleware")
    message(STATUS "Adding middleware integration tests...")
    add_subdirectory(integration/middleware)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/middleware")
    message(STATUS "Adding middleware regression tests...")
    add_subdirectory(regression/middleware)
endif()

# ============================================================================
# Neural Logic Tests (Core)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/logic")
    message(STATUS "Adding neural logic unit tests...")
    add_subdirectory(unit/core/logic)
endif()

# ============================================================================
# Core Directives Tests (Asimov's Laws, Golden Rule, Combinatorial Harm)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/directives")
    message(STATUS "Adding core directives unit tests...")
    add_subdirectory(unit/core/directives)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/core/directives")
    message(STATUS "Adding core directives integration tests...")
    add_subdirectory(integration/core/directives)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/core/directives")
    message(STATUS "Adding core directives regression tests...")
    add_subdirectory(regression/core/directives)
endif()

# ============================================================================
# Medulla Oblongata Tests (Phase 1.1)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/medulla")
    message(STATUS "Adding medulla unit tests...")
    add_subdirectory(unit/core/medulla)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/core/medulla")
    message(STATUS "Adding medulla integration tests...")
    add_subdirectory(integration/core/medulla)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/core/medulla")
    message(STATUS "Adding medulla regression tests...")
    add_subdirectory(regression/core/medulla)
endif()

# ============================================================================
# Cerebellum Core Module Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/cerebellum")
    message(STATUS "Adding cerebellum core unit tests...")
    add_subdirectory(unit/core/cerebellum)
endif()

# ============================================================================
# Cognitive Fault Tolerance Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/cognitive/fault_tolerance")
    message(STATUS "Adding cognitive fault tolerance unit tests...")
    add_subdirectory(unit/cognitive/fault_tolerance)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/cognitive/fault_tolerance")
    message(STATUS "Adding cognitive fault tolerance integration tests...")
    add_subdirectory(integration/cognitive/fault_tolerance)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/cognitive/fault_tolerance")
    message(STATUS "Adding cognitive fault tolerance regression tests...")
    add_subdirectory(regression/cognitive/fault_tolerance)
endif()

# Root-level Fault Tolerance Integration Tests (self-repair pipeline)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/fault_tolerance")
    message(STATUS "Adding fault tolerance integration tests (self-repair)...")
    add_subdirectory(integration/fault_tolerance)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/cognitive/reasoning")
    message(STATUS "Adding symbolic logic brain integration unit tests...")
    add_subdirectory(unit/cognitive/reasoning)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/cognitive/reasoning")
    message(STATUS "Adding reasoning integration tests...")
    add_subdirectory(integration/cognitive/reasoning)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/cognitive/reasoning")
    message(STATUS "Adding reasoning regression tests...")
    add_subdirectory(regression/cognitive/reasoning)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/cognitive/logic")
    message(STATUS "Adding cognitive logic unit tests...")
    add_subdirectory(unit/cognitive/logic)
endif()

# ============================================================================
# Cognitive Introspection Tests (Phase 1.5.4)
# ============================================================================
# NOTE: Introspection tests are now included via unit/cognitive, integration/cognitive,
# and regression/cognitive subdirectory CMakeLists.txt files to avoid duplicates.

# ============================================================================
# Cognitive Bio-Async Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/cognitive")
    message(STATUS "Adding cognitive unit tests...")
    add_subdirectory(unit/cognitive)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/cognitive/bio_async")
    message(STATUS "Adding cognitive bio-async integration tests...")
    add_subdirectory(integration/cognitive/bio_async)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/cognitive/bio_async")
    message(STATUS "Adding cognitive bio-async regression tests...")
    add_subdirectory(regression/cognitive/bio_async)
endif()

# NOTE: Mirror neurons tests are now included via unit/cognitive, integration/cognitive,
# and regression/cognitive subdirectory CMakeLists.txt files to avoid duplicates.

# ============================================================================
# Cognitive Immune System Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/cognitive/immune")
    message(STATUS "Adding cognitive immune system unit tests...")
    add_subdirectory(unit/cognitive/immune)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/cognitive/immune")
    message(STATUS "Adding cognitive immune system integration tests...")
    add_subdirectory(integration/cognitive/immune)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/cognitive/immune")
    message(STATUS "Adding cognitive immune system regression tests...")
    add_subdirectory(regression/cognitive/immune)
endif()

# ============================================================================
# Enhanced Wellbeing Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/cognitive/wellbeing")
    message(STATUS "Adding enhanced wellbeing integration tests...")
    add_subdirectory(integration/cognitive/wellbeing)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/cognitive/wellbeing")
    message(STATUS "Adding enhanced wellbeing regression tests...")
    add_subdirectory(regression/cognitive/wellbeing)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/cognitive/free_energy")
    message(STATUS "Adding FEP bridge regression tests...")
    add_subdirectory(regression/cognitive/free_energy)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/cognitive/curiosity")
    message(STATUS "Adding Enhanced Curiosity regression tests...")
    add_subdirectory(regression/cognitive/curiosity)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/cognitive/jepa")
    message(STATUS "Adding JEPA regression tests...")
    add_subdirectory(regression/cognitive/jepa)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/cognitive/game_theory")
    message(STATUS "Adding Game Theory regression tests...")
    add_subdirectory(regression/cognitive/game_theory)
endif()

# ============================================================================
# Networking Protocol Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/networking/protocol")
    message(STATUS "Adding networking protocol unit tests...")
    add_subdirectory(unit/networking/protocol)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/networking/protocol")
    message(STATUS "Adding networking protocol integration tests...")
    add_subdirectory(integration/networking/protocol)
endif()

# ============================================================================
# Neural Link Protocol Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/networking/nlp")
    message(STATUS "Adding Neural Link Protocol unit tests...")
    add_subdirectory(unit/networking/nlp)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/networking/nlp")
    message(STATUS "Adding Neural Link Protocol integration tests...")
    add_subdirectory(integration/networking/nlp)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/networking/nlp")
    message(STATUS "Adding Neural Link Protocol regression tests...")
    add_subdirectory(regression/networking/nlp)
endif()

# ============================================================================
# Networking Distributed Tests (Async API)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/networking/distributed")
    message(STATUS "Adding networking distributed unit tests...")
    add_subdirectory(unit/networking/distributed)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/networking/distributed")
    message(STATUS "Adding networking distributed integration tests...")
    add_subdirectory(integration/networking/distributed)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/networking/distributed")
    message(STATUS "Adding networking distributed regression tests...")
    add_subdirectory(regression/networking/distributed)
endif()

# ============================================================================
# Networking Immune Bridge Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/networking/immune")
    message(STATUS "Adding networking immune bridge unit tests...")
    add_subdirectory(unit/networking/immune)
endif()

# ============================================================================
# Networking Exception Handling Tests
# ============================================================================
# WHAT: Tests for exception handling in networking modules
# WHY:  Verify network errors properly map to exceptions and trigger immune responses
# HOW:  Unit and integration tests for P2P, protocol, distributed, NLP exceptions

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/networking/exception")
    message(STATUS "Adding networking exception handling unit tests...")
    add_subdirectory(unit/networking/exception)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/networking/exception")
    message(STATUS "Adding networking exception handling integration tests...")
    add_subdirectory(integration/networking/exception)
endif()

# ============================================================================
# NLP Immune Bridge Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/nlp/immune")
    message(STATUS "Adding NLP immune bridge unit tests...")
    add_subdirectory(unit/nlp/immune)
endif()

# ============================================================================
# Brain Factory Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/brain/factory")
    message(STATUS "Adding brain factory unit tests...")
    add_subdirectory(unit/core/brain/factory)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/brain/inference")
    message(STATUS "Adding brain inference unit tests...")
    add_subdirectory(unit/core/brain/inference)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/core/brain/factory")
    message(STATUS "Adding brain factory integration tests...")
    add_subdirectory(integration/core/brain/factory)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/core/brain/factory")
    message(STATUS "Adding brain factory regression tests...")
    add_subdirectory(regression/core/brain/factory)
endif()

# ============================================================================
# Core Brain Bio-Async Integration Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/core/brain")
    message(STATUS "Adding core brain bio-async integration tests...")
    add_subdirectory(integration/core/brain)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/core/brain")
    message(STATUS "Adding core brain regression tests...")
    add_subdirectory(regression/core/brain)
endif()

# ============================================================================
# Brain Regions Regression Tests
# ============================================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/brain_regions/CMakeLists.txt")
    message(STATUS "Adding brain regions regression tests...")
    add_subdirectory(regression/brain_regions)
endif()

# ============================================================================
# Brain Subsystems Regression Tests (7 new subsystems)
# ============================================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/brain_subsystems/CMakeLists.txt")
    message(STATUS "Adding brain subsystems regression tests...")
    add_subdirectory(regression/brain_subsystems)
endif()

# ============================================================================
# Brain KG Module Wiring Regression Tests
# ============================================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/brain/CMakeLists.txt")
    message(STATUS "Adding Brain KG module wiring regression tests...")
    add_subdirectory(regression/brain)
endif()

# ============================================================================
# Bridges Regression Tests
# ============================================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/bridges/CMakeLists.txt")
    message(STATUS "Adding bridges regression tests...")
    add_subdirectory(regression/bridges)
endif()

# ============================================================================
# Core Neuron Types Bio-Async Integration Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/core/neuron_types")
    message(STATUS "Adding neuron types bio-async integration tests...")
    add_subdirectory(integration/core/neuron_types)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/api")
    message(STATUS "Adding API regression tests (complex opt-in)...")
    add_subdirectory(regression/api)
endif()

# ============================================================================
# Brain Oscillations Tests (Phase C2.2: Complex Oscillation Support)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/brain/oscillations")
    message(STATUS "Adding brain oscillations unit tests (complex phasors)...")
    add_subdirectory(unit/core/brain/oscillations)
endif()

# ============================================================================
# Oscillations Sleep Bridge Tests (Phase 1.1: Sleep Integration)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/brain_oscillations/sleep")
    message(STATUS "Adding oscillations sleep bridge tests...")
    add_executable(unit_core_oscillations_sleep_bridge
        unit/core/brain_oscillations/sleep/test_oscillations_sleep_bridge.cpp
    )
    target_link_libraries(unit_core_oscillations_sleep_bridge
        PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
    )
    target_compile_features(unit_core_oscillations_sleep_bridge PRIVATE cxx_std_17)
    gtest_discover_tests(unit_core_oscillations_sleep_bridge
        PROPERTIES LABELS "unit;core;oscillations;sleep")
endif()

# ============================================================================
# Brain KG Message Index Tests (Phase 6: KG Query Optimization)
# ============================================================================
# WHAT: Unit tests for message-type index in brain knowledge graph
# WHY:  Verify efficient O(1) handler lookup by message type
# HOW:  Test CRUD operations on message-type index
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/brain/test_brain_kg_message_index.cpp")
    message(STATUS "Adding brain KG message index tests...")
    add_executable(unit_core_brain_kg_message_index
        unit/core/brain/test_brain_kg_message_index.cpp
    )
    target_link_libraries(unit_core_brain_kg_message_index
        PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
    )
    target_compile_features(unit_core_brain_kg_message_index PRIVATE cxx_std_17)
    gtest_discover_tests(unit_core_brain_kg_message_index
        PROPERTIES LABELS "unit;core;brain_kg;phase6")
endif()

# ============================================================================
# PR Memory Quantum Bridge Tests (Phase 5: Utils/Quantum Integration)
# ============================================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/brain/internal/test_pr_memory_quantum_bridge.cpp")
    message(STATUS "Adding PR memory quantum bridge tests...")
    add_executable(unit_core_pr_memory_quantum_bridge
        unit/core/brain/internal/test_pr_memory_quantum_bridge.cpp
    )
    target_link_libraries(unit_core_pr_memory_quantum_bridge
        PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
    )
    target_compile_features(unit_core_pr_memory_quantum_bridge PRIVATE cxx_std_17)
    gtest_discover_tests(unit_core_pr_memory_quantum_bridge
        PROPERTIES LABELS "unit;core;brain;internal;pr_memory;quantum")
endif()

# ============================================================================
# Sleep Bridge Integration Tests (Phase 1.1: Sleep Integration)
# ============================================================================
# WHAT: Test all sleep bridges coordinating together
# WHY:  Verify plasticity and cognitive systems integrate correctly during sleep
# HOW:  Simulate sleep cycles with all bridges active

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/plasticity/sleep")
    message(STATUS "Adding plasticity sleep bridge integration tests...")
    add_executable(integration_plasticity_sleep_bridge
        integration/plasticity/sleep/test_plasticity_sleep_integration.cpp
    )
    target_link_libraries(integration_plasticity_sleep_bridge
        PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
    )
    target_compile_features(integration_plasticity_sleep_bridge PRIVATE cxx_std_17)
    gtest_discover_tests(integration_plasticity_sleep_bridge
        PROPERTIES LABELS "integration;plasticity;sleep")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/cognitive/sleep")
    message(STATUS "Adding cognitive sleep bridge integration tests...")
    add_executable(integration_cognitive_sleep_bridge
        integration/cognitive/sleep/test_cognitive_sleep_integration.cpp
    )
    target_link_libraries(integration_cognitive_sleep_bridge
        PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
    )
    target_compile_features(integration_cognitive_sleep_bridge PRIVATE cxx_std_17)
    gtest_discover_tests(integration_cognitive_sleep_bridge
        PROPERTIES LABELS "integration;cognitive;sleep")
endif()

# ============================================================================
# Sleep Bridge Regression Tests (Phase 1.1: Sleep Integration)
# ============================================================================
# WHAT: Exact value regression tests for sleep bridges
# WHY:  Prevent accidental changes to biologically-tuned parameters
# HOW:  Test exact values, boundary conditions, and monotonicity

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/plasticity/sleep")
    message(STATUS "Adding plasticity sleep bridge regression tests...")
    add_executable(regression_plasticity_sleep_bridge
        regression/plasticity/sleep/test_plasticity_sleep_regression.cpp
    )
    target_link_libraries(regression_plasticity_sleep_bridge
        PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
    )
    target_compile_features(regression_plasticity_sleep_bridge PRIVATE cxx_std_17)
    gtest_discover_tests(regression_plasticity_sleep_bridge
        PROPERTIES LABELS "regression;plasticity;sleep")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/cognitive/sleep")
    message(STATUS "Adding cognitive sleep bridge regression tests...")
    add_executable(regression_cognitive_sleep_bridge
        regression/cognitive/sleep/test_cognitive_sleep_regression.cpp
    )
    target_link_libraries(regression_cognitive_sleep_bridge
        PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
    )
    target_compile_features(regression_cognitive_sleep_bridge PRIVATE cxx_std_17)
    gtest_discover_tests(regression_cognitive_sleep_bridge
        PROPERTIES LABELS "regression;cognitive;sleep")
endif()

# ============================================================================
# Sleep Bridge E2E Pipeline Tests (Phase 1.1: Sleep Integration)
# ============================================================================
# WHAT: End-to-end tests for complete sleep bridge pipeline
# WHY:  Verify all 8 sleep bridges work together through realistic sleep cycles
# HOW:  Simulate full night sleep with all bridges active

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/e2e/e2e_test_sleep_bridge_pipeline.cpp")
    message(STATUS "Adding sleep bridge e2e pipeline tests...")
    add_executable(e2e_test_sleep_bridge_pipeline
        e2e/e2e_test_sleep_bridge_pipeline.cpp
    )
    target_link_libraries(e2e_test_sleep_bridge_pipeline
        PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
    )
    target_compile_features(e2e_test_sleep_bridge_pipeline PRIVATE cxx_std_17)
    gtest_discover_tests(e2e_test_sleep_bridge_pipeline
        PROPERTIES LABELS "e2e;sleep;pipeline")
endif()

# ============================================================================
# Brain Regions Tests (Broca's Area + Immune Integration)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/brain/regions")
    message(STATUS "Adding brain regions unit tests (Broca immune integration)...")
    add_subdirectory(unit/core/brain/regions)
endif()

# ============================================================================
# Hemispheric Brain Tests (Bilateral Brain Architecture)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/brain/hemispheric")
    message(STATUS "Adding hemispheric brain unit tests...")
    add_subdirectory(unit/core/brain/hemispheric)
endif()

# ============================================================================
# Subcortical Brain Tests (Basal Ganglia, Thalamus, etc.)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/brain/subcortical")
    message(STATUS "Adding subcortical brain unit tests...")
    add_subdirectory(unit/core/brain/subcortical)
endif()

# ============================================================================
# KG Persistence Tests (Disaster Recovery, Analytics, Schema, Temporal)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/brain/kg")
    message(STATUS "Adding KG persistence unit tests...")
    add_subdirectory(unit/core/brain/kg)
endif()

# ============================================================================
# Brain Bridges Tests (Hyperledger-inspired EOV, Consensus, Audit)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/brain/bridges")
    message(STATUS "Adding brain bridges unit tests (Hyperledger)...")
    add_subdirectory(unit/core/brain/bridges)
endif()

# ============================================================================
# Utils Math Tests (Phase 0.5: Complex Number Support)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/utils/math")
    message(STATUS "Adding utils math unit tests (complex numbers)...")
    add_subdirectory(unit/utils/math)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/utils/math")
    message(STATUS "Adding utils math integration tests (complex oscillations)...")
    add_subdirectory(integration/utils/math)
endif()

# Utils Statistics Tests (Statistical and probability methods)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/utils/statistics")
    message(STATUS "Adding utils statistics unit tests...")
    add_subdirectory(unit/utils/statistics)
endif()

# Utils Signal Processing Tests (Phase 0.5: Band-pass filtering for PAC)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/utils/signal")
    message(STATUS "Adding utils signal processing unit tests...")
    add_subdirectory(unit/utils/signal)
endif()

# Utils Fault Tolerance Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/utils/fault_tolerance")
    message(STATUS "Adding utils fault tolerance unit tests...")
    add_subdirectory(unit/utils/fault_tolerance)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/utils/fault_tolerance")
    message(STATUS "Adding utils fault tolerance integration tests...")
    add_subdirectory(integration/utils/fault_tolerance)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/utils/fault_tolerance")
    message(STATUS "Adding utils fault tolerance regression tests...")
    add_subdirectory(regression/utils/fault_tolerance)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/fault_tolerance/CMakeLists.txt")
    message(STATUS "Adding fault tolerance stability regression tests...")
    add_subdirectory(regression/fault_tolerance)
endif()

# Utils VCS Integration Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/utils/vcs")
    message(STATUS "Adding utils VCS integration unit tests...")
    add_subdirectory(unit/utils/vcs)
endif()

# Utils Exception Handling Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/utils/exception")
    message(STATUS "Adding utils exception handling unit tests...")
    add_subdirectory(unit/utils/exception)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/utils/exception")
    message(STATUS "Adding utils exception handling integration tests...")
    add_subdirectory(integration/utils/exception)
endif()

# Exception Macro Integration Tests
# ============================================================================
# WHAT: Tests for exception macro behavior across modules
# WHY:  Verify NIMCP_CHECK_THROW, security exceptions, and swarm exceptions
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/exception")
    message(STATUS "Adding exception macro integration unit tests...")
    add_subdirectory(unit/exception)
endif()

# Fault Tolerance Unit Tests (Phase 8)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/fault_tolerance/CMakeLists.txt")
    message(STATUS "Adding fault tolerance unit tests...")
    add_subdirectory(unit/fault_tolerance)
endif()

# Brain Unit Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/brain/CMakeLists.txt")
    message(STATUS "Adding brain unit tests...")
    add_subdirectory(unit/brain)
endif()

# Utils Memory Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/utils/memory")
    message(STATUS "Adding utils memory unit tests...")
    add_subdirectory(unit/utils/memory)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/utils/memory")
    message(STATUS "Adding utils memory integration tests...")
    add_subdirectory(integration/utils/memory)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/utils/memory")
    message(STATUS "Adding utils memory regression tests...")
    add_subdirectory(regression/utils/memory)
endif()

# Constants Validation Tests (P0-P2 fixes: magic value constants validation)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/constants/CMakeLists.txt")
    message(STATUS "Adding constants validation unit tests...")
    add_subdirectory(unit/constants)
endif()

# Utils Thread Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/utils/thread")
    message(STATUS "Adding utils thread regression tests...")
    add_subdirectory(regression/utils/thread)
endif()

# Utils Containers Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/utils/containers")
    message(STATUS "Adding utils containers unit tests...")
    add_subdirectory(unit/utils/containers)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/utils/containers")
    message(STATUS "Adding utils containers integration tests...")
    add_subdirectory(integration/utils/containers)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/utils/containers")
    message(STATUS "Adding utils containers regression tests...")
    add_subdirectory(regression/utils/containers)
endif()

# Utils Tensor Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/utils/tensor")
    message(STATUS "Adding utils tensor unit tests...")
    add_subdirectory(unit/utils/tensor)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/utils/tensor")
    message(STATUS "Adding utils tensor integration tests...")
    add_subdirectory(integration/utils/tensor)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/utils/tensor")
    message(STATUS "Adding utils tensor regression tests...")
    add_subdirectory(regression/utils/tensor)
endif()

# Utils Encoding Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/utils/encoding")
    message(STATUS "Adding utils encoding unit tests...")
    add_subdirectory(unit/utils/encoding)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/utils/encoding")
    message(STATUS "Adding utils encoding integration tests (PE + Attention, WM)...")
    add_subdirectory(integration/utils/encoding)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/utils/encoding")
    message(STATUS "Adding utils encoding regression tests...")
    add_subdirectory(regression/utils/encoding)
endif()

# ============================================================================
# Utils RNG Integration Tests
# ============================================================================
# WHAT: Integration tests for nimcp_rand module across system modules
# WHY:  Verify unified RNG works correctly with UMM, thread pool, tensor, training
# HOW:  Test cross-module RNG usage, thread-local storage, and reproducibility

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/rng")
    message(STATUS "Adding utils RNG integration tests...")
    add_subdirectory(integration/rng)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/utils/rng")
    message(STATUS "Adding utils RNG regression tests (thread safety)...")
    add_subdirectory(regression/utils/rng)
endif()

# ============================================================================
# Utils Exception Regression Tests
# ============================================================================
# WHAT: Regression tests for exception handling stability
# WHY:  Ensure exception handling doesn't regress in memory safety, thread safety,
#       callback registration, metrics accuracy, circuit breaker, and epitope generation
# HOW:  Tests create/destroy cycles, null safety, concurrent operations, callbacks

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/utils/exception")
    message(STATUS "Adding utils exception regression tests (stability)...")
    add_subdirectory(regression/utils/exception)
endif()

# ============================================================================
# Utils Gabor Filter Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/utils/gabor")
    message(STATUS "Adding utils gabor filter unit tests...")
    add_subdirectory(unit/utils/gabor)
endif()

# ============================================================================
# Utils Ternary Logic Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/utils/ternary")
    message(STATUS "Adding utils ternary logic unit tests...")
    add_subdirectory(unit/utils/ternary)
endif()

# ============================================================================
# Utils Fuzzy Logic Tests
# ============================================================================
# WHAT: Unit tests for fuzzy logic module (types, operators, inference, bridge)
# WHY:  Verify membership functions, operators, inference engines, and bridge
# HOW:  ~185 tests across 4 executables

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/utils/fuzzy")
    message(STATUS "Adding utils fuzzy logic unit tests...")
    add_subdirectory(unit/utils/fuzzy)
endif()

# ============================================================================
# Ternary Unit Tests (Exception Handling)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/ternary")
    message(STATUS "Adding ternary unit tests...")
    add_subdirectory(unit/ternary)
endif()

# ============================================================================
# Distributed Unit Tests (Exception Handling)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/distributed")
    message(STATUS "Adding distributed unit tests...")
    add_subdirectory(unit/distributed)
endif()

# ============================================================================
# Ternary Integration Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/ternary")
    message(STATUS "Adding ternary integration tests...")
    add_subdirectory(integration/ternary)
endif()

# ============================================================================
# Ternary Regression Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/ternary")
    message(STATUS "Adding ternary representation regression tests...")
    add_subdirectory(regression/ternary)
endif()

# ============================================================================
# Cognitive Integration Tests (General)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/cognitive")
    message(STATUS "Adding cognitive integration tests (WM + PE, emotions)...")
    add_subdirectory(integration/cognitive)
endif()

# ============================================================================
# Cognitive Regression Tests (General)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/cognitive/CMakeLists.txt")
    message(STATUS "Adding cognitive regression tests (general)...")
    add_subdirectory(regression/cognitive)
endif()

# ============================================================================
# Reasoning-Generation Regression Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/reasoning_generation/CMakeLists.txt")
    message(STATUS "Adding reasoning-generation regression tests...")
    add_subdirectory(regression/reasoning_generation)
endif()

# ============================================================================
# Brain Regions Integration Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/core/brain_regions")
    message(STATUS "Adding brain regions integration tests (Predictive + PE)...")
    add_subdirectory(integration/core/brain_regions)
endif()

# Brain regions integration tests (direct, not under core)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/brain_regions")
    message(STATUS "Adding brain regions integration tests (Occipital, Signal Propagation)...")
    add_subdirectory(integration/brain_regions)
endif()

# ============================================================================
# Glial Tests (Microglia, Oligodendrocytes)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/glial")
    message(STATUS "Adding glial unit tests...")
    add_subdirectory(unit/glial)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/glial")
    message(STATUS "Adding glial integration tests...")
    add_subdirectory(integration/glial)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/glial")
    message(STATUS "Adding glial performance regression tests...")
    add_subdirectory(regression/glial)
endif()

# ============================================================================
# Axon Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/axon")
    message(STATUS "Adding axon unit tests...")
    add_subdirectory(unit/core/axon)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/core/axon")
    message(STATUS "Adding axon integration tests...")
    add_subdirectory(integration/core/axon)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/core/axon")
    message(STATUS "Adding axon performance regression tests...")
    add_subdirectory(regression/core/axon)
endif()

# ============================================================================
# Surface Geometry Tests (Meng et al. Nature 2026)
# ============================================================================
# WHAT: Tests for surface optimization in physical networks
# WHY:  Verify paper predictions: trifurcations at chi>=0.83, sprouting at rho<0.6
# HOW:  Unit tests for core, manifold, optimization, and bridges

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/geometry")
    message(STATUS "Adding surface geometry unit tests...")
    add_subdirectory(unit/core/geometry)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/core/geometry")
    message(STATUS "Adding surface geometry integration tests...")
    add_subdirectory(integration/core/geometry)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/core/geometry")
    message(STATUS "Adding surface geometry regression tests...")
    add_subdirectory(regression/core/geometry)
endif()

# NOTE: e2e/core/geometry is included via test/e2e/core/CMakeLists.txt
# to avoid duplicate add_subdirectory

# ============================================================================
# Cortical Columns Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/cortical_columns")
    message(STATUS "Adding cortical columns unit tests...")
    add_subdirectory(unit/core/cortical_columns)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/core/cortical_columns")
    message(STATUS "Adding cortical columns integration tests...")
    add_subdirectory(integration/core/cortical_columns)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/core/cortical_columns")
    message(STATUS "Adding cortical columns performance regression tests...")
    add_subdirectory(regression/core/cortical_columns)
endif()

# ============================================================================
# Neuralnet Tests (Homeostasis-Immune Integration)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/neuralnet")
    message(STATUS "Adding neuralnet unit tests...")
    add_subdirectory(unit/core/neuralnet)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/core/neuralnet")
    message(STATUS "Adding neuralnet integration tests...")
    add_subdirectory(integration/core/neuralnet)
endif()

# ============================================================================
# Dendrite Tests (Phase 1.5.7)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/dendrite")
    message(STATUS "Adding dendrite unit tests...")
    add_subdirectory(unit/core/dendrite)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/core/dendrite")
    message(STATUS "Adding dendrite integration tests...")
    add_subdirectory(integration/core/dendrite)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/core/dendrite")
    message(STATUS "Adding dendrite performance regression tests...")
    add_subdirectory(regression/core/dendrite)
endif()

# ============================================================================
# Substrate Bridge Tests (Neural Substrate Integration)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/neural_substrate")
    message(STATUS "Adding neural substrate unit tests...")
    add_subdirectory(unit/core/neural_substrate)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/substrate")
    message(STATUS "Adding shared substrate-effects unit tests...")
    add_subdirectory(unit/core/substrate)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/thalamic")
    message(STATUS "Adding shared thalamic-channel unit tests...")
    add_subdirectory(unit/core/thalamic)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/core/neural_substrate")
    message(STATUS "Adding neural substrate layer regression tests...")
    add_subdirectory(regression/core/neural_substrate)
endif()

# Neuron Types Regression Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/core/neuron_types")
    message(STATUS "Adding neuron types performance regression tests...")
    add_subdirectory(regression/core/neuron_types)
endif()

# Neuralnet Regression Tests (Spike Ring Buffer)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/core/neuralnet")
    message(STATUS "Adding neuralnet regression tests...")
    add_subdirectory(regression/core/neuralnet)
endif()

# ============================================================================
# Security Tests (Phase SC-1: Security Coverage Framework)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/security")
    message(STATUS "Adding security unit tests...")
    add_subdirectory(unit/security)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/security")
    message(STATUS "Adding security integration tests...")
    add_subdirectory(integration/security)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/security")
    message(STATUS "Adding security regression tests...")
    add_subdirectory(regression/security)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/security_utils")
    message(STATUS "Adding security & utils walkthrough P1/P2/P3 regression tests...")
    add_subdirectory(regression/security_utils)
endif()

# ============================================================================
# Language Layer Tests (Phase L: Language Processing)
# ============================================================================
# WHAT: Tests for language layer orchestrator and bridges
# WHY:  Verify language comprehension, production, and integration
# HOW:  Unit/integration/e2e tests for Wernicke, Broca, NLP, bridges

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/language")
    message(STATUS "Adding language layer unit tests...")
    add_subdirectory(unit/language)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/language")
    message(STATUS "Adding language layer integration tests...")
    add_subdirectory(integration/language)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/language")
    message(STATUS "Adding language regression tests...")
    add_subdirectory(regression/language)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/optimization")
    message(STATUS "Adding 40-watt brain optimization unit tests...")
    add_subdirectory(unit/optimization)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/optimization")
    message(STATUS "Adding 40-watt brain optimization integration tests...")
    add_subdirectory(integration/optimization)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/optimization")
    message(STATUS "Adding 40-watt brain optimization regression tests...")
    add_subdirectory(regression/optimization)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/e2e/optimization")
    message(STATUS "Adding 40-watt brain optimization e2e tests...")
    add_subdirectory(e2e/optimization)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/wernicke")
    message(STATUS "Adding Wernicke unit tests...")
    add_subdirectory(unit/core/wernicke)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/wernicke")
    message(STATUS "Adding Wernicke integration tests...")
    add_subdirectory(integration/wernicke)
endif()

# ============================================================================
# Generation Tests (Tokenizer, Embedding, Language Generator)
# ============================================================================
# WHAT: Tests for the text generation pipeline
# WHY:  Verify tokenizer, embedding layer, and language generator correctness
# HOW:  Unit tests for each component, integration tests for the full pipeline

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/generation")
    message(STATUS "Adding generation unit tests...")
    add_subdirectory(unit/generation)
endif()

# ============================================================================
# Utils Logging Tests (Phase LOG-1: Enhanced Logging System)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/utils/logging")
    message(STATUS "Adding utils logging unit tests...")
    add_subdirectory(unit/utils/logging)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/utils/logging")
    message(STATUS "Adding utils logging integration tests...")
    add_subdirectory(integration/utils/logging)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/utils/logging")
    message(STATUS "Adding utils logging regression tests...")
    add_subdirectory(regression/utils/logging)
endif()

# ============================================================================
# Async Tests (Phase BIO-ASYNC: Biologically-Inspired Async)
# ============================================================================
# WHAT: Tests for biological async system (neuromodulators, phase coupling, etc.)
# WHY:  Verify correctness and performance of biologically-plausible async primitives
# HOW:  Unit tests verify components, integration tests verify interactions,
#       regression tests verify performance and numerical stability

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/async")
    message(STATUS "Adding async unit tests...")
    add_subdirectory(unit/async)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/async")
    message(STATUS "Adding async integration tests...")
    add_subdirectory(integration/async)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/async")
    message(STATUS "Adding async regression tests...")
    add_subdirectory(regression/async)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/bio_async")
    message(STATUS "Adding bio-async self-repair regression tests...")
    add_subdirectory(regression/bio_async)
endif()

# ============================================================================
# Plasticity Tests
# ============================================================================
# WHAT: Tests for plasticity modules (STDP, BCM, homeostatic, dendritic, etc.)
# WHY:  Verify synaptic plasticity mechanisms and coordinator
# HOW:  Unit tests for mechanisms, coordinator tests for conflict resolution

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/plasticity")
    message(STATUS "Adding plasticity unit tests...")
    add_subdirectory(unit/plasticity)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/plasticity")
    message(STATUS "Adding plasticity integration tests...")
    add_subdirectory(integration/plasticity)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/plasticity")
    message(STATUS "Adding plasticity regression tests...")
    add_subdirectory(regression/plasticity)
endif()

# ============================================================================
# Perception Tests
# ============================================================================
# WHAT: Tests for perception modules (visual, audio, speech cortex)
# WHY:  Verify sensory processing and cortical integration
# HOW:  Unit tests for cortical processing, integration tests for multimodal

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/perception")
    message(STATUS "Adding perception unit tests...")
    add_subdirectory(unit/perception)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/perception")
    message(STATUS "Adding perception integration tests...")
    add_subdirectory(integration/perception)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/perception")
    message(STATUS "Adding perception regression tests...")
    add_subdirectory(regression/perception)
endif()

# ============================================================================
# JEPA Integration Tests (Joint Embedding Predictive Architecture)
# ============================================================================
# WHAT: Tests for JEPA modules (latent, predictor, multimodal, context)
# WHY:  Verify JEPA system integration with FEP, perception bridges, and context
# HOW:  Integration tests for cross-modal prediction, FEP coordination, context encoding

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/jepa")
    message(STATUS "Adding JEPA integration tests...")
    add_subdirectory(integration/jepa)
endif()

# ============================================================================
# Swarm Intelligence Tests (Phase SWARM-1: Drone Collective Intelligence)
# ============================================================================
# WHAT: Tests for swarm brain coordination system
# WHY:  Verify distributed cognition, consensus, and emergence detection
# HOW:  Unit tests for components, integration tests for multi-drone coordination

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/swarm")
    message(STATUS "Adding swarm unit tests...")
    add_subdirectory(unit/swarm)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/swarm")
    message(STATUS "Adding swarm integration tests...")
    add_subdirectory(integration/swarm)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/swarm")
    message(STATUS "Adding swarm regression tests...")
    add_subdirectory(regression/swarm)
endif()

# ============================================================================
# Mesh Network Tests (Hyperledger-inspired Architecture)
# ============================================================================
# WHAT: Tests for unified mesh network with channels, endorsement, and ordering
# WHY:  Verify distributed consensus, transaction lifecycle, and coordinator pools
# HOW:  Unit tests for types/participant/transaction, integration for full flow

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/mesh")
    message(STATUS "Adding mesh network unit tests...")
    add_subdirectory(unit/mesh)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/mesh")
    message(STATUS "Adding mesh network integration tests...")
    add_subdirectory(integration/mesh)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/knowledge")
    message(STATUS "Adding knowledge graph integration tests...")
    add_subdirectory(integration/knowledge)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/mesh")
    message(STATUS "Adding mesh network regression tests...")
    add_subdirectory(regression/mesh)
endif()

# ============================================================================
# Platform Tier Tests (Portia Spider Foundation)
# ============================================================================
# WHAT: Tests for platform tier detection and configuration
# WHY:  Verify correct resource adaptation across hardware tiers
# HOW:  Unit tests for tier detection, integration tests for brain configuration

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/utils/platform")
    message(STATUS "Adding platform tier unit tests...")
    add_subdirectory(unit/utils/platform)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/utils/platform")
    message(STATUS "Adding platform tier integration tests...")
    add_subdirectory(integration/utils/platform)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/utils/platform")
    message(STATUS "Adding platform tier regression tests...")
    add_subdirectory(regression/utils/platform)
endif()

# ============================================================================
# Sparse Synapse Tests (Memory Optimization)
# ============================================================================
# Already added above in neuralnet section - removed duplicate

# ============================================================================
# Portia Spider Tests (Graceful Degradation, Learning, Sensor Fusion)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/portia")
    message(STATUS "Adding portia spider unit tests...")
    add_subdirectory(unit/portia)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/portia")
    message(STATUS "Adding portia spider integration tests...")
    add_subdirectory(integration/portia)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/portia")
    message(STATUS "Adding portia spider regression tests...")
    add_subdirectory(regression/portia)
endif()

# ============================================================================
# GPU Unit Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/gpu/CMakeLists.txt")
    message(STATUS "Adding GPU unit tests...")
    add_subdirectory(unit/gpu)
endif()

# ============================================================================
# GPU Integration Tests
# ============================================================================
# WHAT: GPU/CPU fallback integration tests
# WHY:  Verify execution mode detection, switching, and graceful fallback
# HOW:  Test full GPU neural network and execution context lifecycle

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/test_gpu_cpu_fallback_integration.cpp")
    message(STATUS "Adding GPU integration tests...")
    add_subdirectory(integration)
endif()

# ============================================================================
# GPU Integration Tests (Equivalence, Backend Switching, Precision)
# ============================================================================
# WHAT: GPU/CPU equivalence, backend switching, mixed precision tests
# WHY:  Verify GPU operations match CPU results and backend system works correctly
# HOW:  Test kernel equivalence, backend lifecycle, and precision conversions

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/gpu/CMakeLists.txt")
    message(STATUS "Adding GPU kernel integration tests...")
    add_subdirectory(integration/gpu)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/gpu/CMakeLists.txt")
    message(STATUS "Adding GPU regression tests...")
    add_subdirectory(regression/gpu)
endif()

# ============================================================================
# LNN Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/lnn/CMakeLists.txt")
    message(STATUS "Adding LNN unit tests...")
    add_subdirectory(unit/lnn)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/lnn/CMakeLists.txt")
    message(STATUS "Adding LNN integration tests...")
    add_subdirectory(integration/lnn)
endif()

# ============================================================================
# SNN Tests (Spiking Neural Network)
# ============================================================================
# WHAT: Tests for SNN module integrating with existing neural infrastructure
# WHY:  SNN is the core neural substrate providing spike-based computation
# HOW:  Tests config, network, encoding, training, and bio-async integration

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/snn/CMakeLists.txt")
    message(STATUS "Adding SNN unit tests...")
    add_subdirectory(unit/snn)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/snn/CMakeLists.txt")
    message(STATUS "Adding SNN integration tests...")
    add_subdirectory(integration/snn)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/snn/CMakeLists.txt")
    message(STATUS "Adding SNN regression tests...")
    add_subdirectory(regression/snn)
endif()

# ============================================================================
# Performance Benchmarks
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks")
    message(STATUS "Adding performance benchmarks...")
    add_subdirectory(benchmarks)
endif()

# ============================================================================
# Utils Quantum Tests (Quantum Walk for Graph Algorithms)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/utils/quantum")
    message(STATUS "Adding utils quantum unit tests...")
    add_subdirectory(unit/utils/quantum)
endif()

# ============================================================================
# Utils RNG Tests (Unified Random Number Generation)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/utils/rng")
    message(STATUS "Adding utils RNG unit tests...")
    add_subdirectory(unit/utils/rng)
endif()

# ============================================================================
# Quantum Bridge Integration Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/quantum")
    message(STATUS "Adding quantum bridge unit tests...")
    add_subdirectory(unit/quantum)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/quantum")
    message(STATUS "Adding quantum bridge integration tests...")
    add_subdirectory(integration/quantum)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/quantum")
    message(STATUS "Adding quantum bridge regression tests...")
    add_subdirectory(regression/quantum)
endif()

# ============================================================================
# CNN Training Regression Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/training")
    message(STATUS "Adding CNN training regression tests...")
    add_subdirectory(regression/training)
endif()

# ============================================================================
# CNN Training Unit Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/training")
    message(STATUS "Adding CNN training unit tests...")
    add_subdirectory(unit/training)
endif()

# ============================================================================
# CNN Training Integration Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/training")
    message(STATUS "Adding CNN training integration tests...")
    add_subdirectory(integration/training)
endif()

# ============================================================================
# Dragonfly Interception Module Tests (Bio-Inspired Target Tracking)
# ============================================================================
# WHAT: Tests for dragonfly-inspired predictive interception module
# WHY:  Verify TSDN population vector encoding, target tracking, prediction
# HOW:  Based on dragonfly neuroscience - 95% prey interception success

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/dragonfly")
    message(STATUS "Adding dragonfly unit tests...")
    add_subdirectory(unit/dragonfly)
endif()

# ============================================================================
# Dragonfly Integration Tests
# ============================================================================
# WHAT: Integration tests for dragonfly with brain and other subsystems
# WHY:  Verify cross-module coordination and signal flow
# HOW:  Test bridge coordination, brain integration, sensory fusion

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/dragonfly")
    message(STATUS "Adding dragonfly integration tests...")
    add_subdirectory(integration/dragonfly)
endif()

# ============================================================================
# Dragonfly Regression Tests
# ============================================================================
# WHAT: Regression tests for dragonfly parameter stability and performance
# WHY:  Ensure biological parameters remain stable across versions
# HOW:  Test fixed values, performance benchmarks, numerical stability

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/dragonfly")
    message(STATUS "Adding dragonfly regression tests...")
    add_subdirectory(regression/dragonfly)
endif()

# ============================================================================
# Parietal Lobe Regression Tests
# ============================================================================
# WHAT: Regression tests for parietal lobe FEP integration
# WHY:  Ensure FEP parameters, numerical stability, and performance remain stable
# HOW:  Test fixed values, boundary conditions, and performance baselines

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/parietal")
    message(STATUS "Adding parietal lobe regression tests...")
    add_subdirectory(regression/parietal)
endif()

# ============================================================================
# Chemistry Layer Regression Tests
# ============================================================================
# WHAT: Regression tests for chemistry layer modules (pH, gasotransmitters)
# WHY:  Ensure numerical stability, boundary conditions, and performance baselines
# HOW:  Subdirectory-based test organization

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/chemistry")
    message(STATUS "Adding chemistry layer regression tests...")
    add_subdirectory(regression/chemistry)
endif()

# ============================================================================
# Biology Layer Regression Tests
# ============================================================================
# WHAT: Regression tests for biology layer modules (neurovascular, etc.)
# WHY:  Ensure numerical stability, boundary conditions, and performance baselines
# HOW:  Subdirectory-based test organization

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/biology")
    message(STATUS "Adding biology layer regression tests...")
    add_subdirectory(regression/biology)
endif()

# ============================================================================
# Layer Infrastructure Unit Tests
# ============================================================================
# WHAT: Unit tests for the 9-layer hierarchical integration framework
# WHY:  Verify correct operation of layer coordinators, bridges, and routing
# HOW:  GTest-based tests for lifecycle, messaging, and state management

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/integration")
    message(STATUS "Adding layer infrastructure unit tests...")
    add_subdirectory(unit/integration)
endif()

# ============================================================================
# Chemistry Layer Unit Tests
# ============================================================================
# WHAT: Unit tests for chemistry layer modules (pH dynamics, gasotransmitters)
# WHY:  Verify chemical signaling and regulation mechanisms
# HOW:  Subdirectory-based test organization

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/chemistry")
    message(STATUS "Adding chemistry layer unit tests...")
    add_subdirectory(unit/chemistry)
endif()

# ============================================================================
# Biology Layer Unit Tests
# ============================================================================
# WHAT: Unit tests for biology layer modules (epigenetics, neurogenesis, neurovascular)
# WHY:  Verify biological processes and regulation mechanisms
# HOW:  Subdirectory-based test organization

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/biology")
    message(STATUS "Adding biology layer unit tests...")
    add_subdirectory(unit/biology)
endif()

# ============================================================================
# Chemistry Layer Integration Tests
# ============================================================================
# WHAT: Integration tests for chemistry layer modules (pH, gasotransmitters)
# WHY:  Verify chemical signaling integration with neural systems
# HOW:  Subdirectory-based test organization

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/chemistry")
    message(STATUS "Adding chemistry layer integration tests...")
    add_subdirectory(integration/chemistry)
endif()

# ============================================================================
# Biology Layer Integration Tests
# ============================================================================
# WHAT: Integration tests for biology layer modules (neurovascular, etc.)
# WHY:  Verify biological process integration with neural systems
# HOW:  Subdirectory-based test organization

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/biology")
    message(STATUS "Adding biology layer integration tests...")
    add_subdirectory(integration/biology)
endif()

# ============================================================================
# Embodiment Module Unit Tests
# ============================================================================
# WHAT: Unit tests for embodiment modules (affordance, body ownership, simulation, interoception)
# WHY:  Verify embodied cognition subsystems for physical interaction modeling
# HOW:  Subdirectory-based test organization

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/embodiment")
    message(STATUS "Adding embodiment module unit tests...")
    add_subdirectory(unit/embodiment)
endif()

# ============================================================================
# Superhuman Enhancement Module Unit Tests
# ============================================================================
# WHAT: Unit tests for superhuman enhancement modules
# WHY:  Verify capability limits, safety boundaries, and proper behavior
# HOW:  GTest with comprehensive coverage of all superhuman modules
#
# MODULES TESTED:
# - Eagle Vision: Enhanced visual acuity with bounds enforcement
# - Time Dilation: Temporal processing with factor limits
# - Savant Mode: Pattern recognition abilities with safe limits
# - Echolocation: 3D spatial mapping with range constraints
# - Hyperthymesia: Autobiographical memory with encoding limits
# - Synesthesia: Cross-modal perception with inhibition control
# - Precognition: Pattern prediction with confidence bounds

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/superhuman")
    message(STATUS "Adding superhuman module unit tests...")
    add_subdirectory(unit/superhuman)
endif()

# ============================================================================
# Physics Layer Unit Tests
# ============================================================================
# WHAT: Unit tests for physics layer modules (thermodynamics, biophysics, etc.)
# WHY:  Verify physical process modeling and constraints
# HOW:  Subdirectory-based test organization

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/io")
    message(STATUS "Adding IO unit tests...")
    add_subdirectory(unit/io)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/physics/thermodynamics")
    message(STATUS "Adding physics thermodynamics unit tests...")
    add_subdirectory(unit/physics/thermodynamics)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/physics/biophysics")
    message(STATUS "Adding physics biophysics unit tests...")
    add_subdirectory(unit/physics/biophysics)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/physics/geometry")
    message(STATUS "Adding physics information geometry unit tests...")
    add_subdirectory(unit/physics/geometry)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/physics/ephaptic")
    message(STATUS "Adding physics ephaptic coupling unit tests...")
    add_subdirectory(unit/physics/ephaptic)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/physics/graphs")
    message(STATUS "Adding physics graph theory unit tests...")
    add_subdirectory(unit/physics/graphs)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/physics/dynamics")
    message(STATUS "Adding physics dynamical systems unit tests...")
    add_subdirectory(unit/physics/dynamics)
endif()

# Exception handling tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/physics/exception/CMakeLists.txt")
    message(STATUS "Adding physics exception handling unit tests...")
    add_subdirectory(unit/physics/exception)
endif()

# ============================================================================
# Physics Layer Integration Tests
# ============================================================================
# WHAT: Integration tests for physics layer modules (HH, biophysics, etc.)
# WHY:  Verify physics modules integrate correctly with SNN and other systems
# HOW:  Subdirectory-based test organization

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/physics")
    message(STATUS "Adding physics layer integration tests...")
    add_subdirectory(integration/physics)
endif()

# ============================================================================
# Physics Layer Regression Tests
# ============================================================================
# WHAT: Regression tests for physics layer modules (HH, thermodynamics, ephaptic)
# WHY:  Verify determinism, numerical stability, physical bounds, API compat
# HOW:  Subdirectory-based test organization

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/physics")
    message(STATUS "Adding physics layer regression tests...")
    add_subdirectory(regression/physics)
endif()

# ============================================================================
# Exception Regression Tests
# ============================================================================
# WHAT: Regression tests for exception handling subsystem
# WHY:  Verify signal exception handling, immune integration, API contracts
# HOW:  Subdirectory-based test organization

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/exception")
    message(STATUS "Adding exception regression tests...")
    add_subdirectory(regression/exception)
endif()

# ============================================================================
# World Model Tests (Omnidirectional + Multi-Modal)
# ============================================================================
# WHAT: Tests for world model modules (DreamerV3/JEPA-inspired)
# WHY:  Verify RSSM, MDN, counterfactual reasoning, multi-modal fusion
# HOW:  Unit, integration, regression, and E2E tests

# NOTE: These subdirectories are already included via their parent directories:
# - unit/cognitive/omni is included via unit/cognitive/CMakeLists.txt
# - unit/cognitive/extrapolation is included via unit/cognitive/CMakeLists.txt
# - integration/cognitive/world_model is included via integration/cognitive/CMakeLists.txt
# - regression/cognitive/world_model is included via regression/cognitive/CMakeLists.txt
# Direct includes removed to avoid "binary directory already used" CMake error.

# NOTE: e2e/cognitive is already included via e2e/CMakeLists.txt
# Direct include removed to avoid "binary directory already used" CMake error.

# ============================================================================
# Adversarial Tests (Security Robustness)
# ============================================================================
# WHAT: Adversarial tests for security subsystems
# WHY:  Verify robustness against tampering, bypass attempts, and edge cases
# HOW:  Test safety KB tampering, rule injection, bypass techniques, DoS
#       resilience, and integrity verification under adversarial conditions

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/adversarial/lgss/CMakeLists.txt")
    message(STATUS "Adding LGSS adversarial tests...")
    add_subdirectory(adversarial/lgss)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/adversarial/bbb/CMakeLists.txt")
    message(STATUS "Adding BBB adversarial / PEN tests...")
    add_subdirectory(adversarial/bbb)
endif()

# ============================================================================
# Python Unit Tests
# ============================================================================
# WHAT: Unit tests for Python binding memory safety patterns
# WHY:  Verify correct memory handling in Python C extension code
# HOW:  Test allocation/deallocation patterns, error handling

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/python")
    message(STATUS "Adding Python unit tests...")
    add_subdirectory(unit/python)
endif()

# ============================================================================
# Python-C Integration Tests
# ============================================================================
# WHAT: Integration tests for Python bindings with C library
# WHY:  Verify memory management and error propagation across Python/C boundary
# HOW:  Test Python interpreter embedding and binding functionality

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/python")
    message(STATUS "Adding Python-C integration tests...")
    add_subdirectory(integration/python)
endif()

# ============================================================================
# C++ Bindings Tests
# ============================================================================
# WHAT: GTest suite for idiomatic C++20 bindings (nimcp.hpp)
# WHY:  Verify RAII wrappers, exceptions, callbacks, and full API coverage
# HOW:  Header-only library tested via cpp_bindings_test executable

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bindings/cpp/CMakeLists.txt")
    message(STATUS "Adding C++ bindings tests...")
    add_subdirectory(bindings/cpp)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bindings/java/CMakeLists.txt")
    message(STATUS "Adding Java bindings tests...")
    add_subdirectory(bindings/java)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bindings/csharp/CMakeLists.txt")
    message(STATUS "Adding C# bindings tests...")
    add_subdirectory(bindings/csharp)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bindings/rust/CMakeLists.txt")
    message(STATUS "Adding Rust bindings tests...")
    add_subdirectory(bindings/rust)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bindings/go/CMakeLists.txt")
    message(STATUS "Adding Go bindings tests...")
    add_subdirectory(bindings/go)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bindings/perl/CMakeLists.txt")
    message(STATUS "Adding Perl bindings tests...")
    add_subdirectory(bindings/perl)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bindings/nodejs/CMakeLists.txt")
    message(STATUS "Adding Node.js bindings tests...")
    add_subdirectory(bindings/nodejs)
endif()

# ============================================================================
# Tensor Operations Integration Tests (Standalone)
# ============================================================================
# WHAT: Integration tests for tensor operations, autodiff, and SIMD
# WHY:  Verify tensor operation chains and gradient computation
# HOW:  Test operation chaining, broadcasting, numerical stability

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/tensor")
    message(STATUS "Adding tensor operations integration tests...")
    add_subdirectory(integration/tensor)
endif()

# ============================================================================
# Cross-Module Error Propagation Tests
# ============================================================================
# WHAT: Integration tests for error propagation across module boundaries
# WHY:  Verify cleanup happens properly on errors and error context preserved
# HOW:  Test multi-module error scenarios and recovery

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/error")
    message(STATUS "Adding cross-module error propagation tests...")
    add_subdirectory(integration/error)
endif()

# ============================================================================
# Utils Fuzzy Logic Tests
# ============================================================================
# WHAT: Integration, regression, and E2E tests for fuzzy logic subsystem
# WHY:  Verify fuzzy types, operators, inference, and bridge modules
# HOW:  Cross-module integration, mathematical invariants, full pipelines

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/utils/fuzzy")
    message(STATUS "Adding fuzzy logic integration tests...")
    add_subdirectory(integration/utils/fuzzy)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/utils/fuzzy")
    message(STATUS "Adding fuzzy logic regression tests...")
    add_subdirectory(regression/utils/fuzzy)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/e2e/utils/fuzzy")
    message(STATUS "Adding fuzzy logic E2E tests...")
    add_subdirectory(e2e/utils/fuzzy)
endif()

# ============================================================================
# Statistics Module Tests (Shannon Integration)
# ============================================================================
# WHAT: Unit, integration, regression, and E2E tests for statistics module
# WHY:  Statistics module provides probability distributions, hypothesis testing,
#       and Shannon information theory integration
# HOW:  Tests verify numerical accuracy, Shannon integration, and edge cases

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/statistics")
    message(STATUS "Adding statistics regression tests...")
    add_subdirectory(regression/statistics)
endif()

# ============================================================================
# Genius Profiles Unit Tests
# ============================================================================
# WHAT: Unit tests for genius profiles system and eidetic memory integration
# WHY:  Tests profile APIs, eidetic apply functions, utility functions
# HOW:  GTest-based unit tests for individual components
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/genius/CMakeLists.txt")
    message(STATUS "Adding Genius Profiles unit tests...")
    add_subdirectory(unit/genius)
endif()

# ============================================================================
# Genius Profiles Integration Tests
# ============================================================================
# WHAT: Integration tests for genius profiles cross-system communication
# WHY:  Tests bio-async, memory systems, and eidetic integration
# HOW:  GTest-based integration tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/genius/CMakeLists.txt")
    message(STATUS "Adding Genius Profiles integration tests...")
    add_subdirectory(integration/genius)
endif()

# ============================================================================
# Genius Profiles Regression Tests
# ============================================================================
# WHAT: Regression tests for genius profiles system
# WHY:  Tests mathematical, artistic, musical, scientific, athletic, strategic,
#       financial, literary, and polymath brain configurations
# HOW:  Tests profile consistency, eidetic memory presets, blending, state machines
# Note: E2E tests are added via test/e2e/CMakeLists.txt
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/genius")
    message(STATUS "Adding Genius Profiles regression tests...")
    add_subdirectory(regression/genius)
endif()

# ============================================================================
# Brain Region KG Wiring Tests
# ============================================================================
# WHAT: Unit, integration, regression, and E2E tests for brain region KG wiring
# WHY:  Tests hippocampus, amygdala, and PFC knowledge graph registration
# HOW:  Pure C tests verifying configuration, registration, queries, and state sync
# ============================================================================

message(STATUS "Adding brain region KG wiring tests...")

# Unit tests - Hippocampus KG wiring
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/brain_regions/kg_wiring/test_hippocampus_kg_wiring.c")
    add_executable(unit_brain_regions_kg_wiring_hippocampus
        unit/brain_regions/kg_wiring/test_hippocampus_kg_wiring.c
    )
    target_link_libraries(unit_brain_regions_kg_wiring_hippocampus PRIVATE nimcp pthread m)
    target_include_directories(unit_brain_regions_kg_wiring_hippocampus PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    add_test(NAME unit_brain_regions_kg_wiring_hippocampus
        COMMAND unit_brain_regions_kg_wiring_hippocampus
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(unit_brain_regions_kg_wiring_hippocampus PROPERTIES
        LABELS "unit;brain_regions;kg_wiring;hippocampus"
        TIMEOUT 30
    )
endif()

# Unit tests - Amygdala KG wiring
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/brain_regions/kg_wiring/test_amygdala_kg_wiring.c")
    add_executable(unit_brain_regions_kg_wiring_amygdala
        unit/brain_regions/kg_wiring/test_amygdala_kg_wiring.c
    )
    target_link_libraries(unit_brain_regions_kg_wiring_amygdala PRIVATE nimcp pthread m)
    target_include_directories(unit_brain_regions_kg_wiring_amygdala PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    add_test(NAME unit_brain_regions_kg_wiring_amygdala
        COMMAND unit_brain_regions_kg_wiring_amygdala
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(unit_brain_regions_kg_wiring_amygdala PROPERTIES
        LABELS "unit;brain_regions;kg_wiring;amygdala"
        TIMEOUT 30
    )
endif()

# Unit tests - PFC KG wiring
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/brain_regions/kg_wiring/test_pfc_kg_wiring.c")
    add_executable(unit_brain_regions_kg_wiring_pfc
        unit/brain_regions/kg_wiring/test_pfc_kg_wiring.c
    )
    target_link_libraries(unit_brain_regions_kg_wiring_pfc PRIVATE nimcp pthread m)
    target_include_directories(unit_brain_regions_kg_wiring_pfc PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    add_test(NAME unit_brain_regions_kg_wiring_pfc
        COMMAND unit_brain_regions_kg_wiring_pfc
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(unit_brain_regions_kg_wiring_pfc PROPERTIES
        LABELS "unit;brain_regions;kg_wiring;pfc"
        TIMEOUT 30
    )
endif()

# Integration tests - Cross-module KG wiring
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/brain_regions/test_kg_wiring_integration.c")
    add_executable(integration_brain_regions_kg_wiring
        integration/brain_regions/test_kg_wiring_integration.c
    )
    target_link_libraries(integration_brain_regions_kg_wiring PRIVATE nimcp pthread m)
    target_include_directories(integration_brain_regions_kg_wiring PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    add_test(NAME integration_brain_regions_kg_wiring
        COMMAND integration_brain_regions_kg_wiring
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(integration_brain_regions_kg_wiring PROPERTIES
        LABELS "integration;brain_regions;kg_wiring"
        TIMEOUT 60
    )
endif()

# Regression tests - KG wiring API stability
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/brain_regions/test_kg_wiring_regression.c")
    add_executable(regression_brain_regions_kg_wiring
        regression/brain_regions/test_kg_wiring_regression.c
    )
    target_link_libraries(regression_brain_regions_kg_wiring PRIVATE nimcp pthread m)
    target_include_directories(regression_brain_regions_kg_wiring PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    add_test(NAME regression_brain_regions_kg_wiring
        COMMAND regression_brain_regions_kg_wiring
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(regression_brain_regions_kg_wiring PROPERTIES
        LABELS "regression;brain_regions;kg_wiring"
        TIMEOUT 30
    )
endif()

# E2E tests - Full brain region workflows
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/e2e/brain_regions/test_kg_wiring_e2e.c")
    add_executable(e2e_brain_regions_kg_wiring
        e2e/brain_regions/test_kg_wiring_e2e.c
    )
    target_link_libraries(e2e_brain_regions_kg_wiring PRIVATE nimcp pthread m)
    target_include_directories(e2e_brain_regions_kg_wiring PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    add_test(NAME e2e_brain_regions_kg_wiring
        COMMAND e2e_brain_regions_kg_wiring
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(e2e_brain_regions_kg_wiring PROPERTIES
        LABELS "e2e;brain_regions;kg_wiring"
        TIMEOUT 120
    )
endif()

# ============================================================================
# BBB (Blood-Brain Barrier) Sublayer Unit Tests (Pure C)
# ============================================================================
# WHAT: Unit tests for BBB perimeter security sublayer modules
# WHY:  Verify BBB access control, code signing, input gate, memory boundary, helpers
# HOW:  Pure C tests without GTest dependency for each BBB sublayer module
# ============================================================================

# BBB Access Control Unit Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/security/bbb/test_bbb_access_control.c")
    add_executable(unit_security_bbb_access_control
        unit/security/bbb/test_bbb_access_control.c
    )
    target_link_libraries(unit_security_bbb_access_control PRIVATE nimcp pthread m)
    target_include_directories(unit_security_bbb_access_control PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
    )
    add_test(NAME unit_security_bbb_access_control
        COMMAND unit_security_bbb_access_control
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(unit_security_bbb_access_control PROPERTIES
        LABELS "unit;security;bbb;access_control"
        TIMEOUT 30
    )
endif()

# BBB Code Signing Unit Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/security/bbb/test_bbb_code_signing.c")
    add_executable(unit_security_bbb_code_signing
        unit/security/bbb/test_bbb_code_signing.c
    )
    target_link_libraries(unit_security_bbb_code_signing PRIVATE nimcp pthread m)
    target_include_directories(unit_security_bbb_code_signing PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
    )
    add_test(NAME unit_security_bbb_code_signing
        COMMAND unit_security_bbb_code_signing
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(unit_security_bbb_code_signing PROPERTIES
        LABELS "unit;security;bbb;code_signing"
        TIMEOUT 30
    )
endif()

# BBB Input Gate Unit Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/security/bbb/test_bbb_input_gate.c")
    add_executable(unit_security_bbb_input_gate
        unit/security/bbb/test_bbb_input_gate.c
    )
    target_link_libraries(unit_security_bbb_input_gate PRIVATE nimcp pthread m)
    target_include_directories(unit_security_bbb_input_gate PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
    )
    add_test(NAME unit_security_bbb_input_gate
        COMMAND unit_security_bbb_input_gate
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(unit_security_bbb_input_gate PROPERTIES
        LABELS "unit;security;bbb;input_gate"
        TIMEOUT 30
    )
endif()

# BBB Memory Boundary Unit Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/security/bbb/test_bbb_memory_boundary.c")
    add_executable(unit_security_bbb_memory_boundary
        unit/security/bbb/test_bbb_memory_boundary.c
    )
    target_link_libraries(unit_security_bbb_memory_boundary PRIVATE nimcp pthread m)
    target_include_directories(unit_security_bbb_memory_boundary PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
    )
    add_test(NAME unit_security_bbb_memory_boundary
        COMMAND unit_security_bbb_memory_boundary
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(unit_security_bbb_memory_boundary PROPERTIES
        LABELS "unit;security;bbb;memory_boundary"
        TIMEOUT 30
    )
endif()

# BBB Helpers Unit Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/security/bbb/test_bbb_helpers.c")
    add_executable(unit_security_bbb_helpers
        unit/security/bbb/test_bbb_helpers.c
    )
    target_link_libraries(unit_security_bbb_helpers PRIVATE nimcp pthread m)
    target_include_directories(unit_security_bbb_helpers PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
    )
    add_test(NAME unit_security_bbb_helpers
        COMMAND unit_security_bbb_helpers
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(unit_security_bbb_helpers PROPERTIES
        LABELS "unit;security;bbb;helpers"
        TIMEOUT 30
    )
endif()

# ============================================================================
# Extended Timeout Configuration (Applied AFTER all tests are registered)
# ============================================================================
# WHAT: Extended timeouts for heavyweight brain tests
# WHY:  These tests create full brain instances with all subsystems
#       Each brain creation initializes 50+ subsystems and comprehensive tests
#       run multiple brain instances with full validation
# HOW:  Set 120s timeout for comprehensive tests, 90s for medium-weight tests
# NOTE: These must come at the END of the file after all add_subdirectory calls
#       so the tests actually exist when set_tests_properties is called

# Comprehensive brain tests - these create multiple full brain instances
# RESOURCE_LOCK prevents OOM when running -j2 alongside other brain tests
set_tests_properties(
  unit_core_brain_test_brain_comprehensive
  unit_core_brain_test_brain_comprehensive_coverage
  unit_core_brain_test_brain_exhaustive_scenarios
  unit_core_brain_test_brain_advanced_coverage
  unit_core_brain_test_brain_100_percent_coverage
  unit_core_brain_test_brain_final_coverage
  unit_core_brain_test_brain_final_push_95
  PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy"
)

# Medium-weight brain tests - single brain instance with extensive validation
set_tests_properties(
  unit_core_brain_test_brain_persistence
  unit_core_brain_test_brain_error_paths
  unit_core_brain_test_brain_snapshot_operations
  unit_core_brain_test_brain_oscillations_immune_integration
  PROPERTIES TIMEOUT 420 RESOURCE_LOCK "brain_heavy"
)
# brain_cache_mutex creates per-test brains (12 tests × ~35s each)
set_tests_properties(
  unit_core_brain_test_brain_cache_mutex
  PROPERTIES TIMEOUT 900 RESOURCE_LOCK "brain_heavy"
)
# oscillations_immune_full has 30 tests with per-test immune+bridge create (~600s)
set_tests_properties(
  unit_core_brain_test_oscillations_immune_full_integration
  PROPERTIES TIMEOUT 900 RESOURCE_LOCK "brain_heavy"
)
# network_analyzer includes LargeBrain_HandlesGracefully (142s)
set_tests_properties(
  unit_core_brain_test_brain_network_analyzer
  PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy"
)
# brain_master includes AllBrainSizesAndTasks (109s) + StressTest + LongRunningSimulation
set_tests_properties(
  unit_core_brain_test_brain_master
  PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy"
)
# brain_cache_threadsafe has 34 tests including a 55s cache-overwrite benchmark
set_tests_properties(
  unit_core_brain_test_brain_cache_threadsafe
  PROPERTIES TIMEOUT 180 RESOURCE_LOCK "brain_heavy"
)

# Additional brain tests discovered via add_test_binary macro
set_tests_properties(
  unit_core_brain_test_brain_finetune
  unit_core_brain_test_brain_layer_freezing
  unit_core_brain_test_brain_edge_cases
  unit_core_brain_test_brain_cycle_coordinator
  PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy"
)

# Hemispheric brain tests - timeouts set in unit/core/brain/hemispheric/CMakeLists.txt

# Factory tests - brain creation and subsystem initialization
# Timeouts for add_test entries set in unit/core/brain/factory/CMakeLists.txt
# Auto-discovered duplicates from init/ dir need RESOURCE_LOCK here
if(TARGET unit_core_brain_factory_init_test_brain_init_basal_ganglia)
  set_tests_properties(
    unit_core_brain_factory_init_test_brain_init_basal_ganglia
    unit_core_brain_factory_init_test_brain_init_hypothalamus
    unit_core_brain_factory_init_test_brain_init_coordinators
    unit_core_brain_factory_init_test_brain_init_allocation
    unit_core_brain_factory_init_test_brain_init_builders
    unit_core_brain_factory_init_test_brain_init_config
    unit_core_brain_factory_init_test_brain_init_infrastructure
    unit_core_brain_factory_init_test_brain_init_integration_config
    unit_core_brain_factory_init_test_brain_init_network
    unit_core_brain_factory_init_test_brain_init_subsystems_part1
    unit_core_brain_factory_init_test_brain_init_subsystems_part2
    PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy"
  )
endif()
# Factory-managed tests already have RESOURCE_LOCK set in factory/CMakeLists.txt

# Utils async/fault tolerance tests
# Note: timeout for test_async_checkpoint is set in unit/utils/fault_tolerance/CMakeLists.txt

# Cognitive wellbeing tests - heavyweight mental health subsystem
# Note: these are managed in unit/cognitive/wellbeing/CMakeLists.txt

# Salience comprehensive test - full attention system
# set_tests_properties(
#   unit_cognitive_salience_test_salience_comprehensive
#   PROPERTIES TIMEOUT 90
# )

# Working memory adapter - cognitive middleware integration
# Note: managed in unit/middleware/CMakeLists.txt

# Brain integration test - full system integration
# Note: managed in unit/middleware/CMakeLists.txt

# Thread Safety Audit Tests (P0-P3 fixes)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/utils/thread/CMakeLists.txt")
    message(STATUS "Adding utils thread unit tests...")
    add_subdirectory(unit/utils/thread)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/thread_safety/CMakeLists.txt")
    message(STATUS "Adding thread safety regression tests...")
    add_subdirectory(regression/thread_safety)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/cross_module/CMakeLists.txt")
    message(STATUS "Adding cross-module walkthrough fixes regression tests...")
    add_subdirectory(regression/cross_module)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/thread_safety/CMakeLists.txt")
    message(STATUS "Adding thread safety integration tests...")
    add_subdirectory(integration/thread_safety)
endif()

# =============================================================================
# Backprop Kernel Backward Compatibility Regression Tests
# =============================================================================
# WHAT: Regression tests for backprop kernel API changes (max_grad_norm, out_grad_norm)
# WHY:  Athena 95% accuracy plan added gradient clipping params; verify backward compat
# =============================================================================
add_executable(regression_test_backprop_kernel_compat
    regression/test_backprop_kernel_backward_compat.cpp
)
target_link_libraries(regression_test_backprop_kernel_compat
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(regression_test_backprop_kernel_compat PRIVATE cxx_std_17)
gtest_discover_tests(regression_test_backprop_kernel_compat
    PROPERTIES LABELS "regression;plasticity;backprop;backward_compat"
    TIMEOUT 120
)

# NOTE: integration_gpu_test_gpu_cpu_fallback_integration and
# integration_test_backprop_grad_clip are defined in
# test/integration/CMakeLists.txt, which is included via
# add_subdirectory(integration) above (line ~1949).

# ============================================================================
# Brain Subsystem Unit Tests (Glymphatic, Endocannabinoid, Neuropeptide)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/regions")
    message(STATUS "Adding brain regions (glymphatic/endocannabinoid/neuropeptide) unit tests...")
    add_subdirectory(unit/regions)
endif()

# ============================================================================
# Cortical Interneuron Unit Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/cortical")
    message(STATUS "Adding cortical interneuron unit tests...")
    add_subdirectory(unit/cortical)
endif()

# ============================================================================
# White Matter Tract Unit Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/white_matter")
    message(STATUS "Adding white matter tract unit tests...")
    add_subdirectory(unit/white_matter)
endif()

# ============================================================================
# Subcortical Unit Tests (Inferior Colliculus)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/subcortical")
    message(STATUS "Adding subcortical (inferior colliculus) unit tests...")
    add_subdirectory(unit/subcortical)
endif()

# ============================================================================
# Spinal Cord Unit Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/spinal")
    message(STATUS "Adding spinal cord unit tests...")
    add_subdirectory(unit/spinal)
endif()

# ============================================================================
# Synapse Compute + Multimodal Integration Bug Fix Tests
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/synapse_compute/test_synapse_compute_bugs.c")
    message(STATUS "Adding synapse compute bug fix tests...")
    add_executable(unit_core_synapse_compute_bugs
        unit/core/synapse_compute/test_synapse_compute_bugs.c
    )
    target_include_directories(unit_core_synapse_compute_bugs PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(unit_core_synapse_compute_bugs PRIVATE c_std_11)
    target_compile_options(unit_core_synapse_compute_bugs PRIVATE
        -Wall -Wextra
    )
    target_link_libraries(unit_core_synapse_compute_bugs PRIVATE
        nimcp Threads::Threads check_pic subunit rt m
    )
    add_test(NAME unit_core_synapse_compute_bugs
             COMMAND unit_core_synapse_compute_bugs
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(unit_core_synapse_compute_bugs
        PROPERTIES
            LABELS "unit;core;synapse_compute;multimodal;bugfix"
            TIMEOUT 120)
endif()

# ============================================================================
# Memory Store Tests (SQLite + FTS5 + vector search)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/memory")
    message(STATUS "Adding memory store unit tests...")
    add_subdirectory(unit/memory)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/memory")
    message(STATUS "Adding memory store integration tests...")
    add_subdirectory(integration/memory)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/memory")
    message(STATUS "Adding memory store regression tests...")
    add_subdirectory(regression/memory)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/e2e/memory")
    message(STATUS "Adding memory store e2e tests...")
    add_subdirectory(e2e/memory)
endif()

# Edge module tests (swarm runtime, sensor hub, watchdog, drone bridges)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/edge")
    message(STATUS "Adding edge module unit tests...")
    add_subdirectory(unit/edge)
endif()

# ==========================================================================
# Simulation Engine Tests (gtest — physics, chemistry, biology, math, integration)
# ==========================================================================

if(GTest_FOUND)

macro(add_sim_test TEST_NAME TEST_SRC)
    add_executable(${TEST_NAME} ${TEST_SRC})
    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${TEST_NAME} nimcp GTest::GTest GTest::Main m)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endmacro()

# Physics simulation engine tests
add_sim_test(test_intuitive_physics     unit/physics/simulation/test_intuitive_physics.cpp)
add_sim_test(test_relativistic          unit/physics/simulation/test_relativistic.cpp)
add_sim_test(test_electromagnetic       unit/physics/simulation/test_electromagnetic.cpp)
add_sim_test(test_qed                   unit/physics/simulation/test_qed.cpp)
add_sim_test(test_qft                   unit/physics/simulation/test_qft.cpp)
add_sim_test(test_astrophysics          unit/physics/simulation/test_astrophysics.cpp)
add_sim_test(test_nuclear               unit/physics/simulation/test_nuclear.cpp)
add_sim_test(test_surface_physics       unit/physics/simulation/test_surface_physics.cpp)
add_sim_test(test_world_simulator       unit/physics/simulation/test_world_simulator.cpp)
add_sim_test(test_world_model_bridges   unit/physics/simulation/test_world_model_bridges.cpp)

# Chemistry simulation engine tests
add_sim_test(test_chemistry_sim         unit/chemistry/simulation/test_chemistry_sim.cpp)
add_sim_test(test_surface_chemistry     unit/chemistry/simulation/test_surface_chemistry.cpp)
add_sim_test(test_biochemistry          unit/chemistry/simulation/test_biochemistry.cpp)
add_sim_test(test_physical_chemistry    unit/chemistry/simulation/test_physical_chemistry.cpp)
add_sim_test(test_organic_chemistry     unit/chemistry/simulation/test_organic_chemistry.cpp)
add_sim_test(test_analytical_chemistry  unit/chemistry/simulation/test_analytical_chemistry.cpp)

# Biology simulation engine tests
add_sim_test(test_biology_sim           unit/biology/simulation/test_biology_sim.cpp)
add_sim_test(test_cell_biology          unit/biology/simulation/test_cell_biology.cpp)
add_sim_test(test_immunology            unit/biology/simulation/test_immunology.cpp)
add_sim_test(test_ecology               unit/biology/simulation/test_ecology.cpp)
add_sim_test(test_physiology            unit/biology/simulation/test_physiology.cpp)

# Mathematics engine tests
add_sim_test(test_number_theory         unit/math/test_number_theory.cpp)
add_sim_test(test_linear_algebra_eng    unit/math/test_linear_algebra.cpp)
add_sim_test(test_probability           unit/math/test_probability.cpp)
add_sim_test(test_numerical_methods     unit/math/test_numerical_methods.cpp)
add_sim_test(test_graph_theory          unit/math/test_graph_theory.cpp)
add_sim_test(test_combinatorics         unit/math/test_combinatorics.cpp)
add_sim_test(test_topology              unit/math/test_topology.cpp)
add_sim_test(test_logic                 unit/math/test_logic.cpp)
add_sim_test(test_zeta                  unit/math/test_zeta.cpp)
add_sim_test(test_knot_theory           unit/math/test_knot_theory.cpp)
add_sim_test(test_complexity            unit/math/test_complexity.cpp)
add_sim_test(test_optimization          unit/math/test_optimization.cpp)
add_sim_test(test_information_theory    unit/math/test_information_theory.cpp)
add_sim_test(test_dsp                   unit/math/test_dsp.cpp)

# Integration tests (cross-engine coupling)
add_sim_test(test_cross_engine_coupling integration/world_model/test_cross_engine_coupling.cpp)

# Regression tests (conservation laws, known formulas — must NEVER fail)
add_sim_test(test_conservation_laws     regression/test_conservation_laws.cpp)
add_sim_test(test_known_formulas        regression/test_known_formulas.cpp)

# End-to-end tests (full simulation pipeline)
add_sim_test(test_brain_simulation_pipeline e2e/test_brain_simulation_pipeline.cpp)

endif() # GTest_FOUND

# ============================================================================
# Cognitive Dispatch Tests (Parallel Actor Pattern)
# ============================================================================
if(GTest_FOUND)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/cognitive_dispatch")
    message(STATUS "Adding cognitive dispatch unit tests...")
    add_subdirectory(unit/core/cognitive_dispatch)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/core/cognitive_dispatch")
    message(STATUS "Adding cognitive dispatch integration tests...")
    add_subdirectory(integration/core/cognitive_dispatch)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/core/cognitive_dispatch")
    message(STATUS "Adding cognitive dispatch regression tests...")
    add_subdirectory(regression/core/cognitive_dispatch)
endif()
endif() # GTest_FOUND cognitive dispatch

# ============================================================================
# Unified Checkpoint Tests
# ============================================================================
if(GTest_FOUND)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/core/persistence")
    message(STATUS "Adding unified checkpoint unit tests...")
    add_subdirectory(unit/core/persistence)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/core/persistence")
    message(STATUS "Adding unified checkpoint integration tests...")
    add_subdirectory(integration/core/persistence)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/core/persistence")
    message(STATUS "Adding unified checkpoint regression tests...")
    add_subdirectory(regression/core/persistence)
endif()
endif() # GTest_FOUND checkpoint

# ============================================================================
# Cycle Coordinator Driven Cycle Tests (integration + regression)
# ============================================================================
# WHAT: Integration + regression tests for brain_cycle_coordinator_register_driven().
# WHY:  Top-level test/integration and test/regression dirs have their own
#       CMakeLists.txt that skip auto-discovery. Register targets explicitly.
# HOW:  Plain C files, link against libnimcp; no GTest/libcheck needed.

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/test_cycle_coordinator_driven_integration.c")
    add_executable(integration_test_cycle_coordinator_driven_integration
        integration/test_cycle_coordinator_driven_integration.c
    )
    target_include_directories(integration_test_cycle_coordinator_driven_integration PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(integration_test_cycle_coordinator_driven_integration
        PRIVATE c_std_11)
    target_link_libraries(integration_test_cycle_coordinator_driven_integration
        PRIVATE nimcp Threads::Threads rt m)
    add_test(NAME integration_test_cycle_coordinator_driven_integration
             COMMAND integration_test_cycle_coordinator_driven_integration
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(integration_test_cycle_coordinator_driven_integration
        PROPERTIES LABELS "integration;cycle_coordinator;driven" TIMEOUT 30)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/test_cycle_coordinator_regression.c")
    add_executable(regression_test_cycle_coordinator_regression
        regression/test_cycle_coordinator_regression.c
    )
    target_include_directories(regression_test_cycle_coordinator_regression PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_test_cycle_coordinator_regression
        PRIVATE c_std_11)
    target_link_libraries(regression_test_cycle_coordinator_regression
        PRIVATE nimcp Threads::Threads rt m)
    add_test(NAME regression_test_cycle_coordinator_regression
             COMMAND regression_test_cycle_coordinator_regression
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(regression_test_cycle_coordinator_regression
        PROPERTIES LABELS "regression;cycle_coordinator" TIMEOUT 15)
endif()

# ============================================================================
# Wave 4 Tests — predictive_immune wiring + meta_learning hook
# ============================================================================
# WHAT: Integration + regression tests for Wave 4. Uses the same plain-C,
#       explicit-registration pattern as the cycle coordinator tests above.
# WHY:  test/integration and test/regression both have their own CMakeLists
#       that disable auto-discovery of top-level test files.
# HOW:  Plain C files owning their own main(), linked against libnimcp only.

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/test_wave4_integration.c")
    add_executable(integration_test_wave4_integration
        integration/test_wave4_integration.c
    )
    target_include_directories(integration_test_wave4_integration PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(integration_test_wave4_integration
        PRIVATE c_std_11)
    target_link_libraries(integration_test_wave4_integration
        PRIVATE nimcp Threads::Threads rt m)
    add_test(NAME integration_test_wave4_integration
             COMMAND integration_test_wave4_integration
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(integration_test_wave4_integration
        PROPERTIES LABELS "integration;cycle_coordinator;wave4" TIMEOUT 30)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/test_wave4_regression.c")
    add_executable(regression_test_wave4_regression
        regression/test_wave4_regression.c
    )
    target_include_directories(regression_test_wave4_regression PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_test_wave4_regression
        PRIVATE c_std_11)
    target_link_libraries(regression_test_wave4_regression
        PRIVATE nimcp Threads::Threads rt m)
    add_test(NAME regression_test_wave4_regression
             COMMAND regression_test_wave4_regression
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(regression_test_wave4_regression
        PROPERTIES LABELS "regression;cycle_coordinator;wave4" TIMEOUT 15)
endif()

# ---------------------------------------------------------------------------
# Wave 4 unit tests — plain C, explicit registration
#
# WHAT: Wave 4 unit tests live directly in test/unit/*.c (no subdir). The
#       discover_category_tests(unit) pass WOULD pick them up, but that
#       path links against GTest + Python + full helper sources, which is
#       overkill (and breaks compilation if a test has its own main and
#       includes brain_internal.h which conflicts with some GTest headers).
# WHY:  Match the lightweight pattern used by unit_test_cycle_coordinator_driven.
# HOW:  Explicit executable + link libnimcp only; same format as integration.
# ---------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_wave4_predictive_immune_init.c")
    # Only define if not already created by discover_category_tests(unit).
    if(NOT TARGET unit_test_wave4_predictive_immune_init)
        add_executable(unit_test_wave4_predictive_immune_init
            unit/test_wave4_predictive_immune_init.c
        )
        target_include_directories(unit_test_wave4_predictive_immune_init PRIVATE
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src
        )
        target_compile_features(unit_test_wave4_predictive_immune_init
            PRIVATE c_std_11)
        target_link_libraries(unit_test_wave4_predictive_immune_init
            PRIVATE nimcp Threads::Threads rt m)
        add_test(NAME unit_test_wave4_predictive_immune_init
                 COMMAND unit_test_wave4_predictive_immune_init
                 WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
        set_tests_properties(unit_test_wave4_predictive_immune_init
            PROPERTIES LABELS "unit;cycle_coordinator;wave4" TIMEOUT 15)
    endif()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_wave4_meta_learning_hook.c")
    if(NOT TARGET unit_test_wave4_meta_learning_hook)
        add_executable(unit_test_wave4_meta_learning_hook
            unit/test_wave4_meta_learning_hook.c
        )
        target_include_directories(unit_test_wave4_meta_learning_hook PRIVATE
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src
        )
        target_compile_features(unit_test_wave4_meta_learning_hook
            PRIVATE c_std_11)
        target_link_libraries(unit_test_wave4_meta_learning_hook
            PRIVATE nimcp Threads::Threads rt m)
        add_test(NAME unit_test_wave4_meta_learning_hook
                 COMMAND unit_test_wave4_meta_learning_hook
                 WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
        set_tests_properties(unit_test_wave4_meta_learning_hook
            PROPERTIES LABELS "unit;meta_learning;wave4" TIMEOUT 15)
    endif()
endif()

# ---------------------------------------------------------------------------
# Wave 5 tests — plain C, explicit registration (mirror Wave 4 pattern).
#
# WHAT: Wave 5 wires intuitive_physics_step() into the post-forward parallel
#       stage pipeline as a new stage_physics_task. Tests live at the unit,
#       integration, and regression tiers; e2e lives under test/e2e/.
# WHY:  Match the lightweight pattern used by the Wave-4 binaries.
# HOW:  Explicit executable + link libnimcp only; plain C with own main().
# ---------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_wave5_physics_stage.c")
    if(NOT TARGET unit_test_wave5_physics_stage)
        add_executable(unit_test_wave5_physics_stage
            unit/test_wave5_physics_stage.c
        )
        target_include_directories(unit_test_wave5_physics_stage PRIVATE
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src
        )
        target_compile_features(unit_test_wave5_physics_stage
            PRIVATE c_std_11)
        target_link_libraries(unit_test_wave5_physics_stage
            PRIVATE nimcp Threads::Threads rt m)
        add_test(NAME unit_test_wave5_physics_stage
                 COMMAND unit_test_wave5_physics_stage
                 WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
        set_tests_properties(unit_test_wave5_physics_stage
            PROPERTIES LABELS "unit;parallel_stages;physics;wave5" TIMEOUT 15)
    endif()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/test_wave5_integration.c")
    add_executable(integration_test_wave5_integration
        integration/test_wave5_integration.c
    )
    target_include_directories(integration_test_wave5_integration PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(integration_test_wave5_integration
        PRIVATE c_std_11)
    target_link_libraries(integration_test_wave5_integration
        PRIVATE nimcp Threads::Threads rt m)
    add_test(NAME integration_test_wave5_integration
             COMMAND integration_test_wave5_integration
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(integration_test_wave5_integration
        PROPERTIES LABELS "integration;parallel_stages;physics;wave5" TIMEOUT 15)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/test_wave5_regression.c")
    add_executable(regression_test_wave5_regression
        regression/test_wave5_regression.c
    )
    target_include_directories(regression_test_wave5_regression PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_test_wave5_regression
        PRIVATE c_std_11)
    target_link_libraries(regression_test_wave5_regression
        PRIVATE nimcp Threads::Threads rt m)
    add_test(NAME regression_test_wave5_regression
             COMMAND regression_test_wave5_regression
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(regression_test_wave5_regression
        PROPERTIES LABELS "regression;parallel_stages;physics;wave5" TIMEOUT 15)
endif()

# ---------------------------------------------------------------------------
# Wave 6 tests — chemistry cluster wiring (plain C, mirror Wave 4/5 pattern).
#
# WHAT: Wave 6 added BRAIN_CYCLE_CHEMISTRY (10ms cadence) driving 4 previously-
#       statue modules (proton_pumps, buffer_systems, ph_dynamics, nitric_oxide)
#       via a single tick wrapper in brain_init_chemistry. Tests cover unit
#       (factory init/destroy contract), integration (coexistence with other
#       driven cycles), regression (enum-count/slot/name pinning), and e2e
#       (5-cycle soak) tiers.
# WHY:  Match the lightweight pattern used by Wave 4/5 binaries.
# HOW:  Explicit executable + link libnimcp only; plain C with own main().
# ---------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_wave6_chemistry_init.c")
    if(NOT TARGET unit_test_wave6_chemistry_init)
        add_executable(unit_test_wave6_chemistry_init
            unit/test_wave6_chemistry_init.c
        )
        target_include_directories(unit_test_wave6_chemistry_init PRIVATE
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src
        )
        target_compile_features(unit_test_wave6_chemistry_init
            PRIVATE c_std_11)
        target_link_libraries(unit_test_wave6_chemistry_init
            PRIVATE nimcp Threads::Threads rt m)
        add_test(NAME unit_test_wave6_chemistry_init
                 COMMAND unit_test_wave6_chemistry_init
                 WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
        set_tests_properties(unit_test_wave6_chemistry_init
            PROPERTIES LABELS "unit;cycle_coordinator;chemistry;wave6" TIMEOUT 15)
    endif()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/test_wave6_integration.c")
    add_executable(integration_test_wave6_integration
        integration/test_wave6_integration.c
    )
    target_include_directories(integration_test_wave6_integration PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(integration_test_wave6_integration
        PRIVATE c_std_11)
    target_link_libraries(integration_test_wave6_integration
        PRIVATE nimcp Threads::Threads rt m)
    add_test(NAME integration_test_wave6_integration
             COMMAND integration_test_wave6_integration
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(integration_test_wave6_integration
        PROPERTIES LABELS "integration;cycle_coordinator;chemistry;wave6" TIMEOUT 15)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/test_wave6_regression.c")
    add_executable(regression_test_wave6_regression
        regression/test_wave6_regression.c
    )
    target_include_directories(regression_test_wave6_regression PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_test_wave6_regression
        PRIVATE c_std_11)
    target_link_libraries(regression_test_wave6_regression
        PRIVATE nimcp Threads::Threads rt m)
    add_test(NAME regression_test_wave6_regression
             COMMAND regression_test_wave6_regression
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(regression_test_wave6_regression
        PROPERTIES LABELS "regression;cycle_coordinator;chemistry;wave6" TIMEOUT 15)
endif()

# ============================================================================
# Wave 8A: Cochlea + 15 Consumer Bridges — Unit + Integration + Regression
# ============================================================================
# WHAT: Wave 8A wired cochlea_t + 15 cochlea_*_bridge_t consumers into a
#       single BRAIN_CYCLE_COCHLEA_BRIDGES driven cycle at 10ms, plus hot-
#       path call sites in brain_decide / brain_learn_vector.
# WHY:  Catch regressions in init/destroy control flow (16 opaque
#       pointers), enum ordinal drift (14 pre-existing + 1 new), and
#       cross-cycle starvation when the new 10ms cycle coexists with the
#       Wave 6 chemistry cycle on the same coordinator.
# HOW:  Explicit executable + link libnimcp only; plain C with own main().
# ---------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_wave8a_cochlea_init.c")
    if(NOT TARGET unit_test_wave8a_cochlea_init)
        add_executable(unit_test_wave8a_cochlea_init
            unit/test_wave8a_cochlea_init.c
        )
        target_include_directories(unit_test_wave8a_cochlea_init PRIVATE
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src
        )
        target_compile_features(unit_test_wave8a_cochlea_init
            PRIVATE c_std_11)
        target_link_libraries(unit_test_wave8a_cochlea_init
            PRIVATE nimcp Threads::Threads rt m)
        add_test(NAME unit_test_wave8a_cochlea_init
                 COMMAND unit_test_wave8a_cochlea_init
                 WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
        set_tests_properties(unit_test_wave8a_cochlea_init
            PROPERTIES LABELS "unit;cycle_coordinator;cochlea;wave8a" TIMEOUT 15)
    endif()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/test_wave8a_integration.c")
    add_executable(integration_test_wave8a_integration
        integration/test_wave8a_integration.c
    )
    target_include_directories(integration_test_wave8a_integration PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(integration_test_wave8a_integration
        PRIVATE c_std_11)
    target_link_libraries(integration_test_wave8a_integration
        PRIVATE nimcp Threads::Threads rt m)
    add_test(NAME integration_test_wave8a_integration
             COMMAND integration_test_wave8a_integration
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(integration_test_wave8a_integration
        PROPERTIES LABELS "integration;cycle_coordinator;cochlea;wave8a" TIMEOUT 15)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regression/test_wave8a_regression.c")
    add_executable(regression_test_wave8a_regression
        regression/test_wave8a_regression.c
    )
    target_include_directories(regression_test_wave8a_regression PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_test_wave8a_regression
        PRIVATE c_std_11)
    target_link_libraries(regression_test_wave8a_regression
        PRIVATE nimcp Threads::Threads rt m)
    add_test(NAME regression_test_wave8a_regression
             COMMAND regression_test_wave8a_regression
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(regression_test_wave8a_regression
        PROPERTIES LABELS "regression;cycle_coordinator;cochlea;wave8a" TIMEOUT 15)
endif()

# Overflow Check Macro Tests (P3-7) - auto-discovered by discover_category_tests(unit)
