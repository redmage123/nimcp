# ============================================================================
# 40-Watt Brain Optimization Regression Tests
# ============================================================================
add_executable(regression_optimization
    test_optimization_regression.cpp
)
target_link_libraries(regression_optimization
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(regression_optimization
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(regression_optimization PRIVATE cxx_std_17)
gtest_discover_tests(regression_optimization
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "regression;optimization" TIMEOUT 300
)

# Hot Path Regression Tests
add_executable(regression_hotpath
    test_hotpath_regression.cpp
)
target_link_libraries(regression_hotpath
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(regression_hotpath
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(regression_hotpath PRIVATE cxx_std_17)
gtest_discover_tests(regression_hotpath
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "regression;optimization;hot_path" TIMEOUT 300
)

# Resource Regression Tests (Phase 3)
add_executable(regression_resource test_resource_regression.cpp)
target_link_libraries(regression_resource PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(regression_resource PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(regression_resource PRIVATE cxx_std_17)
gtest_discover_tests(regression_resource WORKING_DIRECTORY ${CMAKE_BINARY_DIR} PROPERTIES LABELS "regression;optimization;resource" TIMEOUT 300)

# Biological Regression Tests (Phase 3)
add_executable(regression_biological test_biological_regression.cpp)
target_link_libraries(regression_biological PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(regression_biological PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(regression_biological PRIVATE cxx_std_17)
gtest_discover_tests(regression_biological WORKING_DIRECTORY ${CMAKE_BINARY_DIR} PROPERTIES LABELS "regression;optimization;biological" TIMEOUT 300)

# Substrate Regression Tests (Phase 4)
add_executable(regression_substrate test_substrate_regression.cpp)
target_link_libraries(regression_substrate PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(regression_substrate PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(regression_substrate PRIVATE cxx_std_17)
gtest_discover_tests(regression_substrate WORKING_DIRECTORY ${CMAKE_BINARY_DIR} PROPERTIES LABELS "regression;optimization;substrate" TIMEOUT 300)
