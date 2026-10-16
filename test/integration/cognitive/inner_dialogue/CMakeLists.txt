# ============================================================================
# Inner Dialogue Engine Integration Tests
# ============================================================================
# WHAT: Integration tests for Inner Dialogue with ethics, LGSS, BBB, etc.
# WHY:  Verify multi-system interaction and defense-in-depth pipeline
# HOW:  GoogleTest with full engine + subsystem connections
# ============================================================================

add_executable(integration_cognitive_inner_dialogue
    test_inner_dialogue_integration.cpp
)

target_link_libraries(integration_cognitive_inner_dialogue
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_inner_dialogue
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_cognitive_inner_dialogue PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_inner_dialogue
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;inner_dialogue"
        TIMEOUT 180
)
