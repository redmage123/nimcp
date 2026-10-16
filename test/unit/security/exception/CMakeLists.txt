# ============================================================================
# Security Exception Handling Unit Tests
# ============================================================================
# WHAT: Unit tests for security module exception handling
# WHY:  Verify security exceptions are properly created, dispatched, and handled
# HOW:  Test exception creation, handler registration, dispatch chain,
#       immune presentation, and recovery actions for security threats
# ============================================================================

# Security Exception Handling Unit Tests
add_executable(unit_security_test_exception_handling
    test_security_exception_handling.cpp
)
target_link_libraries(unit_security_test_exception_handling
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(unit_security_test_exception_handling
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/utils/exception
        ${CMAKE_SOURCE_DIR}/include/utils/error
        ${CMAKE_SOURCE_DIR}/include/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/utils/exception
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(unit_security_test_exception_handling PRIVATE cxx_std_17)
add_test(NAME unit_security_test_exception_handling
         COMMAND unit_security_test_exception_handling)
set_tests_properties(unit_security_test_exception_handling PROPERTIES
    LABELS "unit;security;exception"
    TIMEOUT 120
)
