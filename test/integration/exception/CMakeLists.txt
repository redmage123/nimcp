# =============================================================================
# Exception Integration Tests
# =============================================================================

# Signal Exception Integration Tests
add_executable(integration_exception_test_signal_exception_integration
    test_signal_exception_integration.cpp
)

target_link_libraries(integration_exception_test_signal_exception_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        pthread
        m
)

target_compile_features(integration_exception_test_signal_exception_integration PRIVATE cxx_std_17)

add_test(
    NAME integration_exception_test_signal_exception_integration
    COMMAND integration_exception_test_signal_exception_integration
)

set_tests_properties(integration_exception_test_signal_exception_integration
    PROPERTIES
    LABELS "integration;exception;signal"
    TIMEOUT 120
)

# =============================================================================
# Exception Recovery Integration Tests
# =============================================================================

add_executable(integration_exception_test_exception_recovery_integration
    test_exception_recovery_integration.cpp
)

target_link_libraries(integration_exception_test_exception_recovery_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(integration_exception_test_exception_recovery_integration PRIVATE cxx_std_17)

add_test(
    NAME integration_exception_test_exception_recovery_integration
    COMMAND integration_exception_test_exception_recovery_integration
)

set_tests_properties(integration_exception_test_exception_recovery_integration
    PROPERTIES
    LABELS "integration;exception;recovery"
    TIMEOUT 120
)

# =============================================================================
# Exception Handler Chain Integration Tests
# =============================================================================

add_executable(integration_exception_test_handler_chain_integration
    test_exception_handler_chain_integration.cpp
)

target_link_libraries(integration_exception_test_handler_chain_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(integration_exception_test_handler_chain_integration PRIVATE cxx_std_17)

add_test(
    NAME integration_exception_test_handler_chain_integration
    COMMAND integration_exception_test_handler_chain_integration
)

set_tests_properties(integration_exception_test_handler_chain_integration
    PROPERTIES
    LABELS "integration;exception;handler_chain"
    TIMEOUT 120
)

# =============================================================================
# Exception Propagation Integration Tests
# =============================================================================

add_executable(integration_exception_test_exception_propagation_integration
    test_exception_propagation_integration.cpp
)

target_link_libraries(integration_exception_test_exception_propagation_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(integration_exception_test_exception_propagation_integration PRIVATE cxx_std_17)

add_test(
    NAME integration_exception_test_exception_propagation_integration
    COMMAND integration_exception_test_exception_propagation_integration
)

set_tests_properties(integration_exception_test_exception_propagation_integration
    PROPERTIES
    LABELS "integration;exception;propagation"
    TIMEOUT 120
)

# =============================================================================
# Exception-Immune Integration Tests
# =============================================================================

add_executable(integration_exception_test_exception_immune_integration
    test_exception_immune_integration.cpp
)

target_link_libraries(integration_exception_test_exception_immune_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(integration_exception_test_exception_immune_integration PRIVATE cxx_std_17)

add_test(
    NAME integration_exception_test_exception_immune_integration
    COMMAND integration_exception_test_exception_immune_integration
)

set_tests_properties(integration_exception_test_exception_immune_integration
    PROPERTIES
    LABELS "integration;exception;immune"
    TIMEOUT 120
)

# =============================================================================
# Exception Module Integration Tests
# =============================================================================

add_executable(integration_exception_test_exception_module_integration
    test_exception_module_integration.cpp
)

target_link_libraries(integration_exception_test_exception_module_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(integration_exception_test_exception_module_integration PRIVATE cxx_std_17)

add_test(
    NAME integration_exception_test_exception_module_integration
    COMMAND integration_exception_test_exception_module_integration
)

set_tests_properties(integration_exception_test_exception_module_integration
    PROPERTIES
    LABELS "integration;exception;module"
    TIMEOUT 180
)

# =============================================================================
# Exception Async Integration Tests
# =============================================================================

add_executable(integration_exception_test_exception_async_integration
    test_exception_async_integration.cpp
)

target_link_libraries(integration_exception_test_exception_async_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(integration_exception_test_exception_async_integration PRIVATE cxx_std_17)

add_test(
    NAME integration_exception_test_exception_async_integration
    COMMAND integration_exception_test_exception_async_integration
)

set_tests_properties(integration_exception_test_exception_async_integration
    PROPERTIES
    LABELS "integration;exception;async"
    TIMEOUT 180
)

# =============================================================================
# Exception Cognitive Integration Tests
# =============================================================================

add_executable(integration_exception_test_exception_cognitive_integration
    test_exception_cognitive_integration.cpp
)

target_link_libraries(integration_exception_test_exception_cognitive_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(integration_exception_test_exception_cognitive_integration PRIVATE cxx_std_17)

add_test(
    NAME integration_exception_test_exception_cognitive_integration
    COMMAND integration_exception_test_exception_cognitive_integration
)

set_tests_properties(integration_exception_test_exception_cognitive_integration
    PROPERTIES
    LABELS "integration;exception;cognitive"
    TIMEOUT 180
)

# =============================================================================
# Exception Bio-Async Flow Integration Tests
# =============================================================================

add_executable(integration_exception_test_exception_bio_async_flow
    test_exception_bio_async_flow.cpp
)

target_link_libraries(integration_exception_test_exception_bio_async_flow
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(integration_exception_test_exception_bio_async_flow PRIVATE cxx_std_17)

add_test(
    NAME integration_exception_test_exception_bio_async_flow
    COMMAND integration_exception_test_exception_bio_async_flow
)

set_tests_properties(integration_exception_test_exception_bio_async_flow
    PROPERTIES
    LABELS "integration;exception;bio_async;router"
    TIMEOUT 180
)

# =============================================================================
# Exception Health Integration Tests
# =============================================================================

add_executable(integration_exception_test_exception_health_integration
    test_exception_health_integration.cpp
)

target_link_libraries(integration_exception_test_exception_health_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(integration_exception_test_exception_health_integration PRIVATE cxx_std_17)

add_test(
    NAME integration_exception_test_exception_health_integration
    COMMAND integration_exception_test_exception_health_integration
)

set_tests_properties(integration_exception_test_exception_health_integration
    PROPERTIES
    LABELS "integration;exception;health;self_repair"
    TIMEOUT 180
)

# =============================================================================
# Exception Cross-Module Integration Tests
# =============================================================================

add_executable(integration_exception_test_exception_cross_module
    test_exception_cross_module.cpp
)

target_link_libraries(integration_exception_test_exception_cross_module
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(integration_exception_test_exception_cross_module PRIVATE cxx_std_17)

add_test(
    NAME integration_exception_test_exception_cross_module
    COMMAND integration_exception_test_exception_cross_module
)

set_tests_properties(integration_exception_test_exception_cross_module
    PROPERTIES
    LABELS "integration;exception;cross_module"
    TIMEOUT 180
)

# =============================================================================
# Exception Cross-Module Flow Integration Tests
# =============================================================================

add_executable(integration_exception_test_exception_cross_module_flow
    test_exception_cross_module_flow.cpp
)

target_link_libraries(integration_exception_test_exception_cross_module_flow
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(integration_exception_test_exception_cross_module_flow PRIVATE cxx_std_17)

add_test(
    NAME integration_exception_test_exception_cross_module_flow
    COMMAND integration_exception_test_exception_cross_module_flow
)

set_tests_properties(integration_exception_test_exception_cross_module_flow
    PROPERTIES
    LABELS "integration;exception;cross_module;flow;swarm;brain;training;perception;memory;bio_async"
    TIMEOUT 300
)

# =============================================================================
# Exception Flow Comprehensive Integration Tests
# =============================================================================

add_executable(integration_exception_test_exception_flow_comprehensive
    test_exception_flow_comprehensive.cpp
)

target_link_libraries(integration_exception_test_exception_flow_comprehensive
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(integration_exception_test_exception_flow_comprehensive PRIVATE cxx_std_17)

add_test(
    NAME integration_exception_test_exception_flow_comprehensive
    COMMAND integration_exception_test_exception_flow_comprehensive
)

set_tests_properties(integration_exception_test_exception_flow_comprehensive
    PROPERTIES
    LABELS "integration;exception;flow;comprehensive;propagation;recovery;cleanup;callback;cross_module"
    TIMEOUT 300
)
