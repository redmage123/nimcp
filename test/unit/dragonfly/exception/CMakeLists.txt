# Unit tests for dragonfly exception handling

# Dragonfly Exception Handling Test
add_executable(unit_dragonfly_exception_handling
    test_dragonfly_exception_handling.cpp
)

target_include_directories(unit_dragonfly_exception_handling PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(unit_dragonfly_exception_handling
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_dragonfly_exception_handling PRIVATE cxx_std_17)

add_test(
    NAME unit_dragonfly_exception_handling
    COMMAND unit_dragonfly_exception_handling --gtest_brief=1
)

set_tests_properties(unit_dragonfly_exception_handling PROPERTIES
    TIMEOUT 60
    LABELS "unit;dragonfly;exception"
)
