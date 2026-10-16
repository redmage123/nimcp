# Hippocampus Integration Tests
# Tests hippocampus integration with all NIMCP subsystems

add_executable(integration_core_brain_regions_hippocampus_test_hippocampus_integration
    test_hippocampus_integration.cpp
)

target_link_libraries(integration_core_brain_regions_hippocampus_test_hippocampus_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)

add_test(NAME integration_core_brain_regions_hippocampus_test_hippocampus_integration
         COMMAND integration_core_brain_regions_hippocampus_test_hippocampus_integration)
