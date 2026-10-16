# ============================================================================
# Security-Imagination Bridge Unit Tests
# ============================================================================
# WHAT: Unit tests for Security-Imagination Bridge (Hypothetical Reasoning Defense)
# WHY:  Verify security integration with imagination/simulation systems
# HOW:  Test lifecycle, workspace sandboxing, confabulation detection, reasoning
#       bounds, reality grounding, bidirectional effects, and statistics
# ============================================================================

add_executable(unit_security_test_security_imagination_bridge
    test_security_imagination_bridge.cpp
)
target_link_libraries(unit_security_test_security_imagination_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(unit_security_test_security_imagination_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/imagination
        ${CMAKE_SOURCE_DIR}/include/cognitive/imagination
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(unit_security_test_security_imagination_bridge PRIVATE cxx_std_17)
add_test(NAME unit_security_test_security_imagination_bridge
         COMMAND unit_security_test_security_imagination_bridge)
set_tests_properties(unit_security_test_security_imagination_bridge PROPERTIES
    LABELS "unit;security;imagination;bridge"
    TIMEOUT 120
)
