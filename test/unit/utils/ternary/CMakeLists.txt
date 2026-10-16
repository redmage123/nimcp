#=============================================================================
# CMakeLists.txt - Ternary Logic Unit Tests
#=============================================================================

# Ternary core tests
add_executable(unit_utils_ternary_test_ternary_core
    test_ternary_core.cpp
)

target_include_directories(unit_utils_ternary_test_ternary_core PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(unit_utils_ternary_test_ternary_core
    nimcp
    GTest::gtest
    GTest::gtest_main
    m
)

# Add test
add_test(NAME unit_utils_ternary_test_ternary_core
         COMMAND unit_utils_ternary_test_ternary_core --gtest_brief=1)
