# Middleware Routing Regression Tests
# Test Attention Gate Regression
add_executable(regression_middleware_routing_attention_gate
    test_attention_gate_regression.cpp
)
target_link_libraries(regression_middleware_routing_attention_gate
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME regression_middleware_routing_attention_gate
         COMMAND regression_middleware_routing_attention_gate)
# Test Routing Table Regression
add_executable(regression_middleware_routing_routing_table
    test_routing_table_regression.cpp
)
target_link_libraries(regression_middleware_routing_routing_table
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME regression_middleware_routing_routing_table
         COMMAND regression_middleware_routing_routing_table)
# Test Thalamic Router Regression
add_executable(regression_middleware_routing_thalamic_router
    test_thalamic_router_regression.cpp
)
target_link_libraries(regression_middleware_routing_thalamic_router
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME regression_middleware_routing_thalamic_router
         COMMAND regression_middleware_routing_thalamic_router)
# Test Attention Gate Error Message Regression
add_executable(regression_middleware_routing_attention_gate_error_msg
    test_attention_gate_error_msg_regression.cpp
)
target_link_libraries(regression_middleware_routing_attention_gate_error_msg
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME regression_middleware_routing_attention_gate_error_msg
         COMMAND regression_middleware_routing_attention_gate_error_msg)
