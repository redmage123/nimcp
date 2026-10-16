# Middleware Events Regression Tests
# Test Event Queue Regression
add_executable(regression_middleware_events_event_queue
    test_event_queue_regression.cpp
)
target_link_libraries(regression_middleware_events_event_queue
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME regression_middleware_events_event_queue
         COMMAND regression_middleware_events_event_queue)
# Test Event Subscriber Regression
add_executable(regression_middleware_events_event_subscriber
    test_event_subscriber_regression.cpp
)
target_link_libraries(regression_middleware_events_event_subscriber
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME regression_middleware_events_event_subscriber
         COMMAND regression_middleware_events_event_subscriber)
# Test Event Types Regression
add_executable(regression_middleware_events_event_types
    test_event_types_regression.cpp
)
target_link_libraries(regression_middleware_events_event_types
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME regression_middleware_events_event_types
         COMMAND regression_middleware_events_event_types)
