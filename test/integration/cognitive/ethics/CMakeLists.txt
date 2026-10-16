# ============================================================================
# Cognitive Ethics Integration Tests
# ============================================================================
# Ethics - SNN - Plasticity Integration Testing

# ============================================================================
# Ethics SNN+Plasticity Integration Tests
# ============================================================================
# Tests bidirectional dataflow between ethics, SNN, and plasticity

add_executable(integration_cognitive_ethics_snn_plasticity
    test_ethics_snn_plasticity_integration.cpp
)

target_link_libraries(integration_cognitive_ethics_snn_plasticity
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_ethics_snn_plasticity
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/cognitive/ethics
        ${CMAKE_SOURCE_DIR}/src/snn
        ${CMAKE_SOURCE_DIR}/src/plasticity
)

target_compile_features(integration_cognitive_ethics_snn_plasticity PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_ethics_snn_plasticity
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;ethics;snn;plasticity"
        TIMEOUT 180
)
