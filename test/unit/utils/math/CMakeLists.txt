# Complex Math Unit Tests
# Phase 0.5: Complex Number Support for Neural Phase Coding

# Helper macro to add complex math test
macro(add_complex_math_test TEST_NAME)
    add_executable(unit_utils_math_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_utils_math_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library for complex.h
    )
    add_test(NAME unit_utils_math_${TEST_NAME}
             COMMAND unit_utils_math_${TEST_NAME})
endmacro()

# Add all complex math tests
add_complex_math_test(test_complex_math)

# Math helpers unit test (nimcp_math_helpers.h consolidation)
add_executable(unit_utils_math_test_math_helpers
    test_math_helpers.cpp
)
target_link_libraries(unit_utils_math_test_math_helpers
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
)
target_include_directories(unit_utils_math_test_math_helpers
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)
add_test(NAME unit_utils_math_test_math_helpers
         COMMAND unit_utils_math_test_math_helpers)
set_tests_properties(unit_utils_math_test_math_helpers PROPERTIES LABELS "unit")
