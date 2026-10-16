# ============================================================================
# NIMCP Core Exception Integration Tests
# ============================================================================
#
# WHAT: Integration tests for exception handling with core modules and immune
# WHY:  Verify exception flows work correctly across module boundaries
# HOW:  Test exception dispatch, immune integration, and recovery actions
#
# ============================================================================

# Integration test for core exception handling
add_executable(integration_core_exception_test_integration
    test_core_exception_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/exception/nimcp_exception.c
    ${CMAKE_SOURCE_DIR}/src/utils/exception/nimcp_exception_handlers.c
    ${CMAKE_SOURCE_DIR}/src/utils/exception/nimcp_exception_immune.c
)

target_link_libraries(integration_core_exception_test_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)

target_compile_features(integration_core_exception_test_integration PRIVATE cxx_std_17)

target_include_directories(integration_core_exception_test_integration
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

# Enable test-only APIs
target_compile_definitions(integration_core_exception_test_integration PRIVATE NIMCP_TESTING)

add_test(NAME integration_core_exception_test_integration
         COMMAND integration_core_exception_test_integration)

set_tests_properties(integration_core_exception_test_integration
    PROPERTIES
        TIMEOUT 180
        LABELS "integration;core;exception"
)
