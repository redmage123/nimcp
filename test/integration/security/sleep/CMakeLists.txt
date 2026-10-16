# Security Sleep Bridge Integration Tests

add_executable(integration_security_sleep_bridges
    test_security_sleep_integration.cpp
)
target_link_libraries(integration_security_sleep_bridges
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(integration_security_sleep_bridges
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/security/sleep
        ${CMAKE_SOURCE_DIR}/src/cognitive
        ${CMAKE_SOURCE_DIR}/src/utils/thread
)
add_test(NAME integration_security_sleep_bridges
         COMMAND integration_security_sleep_bridges)
set_tests_properties(integration_security_sleep_bridges PROPERTIES LABELS "integration")
