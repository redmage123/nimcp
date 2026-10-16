# Cognitive Meta Learning Regression Tests

# Meta Learning SNN-Plasticity Regression Tests
add_executable(regression_cognitive_meta_learning_snn_plasticity
    test_meta_learning_snn_plasticity_regression.cpp
)
target_link_libraries(regression_cognitive_meta_learning_snn_plasticity
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(regression_cognitive_meta_learning_snn_plasticity
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(regression_cognitive_meta_learning_snn_plasticity PRIVATE cxx_std_17)
gtest_discover_tests(regression_cognitive_meta_learning_snn_plasticity
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;meta_learning;snn;plasticity"
        TIMEOUT 180
)
