# Memory Utils Regression Tests
# Phase 2: Brain Substrate Pools Performance

add_executable(regression_utils_memory_test_brain_pools_regression
    test_brain_pools_regression.cpp
)
target_link_libraries(regression_utils_memory_test_brain_pools_regression
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)
add_test(NAME regression_utils_memory_test_brain_pools_regression
         COMMAND regression_utils_memory_test_brain_pools_regression)

# Phase 3 Layer Pools Regression Tests
add_executable(regression_utils_memory_test_layer_pools_regression
    test_layer_pools_regression.cpp
)
target_link_libraries(regression_utils_memory_test_layer_pools_regression
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)
add_test(NAME regression_utils_memory_test_layer_pools_regression
         COMMAND regression_utils_memory_test_layer_pools_regression)
set_tests_properties(regression_utils_memory_test_layer_pools_regression PROPERTIES
    TIMEOUT 120 RESOURCE_LOCK "brain_heavy"
)

# Unified Memory Regression Tests
add_executable(regression_utils_memory_test_unified_memory_regression
    test_unified_memory_regression.cpp
)
target_link_libraries(regression_utils_memory_test_unified_memory_regression
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)
add_test(NAME regression_utils_memory_test_unified_memory_regression
         COMMAND regression_utils_memory_test_unified_memory_regression)
set_tests_properties(regression_utils_memory_test_unified_memory_regression PROPERTIES
    TIMEOUT 120 RESOURCE_LOCK "brain_heavy")

# Memory API Regression Tests (GTest C++)
add_executable(regression_utils_memory_test_memory_api_regression
    test_memory_api_regression.cpp
)
target_link_libraries(regression_utils_memory_test_memory_api_regression
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)
target_include_directories(regression_utils_memory_test_memory_api_regression
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME regression_utils_memory_test_memory_api_regression
         COMMAND regression_utils_memory_test_memory_api_regression)

# P1-P3 Memory Regression Tests (GTest C++)
add_executable(regression_utils_memory_test_p1p3_regression
    ../test_memory_regression.cpp
)
target_link_libraries(regression_utils_memory_test_p1p3_regression
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)
target_include_directories(regression_utils_memory_test_p1p3_regression
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME regression_utils_memory_test_p1p3_regression
         COMMAND regression_utils_memory_test_p1p3_regression)
set_tests_properties(regression_utils_memory_test_p1p3_regression
                     PROPERTIES TIMEOUT 120 LABELS "regression")

# Atomic Recursion Guard Regression Tests
add_executable(regression_utils_memory_test_memory_recursion_atomic_regression
    test_memory_recursion_atomic_regression.cpp
)
target_link_libraries(regression_utils_memory_test_memory_recursion_atomic_regression
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)
target_include_directories(regression_utils_memory_test_memory_recursion_atomic_regression
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)
add_test(NAME regression_utils_memory_test_memory_recursion_atomic_regression
         COMMAND regression_utils_memory_test_memory_recursion_atomic_regression)
set_tests_properties(regression_utils_memory_test_memory_recursion_atomic_regression
                     PROPERTIES TIMEOUT 120 LABELS "regression")

# Hash Table Memory Tracking Regression Tests (linked list → hash table migration)
add_executable(regression_utils_memory_test_memory_hash_tracking_regression
    test_memory_hash_tracking_regression.cpp
)
target_link_libraries(regression_utils_memory_test_memory_hash_tracking_regression
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)
add_test(NAME regression_utils_memory_test_memory_hash_tracking_regression
         COMMAND regression_utils_memory_test_memory_hash_tracking_regression)
set_tests_properties(regression_utils_memory_test_memory_hash_tracking_regression
                     PROPERTIES TIMEOUT 120 LABELS "regression" RESOURCE_LOCK "brain_heavy")
