# Queue Container Regression Tests
# Tests performance and correctness guarantees for NIMCP queue types

add_executable(regression_utils_containers_test_queue_regression
    test_queue_regression.cpp
)
target_link_libraries(regression_utils_containers_test_queue_regression
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)
add_test(NAME regression_utils_containers_test_queue_regression
         COMMAND regression_utils_containers_test_queue_regression)
