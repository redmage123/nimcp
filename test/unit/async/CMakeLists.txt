# Unit tests for bio-async module

# Bio-async unit tests
add_executable(unit_async_test_bio_async
    test_bio_async.cpp
)

target_link_libraries(unit_async_test_bio_async
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m  # math library
)

target_include_directories(unit_async_test_bio_async
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

# Enable C++17 for chrono
target_compile_features(unit_async_test_bio_async PRIVATE cxx_std_17)

# Register test
gtest_discover_tests(unit_async_test_bio_async
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;async"
        TIMEOUT 60
)

# Bio-router unit tests
add_executable(unit_async_test_bio_router
    test_bio_router.cpp
)

target_link_libraries(unit_async_test_bio_router
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_async_test_bio_router
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_async_test_bio_router PRIVATE cxx_std_17)

gtest_discover_tests(unit_async_test_bio_router
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;async;router"
        TIMEOUT 120
)

# Bio-async immune integration unit tests
add_executable(unit_async_immune_integration
    unit_async_immune_integration.cpp
)

target_link_libraries(unit_async_immune_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_async_immune_integration
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_async_immune_integration PRIVATE cxx_std_17)

gtest_discover_tests(unit_async_immune_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;async;immune;integration"
        TIMEOUT 120
)

# ============================================================================
# FEP Bridge Unit Tests
# ============================================================================

# Bio-async FEP bridge tests
add_executable(unit_async_bio_async_fep_bridge
    test_bio_async_fep_bridge.cpp
)
target_link_libraries(unit_async_bio_async_fep_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_include_directories(unit_async_bio_async_fep_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_async_bio_async_fep_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_async_bio_async_fep_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;async;fep;bridge"
        TIMEOUT 60
)

# Semantic compression FEP bridge tests
add_executable(unit_async_semantic_compression_fep_bridge
    test_semantic_compression_fep_bridge.cpp
)
target_link_libraries(unit_async_semantic_compression_fep_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_include_directories(unit_async_semantic_compression_fep_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_async_semantic_compression_fep_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_async_semantic_compression_fep_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;async;fep;bridge;compression"
        TIMEOUT 60
)

# Bio-router FEP bridge tests
add_executable(unit_async_bio_router_fep_bridge
    test_bio_router_fep_bridge.cpp
)
target_link_libraries(unit_async_bio_router_fep_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_include_directories(unit_async_bio_router_fep_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_async_bio_router_fep_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_async_bio_router_fep_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;async;fep;bridge;router"
        TIMEOUT 60
)

# Biological timescales FEP bridge tests
add_executable(unit_async_biological_timescales_fep_bridge
    test_biological_timescales_fep_bridge.cpp
)
target_link_libraries(unit_async_biological_timescales_fep_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_include_directories(unit_async_biological_timescales_fep_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_async_biological_timescales_fep_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_async_biological_timescales_fep_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;async;fep;bridge;timescales"
        TIMEOUT 60
)

# Predictive protocol FEP bridge tests
add_executable(unit_async_predictive_protocol_fep_bridge
    test_predictive_protocol_fep_bridge.cpp
)
target_link_libraries(unit_async_predictive_protocol_fep_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_include_directories(unit_async_predictive_protocol_fep_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_async_predictive_protocol_fep_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_async_predictive_protocol_fep_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;async;fep;bridge;protocol"
        TIMEOUT 60
)

# ============================================================================
# Bio-Async Orchestrator Unit Tests
# ============================================================================

# Bio-async orchestrator tests
add_executable(unit_async_bio_async_orchestrator
    test_bio_async_orchestrator.cpp
)
target_link_libraries(unit_async_bio_async_orchestrator
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_include_directories(unit_async_bio_async_orchestrator
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_async_bio_async_orchestrator PRIVATE cxx_std_17)
gtest_discover_tests(unit_async_bio_async_orchestrator
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;async;orchestrator"
        TIMEOUT 120
)

# ============================================================================
# Wiring Diagram Unit Tests
# ============================================================================

# Wiring diagram tests
add_executable(unit_async_wiring_diagram
    test_wiring_diagram.cpp
)
target_link_libraries(unit_async_wiring_diagram
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
        stdc++fs  # Filesystem library for C++17
)
target_include_directories(unit_async_wiring_diagram
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_async_wiring_diagram PRIVATE cxx_std_17)
gtest_discover_tests(unit_async_wiring_diagram
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;async;wiring"
        TIMEOUT 120
)

# ============================================================================
# KG Dispatch Unit Tests (Phase 7: Runtime Message Orchestration)
# ============================================================================

add_executable(unit_async_bio_router_kg_dispatch
    test_bio_router_kg_dispatch.cpp
)
target_link_libraries(unit_async_bio_router_kg_dispatch
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_include_directories(unit_async_bio_router_kg_dispatch
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_async_bio_router_kg_dispatch PRIVATE cxx_std_17)
gtest_discover_tests(unit_async_bio_router_kg_dispatch
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;async;router;kg;dispatch"
        TIMEOUT 120
)

# ============================================================================
# Wiring Diagram KG Sync Unit Tests (Phase 8: Wiring-KG Bidirectional Sync)
# ============================================================================

add_executable(unit_async_wiring_diagram_kg_sync
    test_wiring_diagram_kg_sync.cpp
)
target_link_libraries(unit_async_wiring_diagram_kg_sync
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
        stdc++fs
)
target_include_directories(unit_async_wiring_diagram_kg_sync
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_async_wiring_diagram_kg_sync PRIVATE cxx_std_17)
gtest_discover_tests(unit_async_wiring_diagram_kg_sync
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;async;wiring;kg;sync"
        TIMEOUT 120
)

# ============================================================================
# Wiring Diagram Validation Unit Tests (Phase 9: Validation & Dependency Resolution)
# ============================================================================

add_executable(unit_async_wiring_diagram_validation
    test_wiring_diagram_validation.cpp
)
target_link_libraries(unit_async_wiring_diagram_validation
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
        stdc++fs
)
target_include_directories(unit_async_wiring_diagram_validation
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_async_wiring_diagram_validation PRIVATE cxx_std_17)
gtest_discover_tests(unit_async_wiring_diagram_validation
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;async;wiring;validation;dependency"
        TIMEOUT 120
)

# ============================================================================
# Self-Assembly Unit Tests (Phase 10: Automatic Self-Assembly)
# ============================================================================

add_executable(unit_async_self_assembly
    test_self_assembly.cpp
)
target_link_libraries(unit_async_self_assembly
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
        stdc++fs
)
target_include_directories(unit_async_self_assembly
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_async_self_assembly PRIVATE cxx_std_17)
gtest_discover_tests(unit_async_self_assembly
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;async;orchestrator;self-assembly"
        TIMEOUT 120
)

# ============================================================================
# Plasticity Bio-Async Bridge Unit Tests
# ============================================================================
# Tests for plasticity-bio-async coordination bridge

add_executable(unit_async_plasticity_bridge
    test_bio_async_plasticity_bridge.cpp
)
target_link_libraries(unit_async_plasticity_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_include_directories(unit_async_plasticity_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_async_plasticity_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_async_plasticity_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;async;plasticity;bridge"
        TIMEOUT 120
)

# ============================================================================
# Bio-Router Registration Unit Tests (P1-12: Re-registration memory leak)
# ============================================================================

add_executable(unit_async_bio_router_registration
    test_bio_router_registration.cpp
)
target_link_libraries(unit_async_bio_router_registration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_include_directories(unit_async_bio_router_registration
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_async_bio_router_registration PRIVATE cxx_std_17)
gtest_discover_tests(unit_async_bio_router_registration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;async;router;registration"
        TIMEOUT 60
)

# ============================================================================
# Bio-Router Thread Safety Unit Tests (TOCTOU race condition fixes)
# ============================================================================

add_executable(unit_async_bio_router_thread_safety
    test_bio_router_thread_safety.cpp
)
target_link_libraries(unit_async_bio_router_thread_safety
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_include_directories(unit_async_bio_router_thread_safety
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_async_bio_router_thread_safety PRIVATE cxx_std_17)
gtest_discover_tests(unit_async_bio_router_thread_safety
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;async;router;thread-safety;toctou"
        TIMEOUT 120
        RESOURCE_LOCK "brain_heavy"
)

# ============================================================================
# Async Exception Macros Unit Tests - DISABLED (API issues)
# Tests moved to existing exception test suite
# ============================================================================

# ============================================================================
# Bio-Async Coverage Unit Tests
# ============================================================================
# WHAT: Unit tests for bio-async coverage improvements
# WHY:  Verify broadcast error handling, stats, NULL guards, message validation
# HOW:  Initialize router, register modules, test error paths directly
# ============================================================================

add_executable(unit_async_test_bio_async_coverage
    test_bio_async_coverage.cpp
)
target_link_libraries(unit_async_test_bio_async_coverage
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_include_directories(unit_async_test_bio_async_coverage
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_async_test_bio_async_coverage PRIVATE cxx_std_17)
gtest_discover_tests(unit_async_test_bio_async_coverage
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;async;coverage"
        TIMEOUT 120
)
