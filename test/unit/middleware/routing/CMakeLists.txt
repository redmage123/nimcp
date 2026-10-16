# Middleware Routing Unit Tests
# Test Attention Gate
add_executable(unit_middleware_routing_attention_gate
    test_attention_gate.cpp
)
target_link_libraries(unit_middleware_routing_attention_gate
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_routing_attention_gate
         COMMAND unit_middleware_routing_attention_gate)
# Test Routing Table
add_executable(unit_middleware_routing_routing_table
    test_routing_table.cpp
)
target_link_libraries(unit_middleware_routing_routing_table
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_routing_routing_table
         COMMAND unit_middleware_routing_routing_table)
# Test Thalamic Router
add_executable(unit_middleware_routing_thalamic_router
    test_thalamic_router.cpp
)
target_link_libraries(unit_middleware_routing_thalamic_router
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_routing_thalamic_router
         COMMAND unit_middleware_routing_thalamic_router)
# Test Signal Wrapper
add_executable(unit_middleware_routing_signal_wrapper
    test_signal_wrapper.cpp
)
target_link_libraries(unit_middleware_routing_signal_wrapper
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_routing_signal_wrapper
         COMMAND unit_middleware_routing_signal_wrapper)
# Test Thalamic-Immune Integration
add_executable(unit_middleware_routing_thalamic_immune_integration
    test_thalamic_immune_integration.cpp
)
target_link_libraries(unit_middleware_routing_thalamic_immune_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_routing_thalamic_immune_integration
         COMMAND unit_middleware_routing_thalamic_immune_integration)
# Test Attention Gate Error Messages
add_executable(unit_middleware_routing_attention_gate_error_messages
    test_attention_gate_error_messages.cpp
)
target_link_libraries(unit_middleware_routing_attention_gate_error_messages
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_routing_attention_gate_error_messages
         COMMAND unit_middleware_routing_attention_gate_error_messages)
