# Regression tests for non-equilibrium thermodynamics module

# =============================================================================
# Thermodynamics Regression Tests
# =============================================================================
add_executable(regression_physics_thermodynamics_test_thermodynamics_regression
    test_thermodynamics_regression.cpp
)

target_link_libraries(regression_physics_thermodynamics_test_thermodynamics_regression
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_physics_thermodynamics_test_thermodynamics_regression
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_physics_thermodynamics_test_thermodynamics_regression PRIVATE cxx_std_17)

gtest_discover_tests(regression_physics_thermodynamics_test_thermodynamics_regression
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;physics;thermodynamics"
        TIMEOUT 120
)
