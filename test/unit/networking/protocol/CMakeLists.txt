# ============================================================================
# Protocol Control Message Unit Tests
# ============================================================================

# Test: Protocol Control Message Functions
add_test_binary(
    test_protocol_control_message
    ${CMAKE_CURRENT_SOURCE_DIR}/test_protocol_control_message.cpp
    unit
)

# Test: Protocol Event Packet Functions
add_test_binary(
    test_protocol_event_packet
    ${CMAKE_CURRENT_SOURCE_DIR}/test_protocol_event_packet.cpp
    unit
)

# Test: Protocol Feature Code and Subscription Functions
add_test_binary(
    test_protocol_feature_subscription
    ${CMAKE_CURRENT_SOURCE_DIR}/test_protocol_feature_subscription.cpp
    unit
)

# ============================================================================
# Collective Cognition Protocol Tests
# ============================================================================

# Test: Message Framing
add_test_binary(
    test_nimcp_msg_framing
    ${CMAKE_CURRENT_SOURCE_DIR}/test_nimcp_msg_framing.cpp
    unit
)

# Test: Message Router
add_test_binary(
    test_nimcp_msg_router
    ${CMAKE_CURRENT_SOURCE_DIR}/test_nimcp_msg_router.cpp
    unit
)
