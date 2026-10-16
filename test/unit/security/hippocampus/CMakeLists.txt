# ============================================================================
# Security-Hippocampus Bridge Unit Tests
# ============================================================================
# WHAT: Unit tests for Security-Hippocampus Bridge (Memory Consolidation Defense)
# WHY:  Verify security integration with hippocampus/memory consolidation systems
# HOW:  Test lifecycle, sleep-phase protection, consolidation verification,
#       memory injection detection, replay validation, and statistics
# ============================================================================

add_executable(unit_security_test_security_hippocampus_bridge
    test_security_hippocampus_bridge.cpp
)
target_link_libraries(unit_security_test_security_hippocampus_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(unit_security_test_security_hippocampus_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/hippocampus
        ${CMAKE_SOURCE_DIR}/include/cognitive/memory
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(unit_security_test_security_hippocampus_bridge PRIVATE cxx_std_17)
add_test(NAME unit_security_test_security_hippocampus_bridge
         COMMAND unit_security_test_security_hippocampus_bridge)
set_tests_properties(unit_security_test_security_hippocampus_bridge PROPERTIES
    LABELS "unit;security;hippocampus;bridge"
    TIMEOUT 120
)
