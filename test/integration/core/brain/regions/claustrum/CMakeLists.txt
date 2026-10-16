# ============================================================================
# Claustrum Integration Tests
# ============================================================================
# WHAT: Build configuration for Claustrum module integration tests
# WHY:  Test consciousness binding, cross-modal integration, and global workspace
# HOW:  Individual executables for each integration test suite

# Helper macro for claustrum integration tests
macro(add_claustrum_integration_test TEST_NAME)
    add_executable(integration_core_brain_regions_claustrum_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(integration_core_brain_regions_claustrum_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    target_include_directories(integration_core_brain_regions_claustrum_${TEST_NAME}
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(integration_core_brain_regions_claustrum_${TEST_NAME}
        PRIVATE cxx_std_17
    )
    target_compile_definitions(integration_core_brain_regions_claustrum_${TEST_NAME}
        PRIVATE NIMCP_TESTING
    )
    add_test(NAME integration_core_brain_regions_claustrum_${TEST_NAME}
             COMMAND integration_core_brain_regions_claustrum_${TEST_NAME})
    set_tests_properties(integration_core_brain_regions_claustrum_${TEST_NAME}
        PROPERTIES
            LABELS "integration;core;brain;claustrum"
            TIMEOUT 180
    )
endmacro()

# Brain init integration tests - Tests claustrum initialization and lifecycle
add_claustrum_integration_test(test_claustrum_brain_init_integration)

# Binding integration tests - Tests cross-modal binding and percept management
add_claustrum_integration_test(test_claustrum_binding_integration)

# Workspace integration tests - Tests global workspace and consciousness
add_claustrum_integration_test(test_claustrum_workspace_integration)

# Bio-async integration tests - Tests bio-async messaging system
add_claustrum_integration_test(test_claustrum_bio_async_integration)
