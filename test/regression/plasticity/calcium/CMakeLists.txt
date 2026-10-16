# Regression tests for Calcium Dynamics stability

add_executable(test_calcium_regression
    test_calcium_regression.cpp
)

target_include_directories(test_calcium_regression PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/plasticity
    ${CMAKE_SOURCE_DIR}/include/plasticity/calcium
    ${CMAKE_SOURCE_DIR}/include/cognitive
    ${CMAKE_SOURCE_DIR}/include/async
    ${CMAKE_SOURCE_DIR}/include/utils
    ${GTEST_INCLUDE_DIRS}
)

target_link_libraries(test_calcium_regression
    nimcp
    ${GTEST_LIBRARIES}
    pthread
    m
)

add_test(NAME CalciumRegressionTests
         COMMAND test_calcium_regression)

set_tests_properties(CalciumRegressionTests PROPERTIES
    TIMEOUT 180
    LABELS "regression;plasticity;calcium;stability"
)
