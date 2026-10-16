# ============================================================================
# Cognitive Theory of Mind Regression Tests
# ============================================================================
# Numerical stability, memory, and performance regression testing

# ============================================================================
# Theory of Mind-SNN-Plasticity Regression Tests
# ============================================================================
# Numerical stability, memory, and performance regression testing

add_executable(regression_cognitive_tom_snn_plasticity
    test_tom_snn_plasticity_regression.cpp
)
target_link_libraries(regression_cognitive_tom_snn_plasticity
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(regression_cognitive_tom_snn_plasticity
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/time
        ${CMAKE_SOURCE_DIR}/src/cognitive/theory_of_mind
        ${CMAKE_SOURCE_DIR}/src/snn
        ${CMAKE_SOURCE_DIR}/src/plasticity
)
target_compile_features(regression_cognitive_tom_snn_plasticity PRIVATE cxx_std_17)
add_test(NAME regression_cognitive_tom_snn_plasticity
         COMMAND regression_cognitive_tom_snn_plasticity)
set_tests_properties(regression_cognitive_tom_snn_plasticity PROPERTIES
    TIMEOUT 180
    LABELS "regression;cognitive;tom;snn;plasticity"
)

# ============================================================================
# Theory of Mind Exception Handling Regression Tests
# ============================================================================
add_executable(regression_cognitive_tom_exception
    test_tom_exception_regression.cpp
)
target_link_libraries(regression_cognitive_tom_exception
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(regression_cognitive_tom_exception
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(regression_cognitive_tom_exception PRIVATE cxx_std_17)
add_test(NAME regression_cognitive_tom_exception
         COMMAND regression_cognitive_tom_exception)
set_tests_properties(regression_cognitive_tom_exception PROPERTIES
    TIMEOUT 180
    LABELS "regression;cognitive;tom;exception"
)
