# Chemistry pH Integration Tests CMakeLists.txt

# pH integration test
add_executable(integration_chemistry_ph_test_ph_integration
    test_ph_integration.cpp
)

target_include_directories(integration_chemistry_ph_test_ph_integration PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(integration_chemistry_ph_test_ph_integration
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME integration_chemistry_ph_test_ph_integration
    COMMAND integration_chemistry_ph_test_ph_integration)
