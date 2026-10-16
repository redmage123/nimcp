# ============================================================================
# Executive Function Integration Tests
# ============================================================================

# Executive SNN-Plasticity Integration Test
add_executable(integration_cognitive_executive_snn_plasticity
    test_executive_snn_plasticity_integration.cpp
)

target_link_libraries(integration_cognitive_executive_snn_plasticity
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_executive_snn_plasticity
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_cognitive_executive_snn_plasticity PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_executive_snn_plasticity
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;executive;snn;plasticity"
        TIMEOUT 180
)

# ============================================================================
# Executive-Cognitive Integration Test
# ============================================================================
# WHAT: Test executive module with meta-controller and working memory
# WHY:  Verify cognitive coordination and slot allocation arbitration
# HOW:  Test executive-meta connection and WM slot management

add_executable(integration_executive_cognitive
    test_executive_cognitive_integration.cpp
)

target_link_libraries(integration_executive_cognitive
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_executive_cognitive
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_executive_cognitive PRIVATE cxx_std_17)

gtest_discover_tests(integration_executive_cognitive
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;executive;metacontroller"
        TIMEOUT 180
)
