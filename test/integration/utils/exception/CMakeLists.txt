# =============================================================================
# Utils Exception Integration Tests
# Exception-Immune Bridge Integration Testing
# =============================================================================

message(STATUS "Adding Utils Exception Integration tests...")

# Helper macro to add exception integration test
macro(add_exception_integration_test TEST_NAME)
    add_executable(integration_utils_exception_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(integration_utils_exception_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            Threads::Threads
            m
    )
    target_include_directories(integration_utils_exception_${TEST_NAME}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
    )
    target_compile_features(integration_utils_exception_${TEST_NAME} PRIVATE cxx_std_17)
    add_test(NAME integration_utils_exception_${TEST_NAME}
             COMMAND integration_utils_exception_${TEST_NAME})
    set_tests_properties(integration_utils_exception_${TEST_NAME}
        PROPERTIES
        LABELS "integration;utils;exception"
        TIMEOUT 180
    )
endmacro()

# =============================================================================
# Exception-Immune Bridge Integration Tests
# =============================================================================
# Tests the complete integration between exception handling and brain immune:
# - Exception thrown -> presented to immune system as antigen
# - Recovery callback triggered -> immune system notified
# - Multiple exceptions -> proper aggregation and immune response
# - Exception with different severities -> correct immune escalation
# - Recovery callback chain (GC -> Compact -> Rollback fallback)

add_exception_integration_test(test_exception_immune_integration)

# =============================================================================
# Exception-Memory Integration Tests (P0-P2 Remediation)
# =============================================================================
# Tests the interaction between exception handling and memory tracking:
# - Exception thrown during allocation doesn't corrupt tracking
# - Recursion guard prevents stack overflow with nested throws
# - Platform mutex works independently of exception system

add_exception_integration_test(test_exception_memory_integration)
