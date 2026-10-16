# ============================================================================
# Security Exception Integration Tests
# ============================================================================
# WHAT: Integration tests for security-exception-immune system pipelines
# WHY:  Verify security modules, exception system, and immune system work together
# HOW:  Test complete flow from security threat detection through exception handling
#       to immune response and recovery actions
# ============================================================================

# Security Exception Integration Tests
add_executable(integration_security_test_exception
    test_security_exception_integration.cpp
)
target_link_libraries(integration_security_test_exception
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(integration_security_test_exception
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/utils/exception
        ${CMAKE_SOURCE_DIR}/include/utils/error
        ${CMAKE_SOURCE_DIR}/include/cognitive/immune
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/utils/exception
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/async
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(integration_security_test_exception PRIVATE cxx_std_17)
add_test(NAME integration_security_test_exception
         COMMAND integration_security_test_exception)
set_tests_properties(integration_security_test_exception PROPERTIES
    LABELS "integration;security;exception;immune"
    TIMEOUT 300
)
