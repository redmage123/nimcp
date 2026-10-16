# Cognitive Fault Tolerance Integration Tests

# Emotional Tagging Integration Tests
add_executable(integration_cognitive_fault_tolerance_emotional_tagging
    test_emotional_tagging_integration.cpp
)
target_link_libraries(integration_cognitive_fault_tolerance_emotional_tagging
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(integration_cognitive_fault_tolerance_emotional_tagging
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
)
add_test(NAME integration_cognitive_fault_tolerance_emotional_tagging
         COMMAND integration_cognitive_fault_tolerance_emotional_tagging)
set_tests_properties(integration_cognitive_fault_tolerance_emotional_tagging PROPERTIES LABELS "integration")

# Health Self-Repair Integration Tests
add_executable(integration_cognitive_fault_tolerance_health_self_repair
    test_health_self_repair_integration.cpp
)
target_link_libraries(integration_cognitive_fault_tolerance_health_self_repair
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(integration_cognitive_fault_tolerance_health_self_repair
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)
add_test(NAME integration_cognitive_fault_tolerance_health_self_repair
         COMMAND integration_cognitive_fault_tolerance_health_self_repair)
set_tests_properties(integration_cognitive_fault_tolerance_health_self_repair PROPERTIES LABELS "integration")

# Health Diagnostic Bridge Integration Tests
add_executable(integration_cognitive_fault_tolerance_health_diagnostic_bridge
    test_health_diagnostic_bridge_integration.cpp
)
target_link_libraries(integration_cognitive_fault_tolerance_health_diagnostic_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(integration_cognitive_fault_tolerance_health_diagnostic_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)
add_test(NAME integration_cognitive_fault_tolerance_health_diagnostic_bridge
         COMMAND integration_cognitive_fault_tolerance_health_diagnostic_bridge)
set_tests_properties(integration_cognitive_fault_tolerance_health_diagnostic_bridge PROPERTIES LABELS "integration")
