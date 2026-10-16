# Unit tests for portia exception handling

# Portia Exception Handling Test
add_executable(unit_portia_exception_handling
    test_portia_exception_handling.cpp
)

target_include_directories(unit_portia_exception_handling PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(unit_portia_exception_handling
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_portia_exception_handling PRIVATE cxx_std_17)

add_test(
    NAME unit_portia_exception_handling
    COMMAND unit_portia_exception_handling --gtest_brief=1
)

set_tests_properties(unit_portia_exception_handling PROPERTIES
    TIMEOUT 60
    LABELS "unit;portia;exception"
)
