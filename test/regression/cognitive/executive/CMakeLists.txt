# ============================================================================
# Executive Function Regression Tests
# ============================================================================

# Executive SNN-Plasticity Regression Test
add_executable(regression_cognitive_executive_snn_plasticity
    test_executive_snn_plasticity_regression.cpp
)

target_link_libraries(regression_cognitive_executive_snn_plasticity
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_executive_snn_plasticity
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_cognitive_executive_snn_plasticity PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_executive_snn_plasticity
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;executive;snn;plasticity"
        TIMEOUT 300
)
