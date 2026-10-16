# Training Exception Handling Unit Tests
# Tests for exception handling in training subsystem components

# Training Exception Handling Unit Tests
add_executable(unit_training_exception_handling
    test_training_exception_handling.cpp
)

target_include_directories(unit_training_exception_handling PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_training_exception_handling PRIVATE cxx_std_20)

target_link_libraries(unit_training_exception_handling
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_training_exception_handling
         COMMAND unit_training_exception_handling
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_training_exception_handling
    PROPERTIES
        LABELS "unit;training;exception;gradient;nan;inf;checkpoint;rollback"
        TIMEOUT 180
        RESOURCE_LOCK "brain_heavy")
