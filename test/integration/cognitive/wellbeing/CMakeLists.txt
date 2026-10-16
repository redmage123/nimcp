# ============================================================================
# Enhanced Wellbeing Integration Tests
# ============================================================================

add_executable(integration_cognitive_wellbeing_enhanced
    test_wellbeing_enhanced_integration.cpp
)

target_link_libraries(integration_cognitive_wellbeing_enhanced
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_wellbeing_enhanced
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/src/cognitive/wellbeing
        ${PROJECT_SOURCE_DIR}/src/cognitive/introspection
        ${PROJECT_SOURCE_DIR}/src/cognitive/immune
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(integration_cognitive_wellbeing_enhanced PRIVATE cxx_std_17)
target_compile_definitions(integration_cognitive_wellbeing_enhanced PRIVATE NIMCP_TESTING)

gtest_discover_tests(integration_cognitive_wellbeing_enhanced
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;wellbeing;enhanced"
        TIMEOUT 180
)

# ============================================================================
# Wellbeing SNN-Plasticity Integration Tests
# ============================================================================
add_executable(integration_cognitive_wellbeing_snn_plasticity
    test_wellbeing_snn_plasticity_integration.cpp
)

target_link_libraries(integration_cognitive_wellbeing_snn_plasticity
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_wellbeing_snn_plasticity
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/src/cognitive/wellbeing
)

target_compile_features(integration_cognitive_wellbeing_snn_plasticity PRIVATE cxx_std_17)
target_compile_definitions(integration_cognitive_wellbeing_snn_plasticity PRIVATE NIMCP_TESTING)

gtest_discover_tests(integration_cognitive_wellbeing_snn_plasticity
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;wellbeing;snn;plasticity"
        TIMEOUT 180
)
