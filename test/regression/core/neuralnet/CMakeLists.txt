# ============================================================================
# Neuralnet Regression Tests - Spike Ring Buffer
# ============================================================================

add_executable(regression_core_neuralnet_spike_ring_buffer
    test_spike_ring_buffer_regression.cpp
)

target_link_libraries(regression_core_neuralnet_spike_ring_buffer
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)

target_compile_features(regression_core_neuralnet_spike_ring_buffer PRIVATE cxx_std_17)

target_include_directories(regression_core_neuralnet_spike_ring_buffer
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME regression_core_neuralnet_spike_ring_buffer
         COMMAND regression_core_neuralnet_spike_ring_buffer)

set_tests_properties(regression_core_neuralnet_spike_ring_buffer
    PROPERTIES
        TIMEOUT 120
        LABELS "regression;core;neuralnet;spike_ring_buffer"
        RESOURCE_LOCK "brain_heavy"
)
