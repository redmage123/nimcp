# Cerebellum Region Regression Tests

macro(add_cerebellum_regression_test TEST_NAME)
    add_executable(regression_core_brain_regions_cerebellum_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(regression_core_brain_regions_cerebellum_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    add_test(NAME regression_core_brain_regions_cerebellum_${TEST_NAME}
             COMMAND regression_core_brain_regions_cerebellum_${TEST_NAME})
endmacro()

# Cerebellum regression tests
add_cerebellum_regression_test(test_cerebellum_regression)
set_tests_properties(regression_core_brain_regions_cerebellum_test_cerebellum_regression PROPERTIES
    TIMEOUT 300
    RESOURCE_LOCK "brain_heavy"
)
