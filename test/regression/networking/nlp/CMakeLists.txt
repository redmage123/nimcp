# ============================================================================
# Neural Link Protocol (NLP) Regression Tests
# ============================================================================
#
# WHAT: Regression tests for the Neural Link Protocol
# WHY:  Ensure protocol performance, stability, and security don't regress
# HOW:  Performance benchmarks, stability tests, security validation
#
# TEST CATEGORIES:
# - Performance: Throughput, latency, resource usage
# - Stability: Long-running sessions, error recovery, memory leaks
# - Security: Replay protection, key rotation, signature validation
#
# @author NIMCP Development Team
# @date 2025-12-08
# ============================================================================

# Guard: Check GTest availability
if(NOT GTest_FOUND)
    message(STATUS "GTest not found - skipping NLP regression tests")
    return()
endif()

set(NLP_REGRESSION_TEST_SOURCES
    test_nlp_performance.cpp
    test_nlp_stability.cpp
    test_nlp_security_regression.cpp
)

foreach(test_src ${NLP_REGRESSION_TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)
    set(test_target "regression_networking_nlp_${test_name}")

    # Check if source file exists
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${test_src})
        add_executable(${test_target} ${test_src})

        target_include_directories(${test_target} PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/networking/nlp
            ${GTEST_INCLUDE_DIRS}
        )

        # Enable C++17 for NIMCP tests
        target_compile_features(${test_target} PRIVATE cxx_std_17)

        # Enable test-only APIs
        target_compile_definitions(${test_target} PRIVATE NIMCP_TESTING)

        target_link_libraries(${test_target} PRIVATE
            nimcp
            GTest::GTest
            GTest::Main
            pthread
        )

        add_test(NAME ${test_target} COMMAND ${test_target})

        # Set appropriate timeouts per test type
        if(test_name STREQUAL "test_nlp_performance")
            # Performance tests: 5 minutes (includes peer scaling tests)
            set_tests_properties(${test_target} PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy")
        elseif(test_name STREQUAL "test_nlp_stability")
            # Stability tests: 5 minutes (includes long-running tests)
            set_tests_properties(${test_target} PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy")
        elseif(test_name STREQUAL "test_nlp_security_regression")
            # Security tests: 3 minutes (includes key rotation tests)
            set_tests_properties(${test_target} PROPERTIES TIMEOUT 180)
        else()
            set_tests_properties(${test_target} PROPERTIES TIMEOUT 300)
        endif()

        # Add test labels for organization
        set_tests_properties(${test_target} PROPERTIES
            LABELS "regression;networking;nlp"
        )
    else()
        message(STATUS "NLP regression test source not found: ${test_src}")
    endif()
endforeach()

# ============================================================================
# Test Documentation
# ============================================================================

# Performance Baselines (as of 2025-12-08):
# - Message Throughput: >1000 msg/sec (standard mode)
# - Encryption Overhead: <2ms per message
# - Burst Buffering: >90% efficiency
# - Peer Scaling: Linear to 50 peers
# - Memory Growth: <1MB per 1000 messages
#
# Stability Requirements:
# - 1000+ messages without failure
# - Thread-safe concurrent operations
# - Graceful error recovery
# - No memory leaks
# - Robust connection management
#
# Security Requirements:
# - Replay attacks blocked (60s window)
# - Tampered messages detected (AES-GCM)
# - Key rotation preserves sessions
# - Sequence wraparound handled safely
# - Timestamp boundaries correct

# ============================================================================
# Usage Notes
# ============================================================================

# Run specific test suites:
# $ ctest -R test_nlp_performance
# $ ctest -R test_nlp_stability
# $ ctest -R test_nlp_security
#
# Run all NLP regression tests:
# $ ctest -L nlp
#
# Run with verbose output:
# $ ctest -R nlp -V
#
# Run only performance tests:
# $ ctest -L performance
