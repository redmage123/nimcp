# Middleware Middleware Integration Tests
# Test Middleware Subsystems Integration
add_executable(integration_middleware_middleware_test_middleware_subsystems_integration
    test_middleware_subsystems_integration.cpp
)
target_link_libraries(integration_middleware_middleware_test_middleware_subsystems_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME integration_middleware_middleware_test_middleware_subsystems_integration
         COMMAND integration_middleware_middleware_test_middleware_subsystems_integration)

# Pipeline Integration Tests
add_executable(integration_middleware_pipeline_integration
    pipeline/test_pipeline_integration.cpp
)
target_link_libraries(integration_middleware_pipeline_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME integration_middleware_pipeline_integration
         COMMAND integration_middleware_pipeline_integration)

# Phase 2 Integration Tests
# TODO: Fix outdated API usage in test_phase2_integration.cpp
# - Uses old spike_record_t instead of spike_train_t
# - Uses old network_config_t and feature_extractor API
# Temporarily disabled until Phase 3 implementation is complete
#add_executable(integration_middleware_middleware_test_phase2_integration
#    test_phase2_integration.cpp
#)
#target_link_libraries(integration_middleware_middleware_test_phase2_integration
#    PRIVATE
#        GTest::GTest
#        GTest::Main
#        nimcp_middleware
#        nimcp
#    )
#add_test(NAME integration_middleware_middleware_test_phase2_integration
#         COMMAND integration_middleware_middleware_test_phase2_integration)

# Brain Integration Integration Tests
add_executable(integration_middleware_test_brain_integration_integration
    test_brain_integration_integration.cpp
)
target_link_libraries(integration_middleware_test_brain_integration_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME integration_middleware_test_brain_integration_integration
         COMMAND integration_middleware_test_brain_integration_integration)

# Cognitive Adapters Integration Tests - DISABLED: Uses deprecated event subscription API
#add_executable(integration_middleware_test_cognitive_adapters_integration
#    test_cognitive_adapters_integration.cpp
#)
#target_link_libraries(integration_middleware_test_cognitive_adapters_integration
#    PRIVATE
#        GTest::GTest
#        GTest::Main
#        nimcp_middleware
#        nimcp
#    )
#add_test(NAME integration_middleware_test_cognitive_adapters_integration
#         COMMAND integration_middleware_test_cognitive_adapters_integration)

# Training Adapters Integration Tests - DISABLED: Uses deprecated event subscription API
#add_executable(integration_middleware_test_training_adapters_integration
#    test_training_adapters_integration.cpp
#)
#target_link_libraries(integration_middleware_test_training_adapters_integration
#    PRIVATE
#        GTest::GTest
#        GTest::Main
#        nimcp_middleware
#        nimcp
#    )
#add_test(NAME integration_middleware_test_training_adapters_integration
#         COMMAND integration_middleware_test_training_adapters_integration)

# Brain Middleware E2E Tests - DISABLED: NORMALIZE enum conflicts with deprecated API
#add_executable(integration_middleware_test_brain_middleware_e2e
#    test_brain_middleware_e2e.cpp
#)
#target_link_libraries(integration_middleware_test_brain_middleware_e2e
#    PRIVATE
#        GTest::GTest
#        GTest::Main
#        nimcp_middleware
#        nimcp
#    )
#add_test(NAME integration_middleware_test_brain_middleware_e2e
#         COMMAND integration_middleware_test_brain_middleware_e2e)

# Shannon + Flow Integration Tests (Phase 1.5.1)
add_executable(integration_middleware_test_shannon_flow
    test_shannon_flow_integration.cpp
)
target_link_libraries(integration_middleware_test_shannon_flow
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)
add_test(NAME integration_middleware_test_shannon_flow
         COMMAND integration_middleware_test_shannon_flow)
set_tests_properties(integration_middleware_test_shannon_flow PROPERTIES TIMEOUT 120)

# Executive Integration Tests (Phase 1.5.2)
add_executable(integration_middleware_test_executive_integration
    test_executive_integration.cpp
)
target_link_libraries(integration_middleware_test_executive_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        nimcp_middleware
        pthread
        m
)
add_test(NAME integration_middleware_test_executive_integration
         COMMAND integration_middleware_test_executive_integration)
set_tests_properties(integration_middleware_test_executive_integration PROPERTIES TIMEOUT 120)

# Complex PAC Detection Integration Tests
add_executable(integration_middleware_test_complex_pac_detection
    test_complex_pac_detection.cpp
)
target_link_libraries(integration_middleware_test_complex_pac_detection
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        nimcp_middleware
        pthread
        m
)
add_test(NAME integration_middleware_test_complex_pac_detection
         COMMAND integration_middleware_test_complex_pac_detection)
set_tests_properties(integration_middleware_test_complex_pac_detection PROPERTIES TIMEOUT 180)

# Signal Filter Integration Tests
add_executable(integration_middleware_test_signal_filter_integration
    test_signal_filter_integration.cpp
)
target_link_libraries(integration_middleware_test_signal_filter_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        nimcp_middleware
        pthread
        m
)
add_test(NAME integration_middleware_test_signal_filter_integration
         COMMAND integration_middleware_test_signal_filter_integration)
set_tests_properties(integration_middleware_test_signal_filter_integration PROPERTIES TIMEOUT 120)

# Phase 1.5.5 Middleware Controller Integration Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration")
    add_subdirectory(integration)
endif()

# Training Module Integration Tests (Phase TM-1: Security + Memory Pool Integration)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/training")
    add_subdirectory(training)
endif()
