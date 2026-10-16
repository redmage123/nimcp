# Integration tests for middleware buffering modules

# Bio-async integration tests
add_executable(test_buffering_bio_async_integration
    test_buffering_bio_async_integration.cpp
)

target_link_libraries(test_buffering_bio_async_integration
    PRIVATE
        nimcp_circular_buffer
        nimcp_phase_coded_buffer
        nimcp_sliding_window
        nimcp_temporal_accumulator
        nimcp_integration_buffer
        nimcp_bio_async
        nimcp_bio_router
        nimcp_logging
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(test_buffering_bio_async_integration
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

add_test(
    NAME BufferingBioAsyncIntegration
    COMMAND test_buffering_bio_async_integration
)

# Set test properties
set_tests_properties(BufferingBioAsyncIntegration PROPERTIES
    LABELS "integration;middleware;buffering;bio-async"
    TIMEOUT 60
)
