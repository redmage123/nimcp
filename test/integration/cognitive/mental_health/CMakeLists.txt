# ============================================================================
# Mental Health Guardian Integration Tests
# ============================================================================
# WHAT: Tests guardian integration with brain factory and lifecycle
# WHY:  Verify guardian works correctly within brain infrastructure
# HOW:  Test factory init, brain accessors, lifecycle coordination

add_executable(integration_cognitive_mental_health_guardian_brain
    test_guardian_brain_integration.cpp
)

target_link_libraries(integration_cognitive_mental_health_guardian_brain
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_mental_health_guardian_brain
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_cognitive_mental_health_guardian_brain PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_mental_health_guardian_brain
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;mental_health;guardian"
        TIMEOUT 120
)
