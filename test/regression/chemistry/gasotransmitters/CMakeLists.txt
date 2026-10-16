# Chemistry Gasotransmitters Regression Tests CMakeLists.txt

# NO regression test
add_executable(regression_chemistry_gasotransmitters_test_no_regression
    test_no_regression.cpp
)

target_include_directories(regression_chemistry_gasotransmitters_test_no_regression PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(regression_chemistry_gasotransmitters_test_no_regression
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME regression_chemistry_gasotransmitters_test_no_regression
    COMMAND regression_chemistry_gasotransmitters_test_no_regression)
