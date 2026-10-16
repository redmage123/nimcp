# ============================================================================
# NIMCP Core Exception Unit Tests
# ============================================================================
#
# WHAT: Unit tests for exception handling system with core module integration
# WHY:  Verify exception handling, dispatch, and recovery for core modules
# HOW:  Test exception creation, handler chain, immune integration, and cleanup
#
# ============================================================================

# Unit test for core exception handling
add_executable(unit_core_exception_test_handling
    test_core_exception_handling.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/exception/nimcp_exception.c
    ${CMAKE_SOURCE_DIR}/src/utils/exception/nimcp_exception_handlers.c
    ${CMAKE_SOURCE_DIR}/src/utils/exception/nimcp_exception_immune.c
)

target_link_libraries(unit_core_exception_test_handling
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)

target_compile_features(unit_core_exception_test_handling PRIVATE cxx_std_17)

target_include_directories(unit_core_exception_test_handling
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

# Enable test-only APIs
target_compile_definitions(unit_core_exception_test_handling PRIVATE NIMCP_TESTING)

add_test(NAME unit_core_exception_test_handling
         COMMAND unit_core_exception_test_handling)

set_tests_properties(unit_core_exception_test_handling
    PROPERTIES
        TIMEOUT 120
        LABELS "unit;core;exception"
)
