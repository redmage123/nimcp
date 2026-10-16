# Chemistry pH Regression Tests CMakeLists.txt

# pH regression test
add_executable(regression_chemistry_ph_test_ph_regression
    test_ph_regression.cpp
)

target_include_directories(regression_chemistry_ph_test_ph_regression PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(regression_chemistry_ph_test_ph_regression
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME regression_chemistry_ph_test_ph_regression
    COMMAND regression_chemistry_ph_test_ph_regression)
