# Core Directives Unit Tests

# Test: Action History
add_executable(unit_core_directives_action_history
    test_action_history.cpp
)

target_link_libraries(unit_core_directives_action_history
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
)

target_include_directories(unit_core_directives_action_history PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

add_test(
    NAME unit_core_directives_action_history
    COMMAND unit_core_directives_action_history --gtest_brief=1
)

# Note: Additional test files will be added here as more directive modules are implemented
# Planned tests (pending implementation):
#   - test_harm_classifier.cpp
#   - test_harm_prevention.cpp (First Law)
#   - test_command_compliance.cpp (Second Law)
#   - test_self_preservation.cpp (Third Law)
#   - test_reciprocity_eval.cpp (Golden Rule)
#   - test_combinatorial_harm.cpp (moved from ethics)
#   - test_core_directives.cpp (main orchestrator)
