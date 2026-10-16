# ============================================================================
# NIMCP Axon Integration Tests
# ============================================================================

add_executable(integration_core_axon_test_integration
    test_axon_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/core/axon/nimcp_axon.c
)

target_link_libraries(integration_core_axon_test_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)

target_compile_features(integration_core_axon_test_integration PRIVATE cxx_std_17)

target_include_directories(integration_core_axon_test_integration
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME integration_core_axon_test_integration
         COMMAND integration_core_axon_test_integration)

set_tests_properties(integration_core_axon_test_integration
    PROPERTIES
        TIMEOUT 120
        LABELS "integration;core;axon"
)
