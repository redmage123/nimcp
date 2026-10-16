# Cognitive Introspection Unit Tests (Phase 1.5.4)

# Connectivity Health Unit Tests
add_executable(unit_cognitive_introspection_connectivity_health
    test_connectivity_health.cpp
)
target_link_libraries(unit_cognitive_introspection_connectivity_health
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_introspection_connectivity_health
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/core/brain
        ${CMAKE_SOURCE_DIR}/src/core/brain_regions
        ${CMAKE_SOURCE_DIR}/src/cognitive/introspection
)
add_test(NAME unit_cognitive_introspection_connectivity_health
         COMMAND unit_cognitive_introspection_connectivity_health)
set_tests_properties(unit_cognitive_introspection_connectivity_health PROPERTIES LABELS "unit")

# Auto Activity History Unit Tests
add_executable(unit_cognitive_introspection_auto_activity_history
    test_auto_activity_history.cpp
)
target_link_libraries(unit_cognitive_introspection_auto_activity_history
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_introspection_auto_activity_history
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/core/brain
        ${CMAKE_SOURCE_DIR}/src/cognitive/introspection
)
add_test(NAME unit_cognitive_introspection_auto_activity_history
         COMMAND unit_cognitive_introspection_auto_activity_history)
set_tests_properties(unit_cognitive_introspection_auto_activity_history PROPERTIES LABELS "unit")

# Consciousness Metrics Unit Tests
add_executable(unit_cognitive_introspection_consciousness_metrics
    test_consciousness_metrics.cpp
)
target_link_libraries(unit_cognitive_introspection_consciousness_metrics
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_introspection_consciousness_metrics
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/core/brain
        ${CMAKE_SOURCE_DIR}/src/cognitive/introspection
)
add_test(NAME unit_cognitive_introspection_consciousness_metrics
         COMMAND unit_cognitive_introspection_consciousness_metrics)
set_tests_properties(unit_cognitive_introspection_consciousness_metrics PROPERTIES LABELS "unit")

# Ensemble Uncertainty Unit Tests
add_executable(unit_cognitive_introspection_ensemble_uncertainty
    test_ensemble_uncertainty.cpp
)
target_link_libraries(unit_cognitive_introspection_ensemble_uncertainty
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_introspection_ensemble_uncertainty
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/core/brain
        ${CMAKE_SOURCE_DIR}/src/cognitive/introspection
)
add_test(NAME unit_cognitive_introspection_ensemble_uncertainty
         COMMAND unit_cognitive_introspection_ensemble_uncertainty)
set_tests_properties(unit_cognitive_introspection_ensemble_uncertainty PROPERTIES LABELS "unit")

# Temporal Patterns Unit Tests
add_executable(unit_cognitive_introspection_temporal_patterns
    test_temporal_patterns.cpp
)
target_link_libraries(unit_cognitive_introspection_temporal_patterns
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_introspection_temporal_patterns
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/core/brain
        ${CMAKE_SOURCE_DIR}/src/cognitive/introspection
)
add_test(NAME unit_cognitive_introspection_temporal_patterns
         COMMAND unit_cognitive_introspection_temporal_patterns)
set_tests_properties(unit_cognitive_introspection_temporal_patterns PROPERTIES LABELS "unit")

# Immune Integration Unit Tests
add_executable(unit_cognitive_introspection_immune_integration
    test_introspection_immune_integration.cpp
)
target_link_libraries(unit_cognitive_introspection_immune_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_cognitive_introspection_immune_integration
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/core/brain
        ${CMAKE_SOURCE_DIR}/src/cognitive/introspection
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
)
add_test(NAME unit_cognitive_introspection_immune_integration
         COMMAND unit_cognitive_introspection_immune_integration)
set_tests_properties(unit_cognitive_introspection_immune_integration PROPERTIES LABELS "unit")

# Substrate bridge tests
add_subdirectory(substrate)

# Introspection SNN Bridge Unit Tests
add_executable(unit_cognitive_introspection_snn_bridge
    test_introspection_snn_bridge.cpp
)
target_link_libraries(unit_cognitive_introspection_snn_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(unit_cognitive_introspection_snn_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(unit_cognitive_introspection_snn_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_introspection_snn_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;introspection;snn"
        TIMEOUT 120
)

# Introspection Plasticity Bridge Unit Tests
add_executable(unit_cognitive_introspection_plasticity_bridge
    test_introspection_plasticity_bridge.cpp
)
target_link_libraries(unit_cognitive_introspection_plasticity_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(unit_cognitive_introspection_plasticity_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(unit_cognitive_introspection_plasticity_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_introspection_plasticity_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;introspection;plasticity"
        TIMEOUT 120
)
