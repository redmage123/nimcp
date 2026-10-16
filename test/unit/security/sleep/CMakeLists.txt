# Security Sleep Bridge Unit Tests

add_executable(unit_security_sleep_bridges
    test_security_sleep_bridges.cpp
)
target_link_libraries(unit_security_sleep_bridges
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_security_sleep_bridges
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/security/sleep
        ${CMAKE_SOURCE_DIR}/src/cognitive
        ${CMAKE_SOURCE_DIR}/src/utils/thread
)
add_test(NAME unit_security_sleep_bridges
         COMMAND unit_security_sleep_bridges)
set_tests_properties(unit_security_sleep_bridges PROPERTIES LABELS "unit")
