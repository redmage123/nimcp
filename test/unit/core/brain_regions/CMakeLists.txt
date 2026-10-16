# Brain Regions Predictive Positional Encoding Unit Tests
# Test coverage for predictive regions PE integration

# Helper macro to add predictive PE test
macro(add_predictive_pe_test TEST_NAME)
    add_executable(unit_core_brain_regions_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_core_brain_regions_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME unit_core_brain_regions_${TEST_NAME}
             COMMAND unit_core_brain_regions_${TEST_NAME})
endmacro()

# Predictive Regions PE tests
add_predictive_pe_test(test_predictive_pe)
