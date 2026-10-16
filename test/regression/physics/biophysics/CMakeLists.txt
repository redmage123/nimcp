# Regression tests for Hodgkin-Huxley biophysics module

# =============================================================================
# HH Regression Tests
# =============================================================================
add_executable(regression_physics_biophysics_test_hh_regression
    test_hh_regression.cpp
)

target_link_libraries(regression_physics_biophysics_test_hh_regression
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_physics_biophysics_test_hh_regression
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_physics_biophysics_test_hh_regression PRIVATE cxx_std_17)

gtest_discover_tests(regression_physics_biophysics_test_hh_regression
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;physics;biophysics;hh"
        TIMEOUT 120
)
