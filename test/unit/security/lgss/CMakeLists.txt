# LGSS Unit Tests

# Test files - Core Safety (A1-A5)
set(LGSS_CORE_TESTS
    test_lgss_safety_kb.cpp
    test_lgss_context.cpp
    test_lgss_action_interceptor.cpp
)

# Test files - Extended Modules (A6-A11)
set(LGSS_EXTENDED_TESTS
    test_lgss_motor_gate.cpp
    test_lgss_plasticity_guard.cpp
    test_lgss_input_validator.cpp
)

# Combined test files
set(LGSS_UNIT_TESTS
    ${LGSS_CORE_TESTS}
    ${LGSS_EXTENDED_TESTS}
)

# Create test executable
add_executable(test_lgss_unit ${LGSS_UNIT_TESTS})

target_link_libraries(test_lgss_unit
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
)

target_include_directories(test_lgss_unit
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

# Add test
add_test(NAME lgss_unit_tests COMMAND test_lgss_unit)

# Set test properties
set_tests_properties(lgss_unit_tests PROPERTIES
    TIMEOUT 120
    LABELS "unit;security;lgss"
)
