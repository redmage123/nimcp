# Cognitive Meta Learning Integration Tests

# Meta Learning SNN-Plasticity Integration Tests
add_executable(integration_cognitive_meta_learning_snn_plasticity
    test_meta_learning_snn_plasticity_integration.cpp
)
target_link_libraries(integration_cognitive_meta_learning_snn_plasticity
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(integration_cognitive_meta_learning_snn_plasticity
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(integration_cognitive_meta_learning_snn_plasticity PRIVATE cxx_std_17)
gtest_discover_tests(integration_cognitive_meta_learning_snn_plasticity
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;meta_learning;snn;plasticity"
        TIMEOUT 180
)
