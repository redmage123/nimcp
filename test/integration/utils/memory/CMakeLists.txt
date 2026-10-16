# Memory Utils Integration Tests
# Phase 2: Brain Substrate Pools Integration

add_executable(integration_utils_memory_test_brain_pools_integration
    test_brain_pools_integration.cpp
)
target_link_libraries(integration_utils_memory_test_brain_pools_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)
add_test(NAME integration_utils_memory_test_brain_pools_integration
         COMMAND integration_utils_memory_test_brain_pools_integration)

# Phase 3 Layer Pools Integration Tests
add_executable(integration_utils_memory_test_layer_pools_integration
    test_layer_pools_integration.cpp
)
target_link_libraries(integration_utils_memory_test_layer_pools_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)
add_test(NAME integration_utils_memory_test_layer_pools_integration
         COMMAND integration_utils_memory_test_layer_pools_integration)

# Unified Memory Integration Tests
add_executable(integration_utils_memory_test_unified_memory_integration
    test_unified_memory_integration.cpp
)
target_link_libraries(integration_utils_memory_test_unified_memory_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)
add_test(NAME integration_utils_memory_test_unified_memory_integration
         COMMAND integration_utils_memory_test_unified_memory_integration)
