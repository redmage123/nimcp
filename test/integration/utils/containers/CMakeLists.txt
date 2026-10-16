# Utils Containers Integration Tests
# Test queue types in realistic multi-threaded scenarios

# Queue Integration Tests
add_executable(integration_utils_containers_test_queue_integration
    test_queue_integration.cpp
)
target_link_libraries(integration_utils_containers_test_queue_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)
add_test(NAME integration_utils_containers_test_queue_integration
         COMMAND integration_utils_containers_test_queue_integration)
set_tests_properties(integration_utils_containers_test_queue_integration
                     PROPERTIES TIMEOUT 300)
