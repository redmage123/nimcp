# ============================================================================
# Security-RCOG Bridge Unit Tests
# ============================================================================
# WHAT: Unit tests for Security-RCOG bidirectional bridge
# WHY:  Verify security module integrates correctly with recursive cognition
# HOW:  Test lifecycle, whitelist, validation, sandboxing, approval, lockdown
# ============================================================================

add_executable(unit_security_rcog_bridge
    test_security_rcog_bridge.cpp
)
target_link_libraries(unit_security_rcog_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(unit_security_rcog_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/rcog
        ${CMAKE_SOURCE_DIR}/include/cognitive/recursive
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(unit_security_rcog_bridge PRIVATE cxx_std_17)
add_test(NAME unit_security_rcog_bridge
         COMMAND unit_security_rcog_bridge)
set_tests_properties(unit_security_rcog_bridge PROPERTIES
    LABELS "unit;security;rcog;bridge"
    TIMEOUT 120
)
