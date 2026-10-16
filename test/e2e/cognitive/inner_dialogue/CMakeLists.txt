# ============================================================================
# Inner Dialogue Engine E2E Tests
# ============================================================================
# WHAT: End-to-end tests for complete conversation workflows
# WHY:  Verify full deliberation pipeline: create → register → start → run → verify
# HOW:  GoogleTest with custom perspectives for convergence/deadlock/rumination
# ============================================================================

add_executable(e2e_cognitive_inner_dialogue
    test_inner_dialogue_e2e.cpp
)

target_link_libraries(e2e_cognitive_inner_dialogue
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(e2e_cognitive_inner_dialogue
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(e2e_cognitive_inner_dialogue PRIVATE cxx_std_17)
target_compile_definitions(e2e_cognitive_inner_dialogue PRIVATE NIMCP_TESTING)

gtest_discover_tests(e2e_cognitive_inner_dialogue
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "e2e;cognitive;inner_dialogue"
        TIMEOUT 300
)
