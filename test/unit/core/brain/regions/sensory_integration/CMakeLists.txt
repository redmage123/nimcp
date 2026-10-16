# ============================================================================
# Sensory Integration Unit Tests
# ============================================================================

macro(add_sensory_integration_unit_test TEST_NAME)
    add_executable(unit_core_brain_regions_sensory_integration_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_core_brain_regions_sensory_integration_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    target_include_directories(unit_core_brain_regions_sensory_integration_${TEST_NAME}
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(unit_core_brain_regions_sensory_integration_${TEST_NAME}
        PRIVATE cxx_std_17
    )
    target_compile_definitions(unit_core_brain_regions_sensory_integration_${TEST_NAME}
        PRIVATE NIMCP_TESTING
    )
    add_test(NAME unit_core_brain_regions_sensory_integration_${TEST_NAME}
             COMMAND unit_core_brain_regions_sensory_integration_${TEST_NAME})
    set_tests_properties(unit_core_brain_regions_sensory_integration_${TEST_NAME}
        PROPERTIES
            LABELS "unit;core;brain;sensory_integration"
            TIMEOUT 120
    )
endmacro()

# Trigeminal-Oral Bridge Unit Tests
add_sensory_integration_unit_test(test_trigeminal_oral_bridge)
