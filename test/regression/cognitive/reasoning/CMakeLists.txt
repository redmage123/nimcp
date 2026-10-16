# ============================================================================
# Reasoning Regression Tests
# ============================================================================

# Test 1: Reasoning performance
add_test_binary(
    test_reasoning_performance
    ${CMAKE_CURRENT_SOURCE_DIR}/test_reasoning_performance.cpp
    regression
)
# Test 2: Reasoning memory
add_test_binary(
    test_reasoning_memory
    ${CMAKE_CURRENT_SOURCE_DIR}/test_reasoning_memory.cpp
    regression
)
# Test 3: Rule learning accuracy
add_test_binary(
    test_rule_learning_accuracy
    ${CMAKE_CURRENT_SOURCE_DIR}/test_rule_learning_accuracy.cpp
    regression
)
# Test 4: Proof finding speed
add_test_binary(
    test_proof_finding_speed
    ${CMAKE_CURRENT_SOURCE_DIR}/test_proof_finding_speed.cpp
    regression
)
# Test 5: Reasoning regression
# DISABLED: Uses unimplemented APIs (brain_compile_rule_to_neural, etc.)
# add_test_binary(
#     test_reasoning_regression
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_reasoning_regression.cpp
#     regression
# )

# Reasoning SNN-Plasticity Regression Tests
add_executable(regression_cognitive_reasoning_snn_plasticity
    test_reasoning_snn_plasticity_regression.cpp
)
target_link_libraries(regression_cognitive_reasoning_snn_plasticity
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(regression_cognitive_reasoning_snn_plasticity
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(regression_cognitive_reasoning_snn_plasticity PRIVATE cxx_std_17)
gtest_discover_tests(regression_cognitive_reasoning_snn_plasticity
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;reasoning;snn;plasticity"
        TIMEOUT 180
)

# Reasoning Calibration Regression Tests
add_executable(regression_cognitive_reasoning_calibration
    test_reasoning_calibration_regression.cpp
)
target_link_libraries(regression_cognitive_reasoning_calibration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(regression_cognitive_reasoning_calibration
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(regression_cognitive_reasoning_calibration PRIVATE cxx_std_17)
gtest_discover_tests(regression_cognitive_reasoning_calibration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;reasoning;calibration"
        TIMEOUT 180
)

# Reasoning Metacognition Regression Tests
add_executable(regression_cognitive_reasoning_metacognition
    test_reasoning_metacognition_regression.cpp
)
target_link_libraries(regression_cognitive_reasoning_metacognition
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(regression_cognitive_reasoning_metacognition
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(regression_cognitive_reasoning_metacognition PRIVATE cxx_std_17)
gtest_discover_tests(regression_cognitive_reasoning_metacognition
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;reasoning;metacognition"
        TIMEOUT 180
)

# Reasoning Affective Modulation Regression Tests
add_executable(regression_cognitive_reasoning_affective
    test_reasoning_affective_regression.cpp
)
target_link_libraries(regression_cognitive_reasoning_affective
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(regression_cognitive_reasoning_affective
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(regression_cognitive_reasoning_affective PRIVATE cxx_std_17)
gtest_discover_tests(regression_cognitive_reasoning_affective
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;reasoning;affective"
        TIMEOUT 180
)

# Reasoning Abduction Regression Tests
add_executable(regression_cognitive_reasoning_abduction
    test_reasoning_abduction_regression.cpp
)
target_link_libraries(regression_cognitive_reasoning_abduction
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(regression_cognitive_reasoning_abduction
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(regression_cognitive_reasoning_abduction PRIVATE cxx_std_17)
gtest_discover_tests(regression_cognitive_reasoning_abduction
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;reasoning;abduction"
        TIMEOUT 180
)

# Reasoning Causal Regression Tests
add_executable(regression_cognitive_reasoning_causal
    test_reasoning_causal_regression.cpp
)
target_link_libraries(regression_cognitive_reasoning_causal
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(regression_cognitive_reasoning_causal
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(regression_cognitive_reasoning_causal PRIVATE cxx_std_17)
gtest_discover_tests(regression_cognitive_reasoning_causal
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;reasoning;causal"
        TIMEOUT 180
)

# Reasoning Visuospatial Regression Tests
add_executable(regression_cognitive_reasoning_visuospatial
    test_reasoning_visuospatial_regression.cpp
)
target_link_libraries(regression_cognitive_reasoning_visuospatial
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(regression_cognitive_reasoning_visuospatial
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(regression_cognitive_reasoning_visuospatial PRIVATE cxx_std_17)
gtest_discover_tests(regression_cognitive_reasoning_visuospatial
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;reasoning;visuospatial"
        TIMEOUT 180
)
