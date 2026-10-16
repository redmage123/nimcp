# ============================================================================
# Inner Dialogue Engine Unit Tests
# ============================================================================
# WHAT: Unit tests for Turn History, Perspective Registry, Convergence, Engine
# WHY:  Verify each inner dialogue function in isolation
# HOW:  GoogleTest fixtures with table-driven tests
# ============================================================================

add_executable(unit_cognitive_inner_dialogue
    test_inner_dialogue_unit.cpp
)

target_link_libraries(unit_cognitive_inner_dialogue
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_inner_dialogue
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_inner_dialogue PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_inner_dialogue
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;inner_dialogue"
        TIMEOUT 180
)
