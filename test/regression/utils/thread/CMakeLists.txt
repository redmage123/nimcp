# Thread Utils Regression Tests
# Regression tests for threading and synchronization primitives

# Thread Sync Regression Test (GTest C++)
add_executable(regression_utils_thread_test_thread_sync_regression
    test_thread_sync_regression.cpp
)
target_link_libraries(regression_utils_thread_test_thread_sync_regression
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)
target_include_directories(regression_utils_thread_test_thread_sync_regression
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME regression_utils_thread_test_thread_sync_regression
         COMMAND regression_utils_thread_test_thread_sync_regression)

# Mutex Pool Regression Test (GTest C++)
add_executable(regression_utils_thread_test_mutex_pool_regression
    test_mutex_pool_regression.cpp
)
target_link_libraries(regression_utils_thread_test_mutex_pool_regression
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)
target_include_directories(regression_utils_thread_test_mutex_pool_regression
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME regression_utils_thread_test_mutex_pool_regression
         COMMAND regression_utils_thread_test_mutex_pool_regression)

# P1-P3 Mutex Regression Test (GTest C++)
add_executable(regression_utils_thread_test_mutex_p1p3_regression
    ../test_mutex_regression.cpp
)
target_link_libraries(regression_utils_thread_test_mutex_p1p3_regression
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)
target_include_directories(regression_utils_thread_test_mutex_p1p3_regression
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME regression_utils_thread_test_mutex_p1p3_regression
         COMMAND regression_utils_thread_test_mutex_p1p3_regression)
set_tests_properties(regression_utils_thread_test_mutex_p1p3_regression
                     PROPERTIES TIMEOUT 120 LABELS "regression")
