# ============================================================================
# Security-Continual Learning Bridge Unit Tests
# ============================================================================
# WHAT: Unit tests for Security-Continual Learning Bridge (Knowledge Protection)
# WHY:  Verify security integration with continual/lifelong learning systems
# HOW:  Test lifecycle, catastrophic forgetting protection, concept drift validation,
#       memory replay integrity, learning rate manipulation detection, and statistics
# ============================================================================

add_executable(unit_security_test_security_continual_learning_bridge
    test_security_continual_learning_bridge.cpp
)
target_link_libraries(unit_security_test_security_continual_learning_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(unit_security_test_security_continual_learning_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/continual
        ${CMAKE_SOURCE_DIR}/include/training
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(unit_security_test_security_continual_learning_bridge PRIVATE cxx_std_17)
add_test(NAME unit_security_test_security_continual_learning_bridge
         COMMAND unit_security_test_security_continual_learning_bridge)
set_tests_properties(unit_security_test_security_continual_learning_bridge PROPERTIES
    LABELS "unit;security;continual;bridge"
    TIMEOUT 120
)
