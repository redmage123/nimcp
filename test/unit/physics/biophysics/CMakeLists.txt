# Biophysics Unit Tests
# Test coverage for nimcp_hodgkin_huxley.h

# Helper macro to add biophysics test
macro(add_biophysics_test TEST_NAME)
    add_executable(unit_physics_biophysics_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_physics_biophysics_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME unit_physics_biophysics_${TEST_NAME}
             COMMAND unit_physics_biophysics_${TEST_NAME})
endmacro()

# Core biophysics tests
add_biophysics_test(test_hodgkin_huxley)
