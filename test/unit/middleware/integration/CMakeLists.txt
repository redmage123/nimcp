# Unit tests for middleware integration (Phase 1.5.5)

add_executable(unit_middleware_integration_controller
    test_middleware_controller.cpp
)

target_link_libraries(unit_middleware_integration_controller
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
    m
)

target_include_directories(unit_middleware_integration_controller PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

add_test(NAME unit_middleware_integration_controller
    COMMAND unit_middleware_integration_controller)
set_tests_properties(unit_middleware_integration_controller PROPERTIES TIMEOUT 180)

# Bio-async integration tests

add_executable(unit_middleware_controller_bio_async
    test_middleware_controller_bio_async.cpp
)

target_link_libraries(unit_middleware_controller_bio_async
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
    m
)

target_include_directories(unit_middleware_controller_bio_async PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

add_test(NAME unit_middleware_controller_bio_async
    COMMAND unit_middleware_controller_bio_async)
set_tests_properties(unit_middleware_controller_bio_async PROPERTIES TIMEOUT 180)

add_executable(unit_flow_tracker_bio_async
    test_flow_tracker_bio_async.cpp
)

target_link_libraries(unit_flow_tracker_bio_async
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
    m
)

target_include_directories(unit_flow_tracker_bio_async PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

add_test(NAME unit_flow_tracker_bio_async
    COMMAND unit_flow_tracker_bio_async)

add_executable(unit_shannon_monitor_bio_async
    test_shannon_monitor_bio_async.cpp
)

target_link_libraries(unit_shannon_monitor_bio_async
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
    m
)

target_include_directories(unit_shannon_monitor_bio_async PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

add_test(NAME unit_shannon_monitor_bio_async
    COMMAND unit_shannon_monitor_bio_async)
