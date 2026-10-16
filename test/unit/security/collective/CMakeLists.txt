# ============================================================================
# Security-Collective Cognition Bridge Unit Tests
# ============================================================================
# WHAT: Unit tests for Security-Collective Cognition Integration Bridge
# WHY:  Verify Byzantine detection, consensus verification, swarm monitoring,
#       emergent pattern validation, and trust scoring work correctly
# HOW:  Test lifecycle, connections, detection, verification, monitoring,
#       pattern validation, trust scoring, and bidirectional effects
# ============================================================================

add_executable(unit_security_test_security_collective_bridge
    test_security_collective_bridge.cpp
)
target_link_libraries(unit_security_test_security_collective_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(unit_security_test_security_collective_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/collective
        ${CMAKE_SOURCE_DIR}/include/cognitive
        ${CMAKE_SOURCE_DIR}/include/cognitive/collective_cognition
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/include/utils
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(unit_security_test_security_collective_bridge PRIVATE cxx_std_17)
add_test(NAME unit_security_test_security_collective_bridge
         COMMAND unit_security_test_security_collective_bridge)
set_tests_properties(unit_security_test_security_collective_bridge PROPERTIES
    LABELS "unit;security;collective;bridge"
    TIMEOUT 120
)
