# Cognitive Bio-Async Integration Tests
# Integration tests for cognitive module bio-async message handling

add_executable(integration_cognitive_bio_async_test
    test_cognitive_bio_async_integration.cpp
)

target_link_libraries(integration_cognitive_bio_async_test
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)

target_include_directories(integration_cognitive_bio_async_test
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)

gtest_discover_tests(integration_cognitive_bio_async_test)
