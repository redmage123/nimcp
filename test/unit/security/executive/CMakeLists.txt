# ============================================================================
# Security-Executive Bridge Unit Tests
# ============================================================================
# WHAT: Unit tests for Security-Executive Integration Bridge
# WHY:  Verify security authorization, resource limits, and audit trails
#       integrate correctly with executive control functions
# HOW:  Test lifecycle, connections, authorization, capabilities, resources,
#       deadlines, audit, rollback, and bidirectional effects
# ============================================================================

add_executable(unit_security_test_security_executive_bridge
    test_security_executive_bridge.cpp
)
target_link_libraries(unit_security_test_security_executive_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(unit_security_test_security_executive_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/executive
        ${CMAKE_SOURCE_DIR}/include/cognitive
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(unit_security_test_security_executive_bridge PRIVATE cxx_std_17)
add_test(NAME unit_security_test_security_executive_bridge
         COMMAND unit_security_test_security_executive_bridge)
set_tests_properties(unit_security_test_security_executive_bridge PROPERTIES
    LABELS "unit;security;executive;bridge"
    TIMEOUT 120
)
