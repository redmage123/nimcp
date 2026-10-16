# Cognitive Introspection Regression Tests (Phase 1.5.4)

# Connectivity Health Regression Tests
add_executable(regression_cognitive_introspection_connectivity_health
    test_connectivity_health_regression.cpp
)
target_link_libraries(regression_cognitive_introspection_connectivity_health
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(regression_cognitive_introspection_connectivity_health
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/core/brain
        ${CMAKE_SOURCE_DIR}/src/core/brain_regions
        ${CMAKE_SOURCE_DIR}/src/cognitive/introspection
)
add_test(NAME regression_cognitive_introspection_connectivity_health
         COMMAND regression_cognitive_introspection_connectivity_health)
set_tests_properties(regression_cognitive_introspection_connectivity_health PROPERTIES LABELS "regression")

# Auto Activity History Regression Tests
add_executable(regression_cognitive_introspection_auto_activity_history
    test_auto_activity_history_regression.cpp
)
target_link_libraries(regression_cognitive_introspection_auto_activity_history
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(regression_cognitive_introspection_auto_activity_history
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/core/brain
        ${CMAKE_SOURCE_DIR}/src/cognitive/introspection
)
add_test(NAME regression_cognitive_introspection_auto_activity_history
         COMMAND regression_cognitive_introspection_auto_activity_history)
set_tests_properties(regression_cognitive_introspection_auto_activity_history PROPERTIES LABELS "regression")

# Consciousness Metrics Regression Tests
add_executable(regression_cognitive_introspection_consciousness_metrics
    test_consciousness_metrics_regression.cpp
)
target_link_libraries(regression_cognitive_introspection_consciousness_metrics
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(regression_cognitive_introspection_consciousness_metrics
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/core/brain
        ${CMAKE_SOURCE_DIR}/src/cognitive/introspection
)
add_test(NAME regression_cognitive_introspection_consciousness_metrics
         COMMAND regression_cognitive_introspection_consciousness_metrics)
set_tests_properties(regression_cognitive_introspection_consciousness_metrics PROPERTIES
    LABELS "regression" TIMEOUT 180 RESOURCE_LOCK "brain_heavy")

# Ensemble Uncertainty Regression Tests
add_executable(regression_cognitive_introspection_ensemble_uncertainty
    test_ensemble_uncertainty_regression.cpp
)
target_link_libraries(regression_cognitive_introspection_ensemble_uncertainty
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(regression_cognitive_introspection_ensemble_uncertainty
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/core/brain
        ${CMAKE_SOURCE_DIR}/src/cognitive/introspection
)
add_test(NAME regression_cognitive_introspection_ensemble_uncertainty
         COMMAND regression_cognitive_introspection_ensemble_uncertainty)
set_tests_properties(regression_cognitive_introspection_ensemble_uncertainty PROPERTIES
    LABELS "regression" TIMEOUT 180 RESOURCE_LOCK "brain_heavy"
)

# Temporal Patterns Regression Tests
add_executable(regression_cognitive_introspection_temporal_patterns
    test_temporal_patterns_regression.cpp
)
target_link_libraries(regression_cognitive_introspection_temporal_patterns
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(regression_cognitive_introspection_temporal_patterns
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/core/brain
        ${CMAKE_SOURCE_DIR}/src/cognitive/introspection
)
add_test(NAME regression_cognitive_introspection_temporal_patterns
         COMMAND regression_cognitive_introspection_temporal_patterns)
set_tests_properties(regression_cognitive_introspection_temporal_patterns PROPERTIES LABELS "regression")

# Introspection SNN-Plasticity Regression Tests
add_executable(regression_cognitive_introspection_snn_plasticity
    test_introspection_snn_plasticity_regression.cpp
)
target_link_libraries(regression_cognitive_introspection_snn_plasticity
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(regression_cognitive_introspection_snn_plasticity
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(regression_cognitive_introspection_snn_plasticity PRIVATE cxx_std_17)
gtest_discover_tests(regression_cognitive_introspection_snn_plasticity
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;introspection;snn;plasticity"
        TIMEOUT 180
)
