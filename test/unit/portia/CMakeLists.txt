# Unit tests for Portia spider system

# Tier Switch Tests
add_executable(test_portia_tier_switch
    test_portia_tier_switch.cpp
)

target_link_libraries(test_portia_tier_switch
    nimcp_portia
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
)

add_test(NAME PortiaTierSwitch COMMAND test_portia_tier_switch)
set_tests_properties(PortiaTierSwitch PROPERTIES
    TIMEOUT 60
    LABELS "unit;portia;tier_switch"
)

# Power Management Tests
add_executable(test_portia_power
    test_portia_power.cpp
)

target_link_libraries(test_portia_power
    nimcp_portia
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
)

add_test(NAME PortiaPower COMMAND test_portia_power)
set_tests_properties(PortiaPower PROPERTIES
    TIMEOUT 60
    LABELS "unit;portia;power"
)

# Planning Tests
add_executable(test_portia_planning
    test_portia_planning.cpp
)

target_link_libraries(test_portia_planning
    nimcp_portia
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
)

add_test(NAME PortiaPlanning COMMAND test_portia_planning)
set_tests_properties(PortiaPlanning PROPERTIES
    TIMEOUT 60
    LABELS "unit;portia;planning"
)

# Attention Tests
add_executable(test_portia_attention
    test_portia_attention.cpp
)

target_link_libraries(test_portia_attention
    nimcp_portia
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
)

add_test(NAME PortiaAttention COMMAND test_portia_attention)
set_tests_properties(PortiaAttention PROPERTIES
    TIMEOUT 60
    LABELS "unit;portia;attention"
)

# Accelerator Tests
add_executable(test_accelerator
    test_accelerator.cpp
)

target_link_libraries(test_accelerator
    nimcp_portia
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
)

add_test(NAME PortiaAccelerator COMMAND test_accelerator)
set_tests_properties(PortiaAccelerator PROPERTIES
    TIMEOUT 60
    LABELS "unit;portia;accelerator"
)

# Learning Tests
add_executable(test_portia_learning
    test_portia_learning.cpp
)

target_link_libraries(test_portia_learning
    nimcp_portia
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
)

add_test(NAME PortiaLearning COMMAND test_portia_learning)
set_tests_properties(PortiaLearning PROPERTIES
    TIMEOUT 60
    LABELS "unit;portia;learning"
)

# Sensor Fusion Tests
add_executable(test_portia_sensor_fusion
    test_portia_sensor_fusion.cpp
)

target_link_libraries(test_portia_sensor_fusion
    nimcp_portia
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
)

add_test(NAME PortiaSensorFusion COMMAND test_portia_sensor_fusion)
set_tests_properties(PortiaSensorFusion PROPERTIES
    TIMEOUT 60
    LABELS "unit;portia;sensor_fusion"
)

# Degradation Tests
add_executable(test_portia_degradation
    test_portia_degradation.cpp
)

target_link_libraries(test_portia_degradation
    nimcp_portia
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
)

add_test(NAME PortiaDegradation COMMAND test_portia_degradation)
set_tests_properties(PortiaDegradation PROPERTIES
    TIMEOUT 60
    LABELS "unit;portia;degradation"
)

# Classification Tests
add_executable(test_portia_classification
    test_portia_classification.cpp
)

target_link_libraries(test_portia_classification
    nimcp_portia
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
)

add_test(NAME PortiaClassification COMMAND test_portia_classification)
set_tests_properties(PortiaClassification PROPERTIES
    TIMEOUT 60
    LABELS "unit;portia;classification"
)

# Deception Tests
add_executable(test_portia_deception
    test_portia_deception.cpp
)

target_link_libraries(test_portia_deception
    nimcp_portia
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
)

add_test(NAME PortiaDeception COMMAND test_portia_deception)
set_tests_properties(PortiaDeception PROPERTIES
    TIMEOUT 60
    LABELS "unit;portia;deception"
)

# Swarm Bridge Tests
add_executable(test_portia_swarm_bridge
    test_portia_swarm_bridge.cpp
)

target_link_libraries(test_portia_swarm_bridge
    nimcp_portia
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
    m
)

add_test(NAME PortiaSwarmBridge COMMAND test_portia_swarm_bridge)
set_tests_properties(PortiaSwarmBridge PROPERTIES
    TIMEOUT 60
    LABELS "unit;portia;swarm_bridge"
)

# Portia-Swarm-Logic Bridge Tests
add_executable(test_portia_swarm_logic_bridge
    test_portia_swarm_logic_bridge.cpp
)

target_link_libraries(test_portia_swarm_logic_bridge
    nimcp_portia
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
    m
)

add_test(NAME PortiaSwarmLogicBridge COMMAND test_portia_swarm_logic_bridge)
set_tests_properties(PortiaSwarmLogicBridge PROPERTIES
    TIMEOUT 120
    LABELS "unit;portia;swarm_logic_bridge;unified"
)

# Portia-Logic Bridge Tests
add_executable(test_portia_logic_bridge
    test_portia_logic_bridge.cpp
)

target_link_libraries(test_portia_logic_bridge
    nimcp_portia
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
    m
)

add_test(NAME PortiaLogicBridge COMMAND test_portia_logic_bridge)
set_tests_properties(PortiaLogicBridge PROPERTIES
    TIMEOUT 120
    LABELS "unit;portia;logic_bridge"
)

# ============================================================================
# Portia Monitoring Tests
# ============================================================================
# DISABLED: portia_monitor_* functions not yet implemented
message(STATUS "test_portia_monitoring: DISABLED (portia monitoring not implemented)")
#[[
add_executable(test_portia_monitoring
    test_portia_monitoring.cpp
)

target_link_libraries(test_portia_monitoring
    nimcp_portia
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
    m
)

add_test(NAME PortiaMonitoring COMMAND test_portia_monitoring)
set_tests_properties(PortiaMonitoring PROPERTIES
    TIMEOUT 60
    LABELS "unit;portia;monitoring"
)
#]]

# Portia-Collective Bridge Tests
add_executable(test_portia_collective_bridge
    test_portia_collective_bridge.cpp
)

target_link_libraries(test_portia_collective_bridge
    nimcp_portia
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
    m
)

add_test(NAME PortiaCollectiveBridge COMMAND test_portia_collective_bridge)
set_tests_properties(PortiaCollectiveBridge PROPERTIES
    TIMEOUT 60
    LABELS "unit;portia;collective_bridge"
)

# ============================================================================
# Portia-Immune Bridge Tests
# ============================================================================
add_subdirectory(immune)

# Exception handling tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/exception/CMakeLists.txt")
    add_subdirectory(exception)
endif()
