# Cognitive Meta Learning Unit Tests

# Meta Learning SNN Bridge Unit Tests
add_executable(unit_cognitive_meta_learning_snn_bridge
    test_meta_learning_snn_bridge.cpp
)
target_link_libraries(unit_cognitive_meta_learning_snn_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(unit_cognitive_meta_learning_snn_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(unit_cognitive_meta_learning_snn_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_meta_learning_snn_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;meta_learning;snn"
        TIMEOUT 120
)

# Meta Learning Plasticity Bridge Unit Tests
add_executable(unit_cognitive_meta_learning_plasticity_bridge
    test_meta_learning_plasticity_bridge.cpp
)
target_link_libraries(unit_cognitive_meta_learning_plasticity_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(unit_cognitive_meta_learning_plasticity_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(unit_cognitive_meta_learning_plasticity_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_meta_learning_plasticity_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;meta_learning;plasticity"
        TIMEOUT 120
)
