# Security Orchestrator Unit Tests

add_executable(unit_security_test_security_orchestrator
    test_security_orchestrator.cpp
)

target_link_libraries(unit_security_test_security_orchestrator
    nimcp
    GTest::gtest
    GTest::gtest_main
    ${CMAKE_THREAD_LIBS_INIT}
)

target_include_directories(unit_security_test_security_orchestrator PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

add_test(
    NAME unit_security_test_security_orchestrator
    COMMAND unit_security_test_security_orchestrator
)

set_tests_properties(unit_security_test_security_orchestrator PROPERTIES
    LABELS "unit;security;orchestrator"
    TIMEOUT 60
)
