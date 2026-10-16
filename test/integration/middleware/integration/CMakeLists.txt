# Integration tests for middleware controller (Phase 1.5.5)

add_executable(integration_middleware_controller
    test_middleware_controller_integration.cpp
)

target_link_libraries(integration_middleware_controller
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
    m
)

target_include_directories(integration_middleware_controller PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

add_test(NAME integration_middleware_controller
    COMMAND integration_middleware_controller)

# Bio-async middleware integration tests

add_executable(integration_middleware_bio_async
    test_middleware_bio_async_integration.cpp
)

target_link_libraries(integration_middleware_bio_async
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
    m
)

target_include_directories(integration_middleware_bio_async PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

add_test(NAME integration_middleware_bio_async
    COMMAND integration_middleware_bio_async)

set_tests_properties(integration_middleware_bio_async PROPERTIES
    TIMEOUT 120)
