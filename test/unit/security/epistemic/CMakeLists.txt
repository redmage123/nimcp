# ============================================================================
# Security-Epistemic Bridge Unit Tests
# ============================================================================
# WHAT: Unit tests for Security-Epistemic Bridge (Belief System Protection)
# WHY:  Verify security integration with epistemic/belief systems
# HOW:  Test lifecycle, confidence validation, belief integrity verification,
#       uncertainty enforcement, evidence chain validation, and statistics
# ============================================================================

add_executable(unit_security_test_security_epistemic_bridge
    test_security_epistemic_bridge.cpp
)
target_link_libraries(unit_security_test_security_epistemic_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(unit_security_test_security_epistemic_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/epistemic
        ${CMAKE_SOURCE_DIR}/include/cognitive/epistemic
        ${CMAKE_SOURCE_DIR}/include/utils/bridge
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(unit_security_test_security_epistemic_bridge PRIVATE cxx_std_17)
add_test(NAME unit_security_test_security_epistemic_bridge
         COMMAND unit_security_test_security_epistemic_bridge)
set_tests_properties(unit_security_test_security_epistemic_bridge PROPERTIES
    LABELS "unit;security;epistemic;bridge"
    TIMEOUT 120
)
