# ============================================================================
# Security-Training Bridge Unit Tests
# ============================================================================
# WHAT: Unit tests for Security-Training Integration Bridge
# WHY:  Verify data poisoning protection, gradient sanitization, model integrity
# HOW:  Test lifecycle, validation, poisoning detection, and bidirectional effects
# ============================================================================

add_executable(unit_security_test_security_training_bridge
    test_security_training_bridge.cpp
)
target_link_libraries(unit_security_test_security_training_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(unit_security_test_security_training_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/training
        ${CMAKE_SOURCE_DIR}/include/training
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(unit_security_test_security_training_bridge PRIVATE cxx_std_17)
add_test(NAME unit_security_test_security_training_bridge
         COMMAND unit_security_test_security_training_bridge)
set_tests_properties(unit_security_test_security_training_bridge PROPERTIES
    LABELS "unit;security;training;bridge"
    TIMEOUT 120
)
