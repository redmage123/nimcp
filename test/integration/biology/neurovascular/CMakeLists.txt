# Biology Neurovascular Integration Tests CMakeLists.txt

# Neurovascular integration test
add_executable(integration_biology_neurovascular_test_neurovascular_integration
    test_neurovascular_integration.cpp
)

target_include_directories(integration_biology_neurovascular_test_neurovascular_integration PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(integration_biology_neurovascular_test_neurovascular_integration
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME integration_biology_neurovascular_test_neurovascular_integration
    COMMAND integration_biology_neurovascular_test_neurovascular_integration)
