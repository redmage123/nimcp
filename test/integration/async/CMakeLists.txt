# Integration tests for bio-async module

# Test 1: Cross-module communication
add_executable(integration_async_test_cross_module_communication
    test_cross_module_communication.cpp
)

target_link_libraries(integration_async_test_cross_module_communication
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_async_test_cross_module_communication
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_async_test_cross_module_communication PRIVATE cxx_std_17)

gtest_discover_tests(integration_async_test_cross_module_communication
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;async;cross-module"
        TIMEOUT 180
)

# Test 2: Channel semantics
add_executable(integration_async_test_channel_semantics
    test_channel_semantics.cpp
)

target_link_libraries(integration_async_test_channel_semantics
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_async_test_channel_semantics
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_async_test_channel_semantics PRIVATE cxx_std_17)

gtest_discover_tests(integration_async_test_channel_semantics
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;async;channels"
        TIMEOUT 180
)

# Test 3: Phase synchronization
add_executable(integration_async_test_phase_synchronization
    test_phase_synchronization.cpp
)

target_link_libraries(integration_async_test_phase_synchronization
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_async_test_phase_synchronization
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_async_test_phase_synchronization PRIVATE cxx_std_17)

gtest_discover_tests(integration_async_test_phase_synchronization
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;async;phase-sync"
        TIMEOUT 180
)

# Test 4: Glial wave propagation
add_executable(integration_async_test_glial_wave_propagation
    test_glial_wave_propagation.cpp
)

target_link_libraries(integration_async_test_glial_wave_propagation
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_async_test_glial_wave_propagation
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_async_test_glial_wave_propagation PRIVATE cxx_std_17)

gtest_discover_tests(integration_async_test_glial_wave_propagation
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;async;glial"
        TIMEOUT 180
)

# Test 5: Predictive coding
add_executable(integration_async_test_predictive_coding
    test_predictive_coding.cpp
)

target_link_libraries(integration_async_test_predictive_coding
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_async_test_predictive_coding
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_async_test_predictive_coding PRIVATE cxx_std_17)

gtest_discover_tests(integration_async_test_predictive_coding
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;async;predictive"
        TIMEOUT 180
)

# Existing bio-async integration test
add_executable(integration_async_test_bio_async_integration
    test_bio_async_integration.cpp
)

target_link_libraries(integration_async_test_bio_async_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_async_test_bio_async_integration
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_async_test_bio_async_integration PRIVATE cxx_std_17)

gtest_discover_tests(integration_async_test_bio_async_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;async"
        TIMEOUT 120
)

# Test 7: Wiring diagram integration
add_executable(integration_async_test_wiring_diagram
    test_wiring_diagram_integration.cpp
)

target_link_libraries(integration_async_test_wiring_diagram
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_async_test_wiring_diagram
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_async_test_wiring_diagram PRIVATE cxx_std_17)

gtest_discover_tests(integration_async_test_wiring_diagram
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;async;wiring"
        TIMEOUT 180
)

# Test 8: KG Dispatch Integration (Phase 7: Runtime Message Orchestration)
add_executable(integration_async_test_bio_router_kg_dispatch
    test_bio_router_kg_dispatch_integration.cpp
)

target_link_libraries(integration_async_test_bio_router_kg_dispatch
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_async_test_bio_router_kg_dispatch
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_async_test_bio_router_kg_dispatch PRIVATE cxx_std_17)

gtest_discover_tests(integration_async_test_bio_router_kg_dispatch
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;async;router;kg;dispatch"
        TIMEOUT 180
)

# Test 9: Wiring Diagram KG Sync Integration (Phase 8: Wiring-KG Bidirectional Sync)
add_executable(integration_async_test_wiring_diagram_kg_sync
    test_wiring_diagram_kg_sync_integration.cpp
)

target_link_libraries(integration_async_test_wiring_diagram_kg_sync
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
        stdc++fs
)

target_include_directories(integration_async_test_wiring_diagram_kg_sync
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_async_test_wiring_diagram_kg_sync PRIVATE cxx_std_17)

gtest_discover_tests(integration_async_test_wiring_diagram_kg_sync
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;async;wiring;kg;sync"
        TIMEOUT 180
)

# Test 10: Wiring Diagram Validation Integration (Phase 9: Validation & Dependency Resolution)
add_executable(integration_async_test_wiring_diagram_validation
    test_wiring_diagram_validation_integration.cpp
)

target_link_libraries(integration_async_test_wiring_diagram_validation
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
        stdc++fs
)

target_include_directories(integration_async_test_wiring_diagram_validation
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_async_test_wiring_diagram_validation PRIVATE cxx_std_17)

gtest_discover_tests(integration_async_test_wiring_diagram_validation
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;async;wiring;validation;dependency"
        TIMEOUT 180
)

# Test 11: Self-Assembly Integration (Phase 10: Automatic Self-Assembly)
add_executable(integration_async_test_self_assembly
    test_self_assembly_integration.cpp
)

target_link_libraries(integration_async_test_self_assembly
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
        stdc++fs
)

target_include_directories(integration_async_test_self_assembly
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_async_test_self_assembly PRIVATE cxx_std_17)

gtest_discover_tests(integration_async_test_self_assembly
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;async;orchestrator;self-assembly"
        TIMEOUT 180
)
