# Cognitive Fault Tolerance Regression Tests

# Emotional Tagging Regression Tests
add_executable(regression_cognitive_fault_tolerance_emotional_tagging
    test_emotional_tagging_regression.cpp
)
target_link_libraries(regression_cognitive_fault_tolerance_emotional_tagging
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(regression_cognitive_fault_tolerance_emotional_tagging
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
)
add_test(NAME regression_cognitive_fault_tolerance_emotional_tagging
         COMMAND regression_cognitive_fault_tolerance_emotional_tagging)
set_tests_properties(regression_cognitive_fault_tolerance_emotional_tagging PROPERTIES LABELS "regression")

# Health Repair Bridge Regression Tests
add_executable(regression_cognitive_fault_tolerance_health_repair_bridge
    test_health_repair_bridge_regression.cpp
)
target_link_libraries(regression_cognitive_fault_tolerance_health_repair_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(regression_cognitive_fault_tolerance_health_repair_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)
add_test(NAME regression_cognitive_fault_tolerance_health_repair_bridge
         COMMAND regression_cognitive_fault_tolerance_health_repair_bridge)
set_tests_properties(regression_cognitive_fault_tolerance_health_repair_bridge PROPERTIES LABELS "regression")

# Health Diagnostic Bridge Regression Tests
add_executable(regression_cognitive_fault_tolerance_health_diagnostic_bridge
    test_health_diagnostic_bridge_regression.cpp
)
target_link_libraries(regression_cognitive_fault_tolerance_health_diagnostic_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(regression_cognitive_fault_tolerance_health_diagnostic_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)
add_test(NAME regression_cognitive_fault_tolerance_health_diagnostic_bridge
         COMMAND regression_cognitive_fault_tolerance_health_diagnostic_bridge)
set_tests_properties(regression_cognitive_fault_tolerance_health_diagnostic_bridge PROPERTIES LABELS "regression")
