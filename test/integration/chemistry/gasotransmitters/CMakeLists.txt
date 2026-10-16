# Chemistry Gasotransmitters Integration Tests CMakeLists.txt

# NO plasticity integration test
add_executable(integration_chemistry_gasotransmitters_test_no_plasticity_integration
    test_no_plasticity_integration.cpp
)

target_include_directories(integration_chemistry_gasotransmitters_test_no_plasticity_integration PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(integration_chemistry_gasotransmitters_test_no_plasticity_integration
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME integration_chemistry_gasotransmitters_test_no_plasticity_integration
    COMMAND integration_chemistry_gasotransmitters_test_no_plasticity_integration)
