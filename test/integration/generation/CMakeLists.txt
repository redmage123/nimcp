# =============================================================================
# NIMCP Generation Pipeline Integration Tests
# =============================================================================
# WHAT: Integration tests for the full tokenizer -> embedding -> generator pipeline
# WHY:  Verify components compose correctly — vocab IDs flow through embeddings
#       into the generator, and generated IDs decode back to valid text
# HOW:  GTest executable linked against nimcp library
# =============================================================================

add_executable(integration_generation_test_pipeline
    test_generation_pipeline_integration.cpp
)

target_link_libraries(integration_generation_test_pipeline
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_generation_test_pipeline
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_generation_test_pipeline PRIVATE cxx_std_17)

gtest_discover_tests(integration_generation_test_pipeline
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;generation;pipeline"
        TIMEOUT 180
)
