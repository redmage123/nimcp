# ============================================================================
# Security-Game Theory Bridge Unit Tests
# ============================================================================
# WHAT: Unit tests for Security-Game Theory Integration Bridge
# WHY:  Verify payoff validation, coalition monitoring, mechanism verification,
#       equilibrium checking, and manipulation detection integrate correctly
# HOW:  Test lifecycle, connections, validation, monitoring, verification,
#       detection, and bidirectional effects
# ============================================================================

add_executable(unit_security_test_security_game_theory_bridge
    test_security_game_theory_bridge.cpp
)
target_link_libraries(unit_security_test_security_game_theory_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(unit_security_test_security_game_theory_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/game_theory
        ${CMAKE_SOURCE_DIR}/include/cognitive
        ${CMAKE_SOURCE_DIR}/include/cognitive/game_theory
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(unit_security_test_security_game_theory_bridge PRIVATE cxx_std_17)
add_test(NAME unit_security_test_security_game_theory_bridge
         COMMAND unit_security_test_security_game_theory_bridge)
set_tests_properties(unit_security_test_security_game_theory_bridge PROPERTIES
    LABELS "unit;security;game_theory;bridge"
    TIMEOUT 120
)
