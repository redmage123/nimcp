# ============================================================================
# NIMCP Dendrite Integration Tests
# ============================================================================
# WHAT: Integration tests for dendrite module
# WHY:  Verify cross-module interactions and system-level behavior
# HOW:  GTest-based tests with multi-component scenarios
# ============================================================================

add_executable(integration_core_dendrite_test_integration
    test_dendrite_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/core/dendrite/nimcp_dendrite.c
)

target_link_libraries(integration_core_dendrite_test_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)

target_compile_features(integration_core_dendrite_test_integration PRIVATE cxx_std_17)

target_include_directories(integration_core_dendrite_test_integration
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

# Enable NIMCP_TESTING for test APIs
target_compile_definitions(integration_core_dendrite_test_integration PRIVATE NIMCP_TESTING)

add_test(NAME integration_core_dendrite_test_integration
         COMMAND integration_core_dendrite_test_integration)

set_tests_properties(integration_core_dendrite_test_integration
    PROPERTIES
        TIMEOUT 180
        LABELS "integration;core;dendrite"
)
