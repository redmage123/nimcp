#=============================================================================
# CMakeLists.txt - Statistics Regression Tests
#=============================================================================

# Regression test macro
macro(add_statistics_regression_test TEST_NAME)
    add_executable(regression_statistics_${TEST_NAME} ${TEST_NAME}.cpp)
    target_link_libraries(regression_statistics_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
    )
    target_include_directories(regression_statistics_${TEST_NAME}
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
    )
    target_compile_features(regression_statistics_${TEST_NAME} PRIVATE cxx_std_17)
    add_test(
        NAME regression_statistics_${TEST_NAME}
        COMMAND regression_statistics_${TEST_NAME}
    )
endmacro()

# Regression tests
add_statistics_regression_test(test_statistics_regression)

# =============================================================================
# Statistics Enhancement Module Regression Tests (Phase STATS-ENH)
# =============================================================================

# Neural statistics regression tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_neural_statistics_regression.cpp)
    add_statistics_regression_test(test_neural_statistics_regression)
endif()

# Information theory regression tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_information_theory_regression.cpp)
    add_statistics_regression_test(test_information_theory_regression)
endif()

# Multivariate analysis regression tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_multivariate_regression.cpp)
    add_statistics_regression_test(test_multivariate_regression)
endif()

# Time series analysis regression tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_timeseries_regression.cpp)
    add_statistics_regression_test(test_timeseries_regression)
endif()

# ML statistics regression tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_ml_statistics_regression.cpp)
    add_statistics_regression_test(test_ml_statistics_regression)
endif()

# Streaming statistics regression tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_streaming_statistics_regression.cpp)
    add_statistics_regression_test(test_streaming_statistics_regression)
endif()

# Survival and Bayesian combined regression tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_survival_bayesian_regression.cpp)
    add_statistics_regression_test(test_survival_bayesian_regression)
endif()
