# ============================================================================
# Security-Language Bridge Unit Tests
# ============================================================================
# WHAT: Unit tests for Security-Language Bridge (Input Sanitization & Injection Detection)
# WHY:  Verify security integration with language processing systems
# HOW:  Test lifecycle, sanitization, injection detection, content policy,
#       threat scoring, lockdown mode, bidirectional effects, and statistics
# ============================================================================

# Security-Language Bridge Unit Tests
add_executable(unit_security_test_security_language_bridge
    test_security_language_bridge.cpp
)
target_link_libraries(unit_security_test_security_language_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(unit_security_test_security_language_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/language
        ${CMAKE_SOURCE_DIR}/include/language
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(unit_security_test_security_language_bridge PRIVATE cxx_std_17)
add_test(NAME unit_security_test_security_language_bridge
         COMMAND unit_security_test_security_language_bridge)
set_tests_properties(unit_security_test_security_language_bridge PROPERTIES
    LABELS "unit;security;language;bridge"
    TIMEOUT 120
)
