# Training Exception Handling Integration Tests
# Tests for exception handling integration with brain immune system

# Training Exception Handling Integration Tests
add_executable(integration_training_exception
    test_training_exception_integration.cpp
)

target_include_directories(integration_training_exception PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_training_exception PRIVATE cxx_std_20)

target_link_libraries(integration_training_exception
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_training_exception
         COMMAND integration_training_exception
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_training_exception
    PROPERTIES
        LABELS "integration;training;exception;immune;gradient;recovery;checkpoint"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")
