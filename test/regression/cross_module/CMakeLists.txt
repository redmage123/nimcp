# Cross-Module Walkthrough Fixes Regression Tests

add_executable(regression_cross_module_walkthrough_fixes
    test_cross_module_walkthrough_fixes.cpp
)

target_link_libraries(regression_cross_module_walkthrough_fixes
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cross_module_walkthrough_fixes
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_cross_module_walkthrough_fixes PRIVATE cxx_std_17)

gtest_discover_tests(regression_cross_module_walkthrough_fixes
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cross_module;walkthrough"
        TIMEOUT 60
)
