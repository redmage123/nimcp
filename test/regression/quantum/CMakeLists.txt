# ============================================================================
# Quantum Bridge Regression Tests
# ============================================================================
# WHAT: Regression tests for quantum bridges stability and consistency
# WHY:  Ensure quantum bridge behavior doesn't regress over time
# HOW:  Test edge cases, boundary conditions, and behavioral consistency

add_executable(regression_quantum_bridges
    test_quantum_bridges_regression.cpp
)

target_include_directories(regression_quantum_bridges PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(regression_quantum_bridges
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)

target_compile_features(regression_quantum_bridges PRIVATE cxx_std_17)

add_test(NAME regression_quantum_bridges COMMAND regression_quantum_bridges)
set_tests_properties(regression_quantum_bridges PROPERTIES LABELS "regression;quantum")

# ============================================================================
# Quantum RNG Regression Test
# ============================================================================
# WHAT: Regression tests for Quantum RNG Box-Muller implementation
# WHY:  Lock in correct divisor and dual-output behavior
# HOW:  Test golden values, distribution properties, edge cases

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_quantum_rng_regression.cpp)
    add_executable(regression_quantum_rng
        test_quantum_rng_regression.cpp
    )

    target_include_directories(regression_quantum_rng PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    target_link_libraries(regression_quantum_rng
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )

    target_compile_features(regression_quantum_rng PRIVATE cxx_std_17)

    add_test(NAME regression_quantum_rng COMMAND regression_quantum_rng)
    set_tests_properties(regression_quantum_rng PROPERTIES LABELS "regression;quantum;rng")

    message(STATUS "  Added quantum RNG regression test")
endif()
