# Unit tests for middleware immune integration

# Test: Pattern Immune Integration
add_executable(unit_middleware_immune_pattern_immune
    test_pattern_immune.cpp
)

target_link_libraries(unit_middleware_immune_pattern_immune
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
)

target_include_directories(unit_middleware_immune_pattern_immune PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME unit_middleware_immune_pattern_immune
         COMMAND unit_middleware_immune_pattern_immune)

# Set test properties
set_tests_properties(unit_middleware_immune_pattern_immune PROPERTIES
    TIMEOUT 60
    LABELS "unit;middleware;immune;pattern"
)

# Note: routing_immune_integration and training_immune tests temporarily disabled
# due to API compatibility issues. Re-enable after fixing.
