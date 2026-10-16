# Middleware Buffering Unit Tests
# Test Circular Buffer
add_executable(unit_middleware_buffering_circular_buffer
    test_circular_buffer.cpp
)
target_link_libraries(unit_middleware_buffering_circular_buffer
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_buffering_circular_buffer
         COMMAND unit_middleware_buffering_circular_buffer)
# Test Integration Buffer
add_executable(unit_middleware_buffering_integration_buffer
    test_integration_buffer.cpp
)
target_link_libraries(unit_middleware_buffering_integration_buffer
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_buffering_integration_buffer
         COMMAND unit_middleware_buffering_integration_buffer)
# Test Sliding Window
add_executable(unit_middleware_buffering_sliding_window
    test_sliding_window.cpp
)
target_link_libraries(unit_middleware_buffering_sliding_window
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_buffering_sliding_window
         COMMAND unit_middleware_buffering_sliding_window)
# Test Temporal Accumulator
add_executable(unit_middleware_buffering_temporal_accumulator
    test_temporal_accumulator.cpp
)
target_link_libraries(unit_middleware_buffering_temporal_accumulator
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_buffering_temporal_accumulator
         COMMAND unit_middleware_buffering_temporal_accumulator)
# Test Phase Coded Buffer
add_executable(unit_middleware_buffering_phase_coded_buffer
    test_phase_coded_buffer.cpp
)
target_link_libraries(unit_middleware_buffering_phase_coded_buffer
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_buffering_phase_coded_buffer
         COMMAND unit_middleware_buffering_phase_coded_buffer)

# Test Buffer Immune Integration
add_executable(unit_middleware_buffering_buffer_immune_integration
    test_buffer_immune_integration.cpp
)
target_link_libraries(unit_middleware_buffering_buffer_immune_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_buffering_buffer_immune_integration
         COMMAND unit_middleware_buffering_buffer_immune_integration)

# Test Circular Buffer Batch Operations (P1-10 fix validation)
add_executable(unit_middleware_buffering_circular_buffer_batch
    test_circular_buffer_batch.cpp
)
target_link_libraries(unit_middleware_buffering_circular_buffer_batch
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_buffering_circular_buffer_batch
         COMMAND unit_middleware_buffering_circular_buffer_batch)
