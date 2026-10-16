# Parahippocampal Cortex Integration Tests
# Tests integration with security, immune, bio-async, KG, SNN, and plasticity systems

# Macro to add parahippocampal integration tests
macro(add_parahipp_integration_test TEST_NAME)
    add_executable(integration_core_brain_regions_parahipp_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(integration_core_brain_regions_parahipp_${TEST_NAME}
        PRIVATE GTest::GTest GTest::Main nimcp m pthread
    )
    target_include_directories(integration_core_brain_regions_parahipp_${TEST_NAME}
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/core
        ${CMAKE_SOURCE_DIR}/include/core/brain
        ${CMAKE_SOURCE_DIR}/include/core/brain/regions/parahippocampal
        ${CMAKE_SOURCE_DIR}/include/snn
        ${CMAKE_SOURCE_DIR}/include/plasticity
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/include/utils/logging
    )
    add_test(
        NAME integration_core_brain_regions_parahipp_${TEST_NAME}
        COMMAND integration_core_brain_regions_parahipp_${TEST_NAME}
    )
    set_tests_properties(integration_core_brain_regions_parahipp_${TEST_NAME}
        PROPERTIES
        LABELS "integration;core;brain;parahippocampal"
        TIMEOUT 60
    )
endmacro()

# Add all parahippocampal integration tests
add_parahipp_integration_test(test_parahipp_brain_init_integration)
add_parahipp_integration_test(test_parahipp_bio_async_integration)
add_parahipp_integration_test(test_parahipp_snn_plasticity_integration)
add_parahipp_integration_test(test_parahipp_cognitive_integration)
add_parahipp_integration_test(test_parahipp_immune_integration)
add_parahipp_integration_test(test_parahipp_security_integration)
add_parahipp_integration_test(test_parahipp_kg_wiring_integration)
