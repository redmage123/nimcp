# ============================================================================
# Security-Perception Input Bridge Unit Tests
# ============================================================================
# WHAT: Unit tests for Security-Perception Input Bridge (Audio/Visual Validation)
# WHY:  Verify security integration with perception input systems
# HOW:  Test lifecycle, audio/visual validation, anomaly detection, input gating,
#       threat scoring, lockdown mode, bidirectional effects, and statistics
# ============================================================================

# Security-Perception Input Bridge Unit Tests
add_executable(unit_security_test_security_perception_input_bridge
    test_security_perception_input_bridge.cpp
)
target_link_libraries(unit_security_test_security_perception_input_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(unit_security_test_security_perception_input_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/perception
        ${CMAKE_SOURCE_DIR}/include/perception
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(unit_security_test_security_perception_input_bridge PRIVATE cxx_std_17)
add_test(NAME unit_security_test_security_perception_input_bridge
         COMMAND unit_security_test_security_perception_input_bridge)
set_tests_properties(unit_security_test_security_perception_input_bridge PROPERTIES
    LABELS "unit;security;perception;bridge"
    TIMEOUT 120
)
