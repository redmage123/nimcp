# RNG Utils Unit Tests
# Test coverage for nimcp_rand.h - unified random number generation module

# Helper macro to add rng test
macro(add_rng_test TEST_NAME)
    add_executable(unit_utils_rng_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_utils_rng_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    target_compile_features(unit_utils_rng_${TEST_NAME} PRIVATE cxx_std_17)
    add_test(NAME unit_utils_rng_${TEST_NAME}
             COMMAND unit_utils_rng_${TEST_NAME})
endmacro()

# Core RNG tests
add_rng_test(test_rand)
