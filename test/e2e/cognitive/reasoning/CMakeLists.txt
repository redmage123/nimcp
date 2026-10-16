# ============================================================================
# Reasoning E2E Tests
# ============================================================================

# Test: Symbolic Logic Export/Import E2E
add_executable(e2e_cognitive_reasoning_symbolic_logic
    test_symbolic_logic_e2e.cpp
)
target_link_libraries(e2e_cognitive_reasoning_symbolic_logic
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_include_directories(e2e_cognitive_reasoning_symbolic_logic
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(e2e_cognitive_reasoning_symbolic_logic PRIVATE cxx_std_17)
target_compile_definitions(e2e_cognitive_reasoning_symbolic_logic PRIVATE NIMCP_TESTING)
gtest_discover_tests(e2e_cognitive_reasoning_symbolic_logic
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "e2e;cognitive;reasoning;symbolic_logic"
        TIMEOUT 300
)
