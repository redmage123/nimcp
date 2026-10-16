# Biology Neurovascular Regression Tests CMakeLists.txt

# Neurovascular regression test
add_executable(regression_biology_neurovascular_test_neurovascular_regression
    test_neurovascular_regression.cpp
)

target_include_directories(regression_biology_neurovascular_test_neurovascular_regression PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(regression_biology_neurovascular_test_neurovascular_regression
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME regression_biology_neurovascular_test_neurovascular_regression
    COMMAND regression_biology_neurovascular_test_neurovascular_regression)
