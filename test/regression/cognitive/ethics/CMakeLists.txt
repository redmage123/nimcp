# ============================================================================
# Cognitive Ethics Regression Tests
# ============================================================================
# Numerical stability, memory, and performance regression testing

# ============================================================================
# Ethics-SNN-Plasticity Regression Tests
# ============================================================================
# Numerical stability, memory, and performance regression testing

add_executable(regression_cognitive_ethics_snn_plasticity
    test_ethics_snn_plasticity_regression.cpp
)
target_link_libraries(regression_cognitive_ethics_snn_plasticity
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(regression_cognitive_ethics_snn_plasticity
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/time
        ${CMAKE_SOURCE_DIR}/src/cognitive/ethics
        ${CMAKE_SOURCE_DIR}/src/snn
        ${CMAKE_SOURCE_DIR}/src/plasticity
)
target_compile_features(regression_cognitive_ethics_snn_plasticity PRIVATE cxx_std_17)
add_test(NAME regression_cognitive_ethics_snn_plasticity
         COMMAND regression_cognitive_ethics_snn_plasticity)
set_tests_properties(regression_cognitive_ethics_snn_plasticity PROPERTIES
    TIMEOUT 180
    LABELS "regression;cognitive;ethics;snn;plasticity"
)
