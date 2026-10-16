# Regression tests for parallel cognitive dispatch

add_executable(regression_cognitive_dispatch
    test_cognitive_dispatch_regression.cpp
)
target_link_libraries(regression_cognitive_dispatch
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(regression_cognitive_dispatch PRIVATE cxx_std_17)
target_include_directories(regression_cognitive_dispatch
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
gtest_discover_tests(regression_cognitive_dispatch
    PROPERTIES LABELS "regression;core;cognitive_dispatch"
    TIMEOUT 120
)
