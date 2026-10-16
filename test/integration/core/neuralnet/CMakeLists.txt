# ============================================================================
# NIMCP Neuralnet-Homeostasis-Immune Integration Tests
# ============================================================================

add_executable(integration_core_neuralnet_homeostasis_immune
    test_homeostasis_immune_integration.cpp
)

target_link_libraries(integration_core_neuralnet_homeostasis_immune
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)

target_compile_features(integration_core_neuralnet_homeostasis_immune PRIVATE cxx_std_17)

target_include_directories(integration_core_neuralnet_homeostasis_immune
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME integration_core_neuralnet_homeostasis_immune
         COMMAND integration_core_neuralnet_homeostasis_immune)

set_tests_properties(integration_core_neuralnet_homeostasis_immune
    PROPERTIES
        TIMEOUT 120
        LABELS "integration;core;neuralnet;homeostasis;immune"
)

# Spike Ring Buffer Integration Tests
add_executable(integration_core_neuralnet_spike_ring_buffer
    test_spike_ring_buffer_integration.cpp
)

target_link_libraries(integration_core_neuralnet_spike_ring_buffer
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)

target_compile_features(integration_core_neuralnet_spike_ring_buffer PRIVATE cxx_std_17)

target_include_directories(integration_core_neuralnet_spike_ring_buffer
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME integration_core_neuralnet_spike_ring_buffer
         COMMAND integration_core_neuralnet_spike_ring_buffer)

set_tests_properties(integration_core_neuralnet_spike_ring_buffer
    PROPERTIES
        TIMEOUT 120
        LABELS "integration;core;neuralnet;spike_ring_buffer"
)
