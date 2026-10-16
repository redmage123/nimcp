# ============================================================================
# Neural Link Protocol Integration Tests
# ============================================================================

# Test 1: Peer Communication Integration
# Tests two-node handshake, multi-peer mesh, message round-trip, broadcast,
# and mesh relay functionality
add_test_binary(
    test_nlp_peer_communication
    ${CMAKE_CURRENT_SOURCE_DIR}/test_nlp_peer_communication.cpp
    integration
)

# Test 2: Mode Transitions Integration
# Tests operating mode transitions (Standard/Tactical/Stealth), EMCON level
# enforcement, stealth burst timing, and automatic mode switching
add_test_binary(
    test_nlp_mode_transitions
    ${CMAKE_CURRENT_SOURCE_DIR}/test_nlp_mode_transitions.cpp
    integration
)

# Test 3: Neural Synchronization Integration
# Tests spike batch sync, weight delta propagation, state sync recovery,
# and gradient push for distributed learning
add_test_binary(
    test_nlp_neural_sync
    ${CMAKE_CURRENT_SOURCE_DIR}/test_nlp_neural_sync.cpp
    integration
)

# Test 4: Disaster Scenario Integration
# Tests victim reports, sensor data aggregation, location tracking,
# hazard alerts, and emergency messaging for SAR operations
add_test_binary(
    test_nlp_disaster_scenario
    ${CMAKE_CURRENT_SOURCE_DIR}/test_nlp_disaster_scenario.cpp
    integration
)
