# CMakeLists.txt for constants unit tests

# Test executable for constants validation
add_executable(unit_constants_test
    test_constants.cpp
)

target_link_libraries(unit_constants_test
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(unit_constants_test
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

# Add to CTest
add_test(
    NAME unit_constants_test
    COMMAND unit_constants_test
)

set_tests_properties(unit_constants_test
    PROPERTIES
        LABELS "unit;constants"
        TIMEOUT 60
)
