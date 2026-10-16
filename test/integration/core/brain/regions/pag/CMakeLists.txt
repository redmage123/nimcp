# ============================================================================
# PAG (Periaqueductal Gray) Integration Tests
# ============================================================================
# WHAT: Build configuration for PAG module integration tests
# WHY:  Test defensive behaviors, pain modulation, and survival responses
# HOW:  Individual executables for each integration test suite

# Helper macro for PAG integration tests
macro(add_pag_integration_test TEST_NAME)
    add_executable(integration_core_brain_regions_pag_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(integration_core_brain_regions_pag_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    target_include_directories(integration_core_brain_regions_pag_${TEST_NAME}
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(integration_core_brain_regions_pag_${TEST_NAME}
        PRIVATE cxx_std_17
    )
    target_compile_definitions(integration_core_brain_regions_pag_${TEST_NAME}
        PRIVATE NIMCP_TESTING
    )
    add_test(NAME integration_core_brain_regions_pag_${TEST_NAME}
             COMMAND integration_core_brain_regions_pag_${TEST_NAME})
    set_tests_properties(integration_core_brain_regions_pag_${TEST_NAME}
        PROPERTIES
            LABELS "integration;core;brain;pag"
            TIMEOUT 180
    )
endmacro()

# Brain init integration tests - Tests PAG initialization and lifecycle
add_pag_integration_test(test_pag_brain_init_integration)
