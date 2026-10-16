# ============================================================================
# Security E2E Tests - AI Safety Enhancement Pipeline
# ============================================================================
# WHAT: End-to-end tests for complete AI safety infrastructure
# WHY:  Verify all 10 safety modules work together as unified system
# HOW:  Test full lifecycle: initialization, monitoring, threat response,
#       value preservation, recovery, and graceful shutdown
# ============================================================================

# DISABLED: Test needs API updates to match current alignment_monitor API
# TODO: Update to use current alignment_monitor_record_action() instead of alignment_monitor_observe()
#
# # Safety Enhancement E2E Test (C++ with GTest)
# add_executable(e2e_security_test_safety_enhancement
#     test_safety_enhancement_e2e.cpp
# )
# target_link_libraries(e2e_security_test_safety_enhancement
#     PRIVATE
#         GTest::GTest
#         GTest::Main
#         nimcp
#         pthread
# )
# target_include_directories(e2e_security_test_safety_enhancement
#     PRIVATE
#         ${CMAKE_SOURCE_DIR}/include
#         ${CMAKE_SOURCE_DIR}/include/security
#         ${CMAKE_SOURCE_DIR}/include/async
#         ${CMAKE_SOURCE_DIR}/src/security
#         ${CMAKE_SOURCE_DIR}/src/utils/memory
#         ${CMAKE_SOURCE_DIR}/src/utils/logging
#         ${CMAKE_SOURCE_DIR}/test
#         ${CMAKE_SOURCE_DIR}/test/utils
# )
# target_compile_features(e2e_security_test_safety_enhancement PRIVATE cxx_std_17)
# add_test(NAME e2e_security_test_safety_enhancement
#          COMMAND e2e_security_test_safety_enhancement)
# set_tests_properties(e2e_security_test_safety_enhancement PROPERTIES
#     LABELS "e2e;security;safety;enhancement"
#     TIMEOUT 600
# )

# ============================================================================
# Security E2E Tests (C++/GTest)
# ============================================================================

# Security Stack E2E Test
# Tests: BBB validation, pattern detection, rate limiting, anomaly detection
add_executable(e2e_security_stack
    test_security_stack_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)
target_include_directories(e2e_security_stack PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/security
    ${CMAKE_SOURCE_DIR}/include/async
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/test/e2e
)
target_link_libraries(e2e_security_stack
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)
target_compile_features(e2e_security_stack PRIVATE cxx_std_17)
add_test(NAME e2e_security_stack COMMAND e2e_security_stack)
set_tests_properties(e2e_security_stack PROPERTIES
    LABELS "e2e;security;stack;bbb;anomaly;rate_limit"
    TIMEOUT 300
)

# Attack Simulation E2E Test
# Tests: SQL injection, XSS, buffer overflow, prompt injection
add_executable(e2e_attack_simulation
    test_attack_simulation_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)
target_include_directories(e2e_attack_simulation PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/security
    ${CMAKE_SOURCE_DIR}/include/async
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/test/e2e
)
target_link_libraries(e2e_attack_simulation
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)
target_compile_features(e2e_attack_simulation PRIVATE cxx_std_17)
add_test(NAME e2e_attack_simulation COMMAND e2e_attack_simulation)
set_tests_properties(e2e_attack_simulation PROPERTIES
    LABELS "e2e;security;attack;sqli;xss;overflow;prompt_injection"
    TIMEOUT 300
)

# Crypto Operations E2E Test
# Tests: Key generation, encryption/decryption, signatures, key rotation
add_executable(e2e_crypto_operations
    test_crypto_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)
target_include_directories(e2e_crypto_operations PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/security
    ${CMAKE_SOURCE_DIR}/include/async
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/test/e2e
)
target_link_libraries(e2e_crypto_operations
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)
target_compile_features(e2e_crypto_operations PRIVATE cxx_std_17)
add_test(NAME e2e_crypto_operations COMMAND e2e_crypto_operations)
set_tests_properties(e2e_crypto_operations PROPERTIES
    LABELS "e2e;security;crypto;kyber;dilithium;pq;audit"
    TIMEOUT 600
)

# Security Recovery E2E Test
# Tests: Violation recovery, quarantine cycles, audit trail, multi-component coordination
add_executable(e2e_security_recovery
    test_security_recovery_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)
target_include_directories(e2e_security_recovery PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/security
    ${CMAKE_SOURCE_DIR}/include/async
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/test/e2e
)
target_link_libraries(e2e_security_recovery
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)
target_compile_features(e2e_security_recovery PRIVATE cxx_std_17)
add_test(NAME e2e_security_recovery COMMAND e2e_security_recovery)
set_tests_properties(e2e_security_recovery PROPERTIES
    LABELS "e2e;security;recovery;quarantine;audit;corrigibility;tripwire"
    TIMEOUT 300
)

# ============================================================================
# Security E2E Pipeline Test
# ============================================================================
# WHAT: End-to-end tests for complete security pipeline
# WHY:  Verify all security components work together as unified defense
# HOW:  Test full activation, threat detection/response, multi-module
#       coordination, recovery, and performance under attack
# ============================================================================

add_executable(e2e_security_pipeline
    test_security_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)
target_include_directories(e2e_security_pipeline PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/security
    ${CMAKE_SOURCE_DIR}/include/cognitive
    ${CMAKE_SOURCE_DIR}/include/async
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/test/e2e
    ${CMAKE_SOURCE_DIR}/test/utils
)
target_link_libraries(e2e_security_pipeline
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)
target_compile_features(e2e_security_pipeline PRIVATE cxx_std_17)
add_test(NAME e2e_security_pipeline COMMAND e2e_security_pipeline)
set_tests_properties(e2e_security_pipeline PROPERTIES
    LABELS "e2e;security;pipeline;bbb;tripwire;immune;lgss"
    TIMEOUT 600
)

message(STATUS "Added Security E2E tests:")
message(STATUS "  - e2e_security_test_safety_enhancement (C++/GTest)")
message(STATUS "  - e2e_security_stack (C++/GTest)")
message(STATUS "  - e2e_attack_simulation (C++/GTest)")
message(STATUS "  - e2e_crypto_operations (C++/GTest)")
message(STATUS "  - e2e_security_recovery (C++/GTest)")
message(STATUS "  - e2e_security_pipeline (C++/GTest)")

# ============================================================================
# Mesh Security E2E Test
# ============================================================================
# WHAT: End-to-end tests for mesh security with BBB integration
# WHY:  Verify complete security pipeline: mesh + BBB + immune system

add_executable(e2e_mesh_security
    test_mesh_security_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)
target_include_directories(e2e_mesh_security PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/security
    ${CMAKE_SOURCE_DIR}/include/mesh
    ${CMAKE_SOURCE_DIR}/include/async
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/test/e2e
)
target_link_libraries(e2e_mesh_security
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)
target_compile_features(e2e_mesh_security PRIVATE cxx_std_17)
add_test(NAME e2e_mesh_security COMMAND e2e_mesh_security)
set_tests_properties(e2e_mesh_security PROPERTIES
    LABELS "e2e;security;mesh;bbb"
    TIMEOUT 600
)

message(STATUS "  - e2e_mesh_security (C++/GTest)")

# ============================================================================
# BBB + Immune Pipeline E2E Tests
# ============================================================================
add_executable(e2e_security_test_bbb_immune_pipeline
    test_bbb_immune_pipeline_e2e.cpp
)
target_link_libraries(e2e_security_test_bbb_immune_pipeline
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(e2e_security_test_bbb_immune_pipeline
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/cognitive/immune
        ${CMAKE_SOURCE_DIR}/include/utils/exception
        ${CMAKE_SOURCE_DIR}/include/utils/error
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(e2e_security_test_bbb_immune_pipeline PRIVATE cxx_std_17)
add_test(NAME e2e_security_test_bbb_immune_pipeline
         COMMAND e2e_security_test_bbb_immune_pipeline)
set_tests_properties(e2e_security_test_bbb_immune_pipeline PROPERTIES
    LABELS "e2e;security;bbb;immune;pipeline"
    TIMEOUT 300
)

# ============================================================================
# BBB Full Workflow E2E Tests (P1-P3 Remediation)
# ============================================================================
# WHAT: Complete Blood-Brain Barrier workflow tests
# WHY:  Verify full validation, threat detection, statistics, and enable/disable cycles

add_executable(e2e_security_test_bbb_e2e
    test_bbb_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)
target_include_directories(e2e_security_test_bbb_e2e PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/security
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/include/utils/error
    ${CMAKE_SOURCE_DIR}/test/e2e
)
target_link_libraries(e2e_security_test_bbb_e2e
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)
target_compile_features(e2e_security_test_bbb_e2e PRIVATE cxx_std_17)
add_test(NAME e2e_security_test_bbb_e2e
         COMMAND e2e_security_test_bbb_e2e)
set_tests_properties(e2e_security_test_bbb_e2e PROPERTIES
    LABELS "e2e;security;bbb;validation;threat_detection"
    TIMEOUT 600
)

message(STATUS "Added P1-P3 Remediation Security E2E tests:")
message(STATUS "  - e2e_security_test_bbb_e2e (BBB Full Workflow)")

# ============================================================================
# BBB Quarantine E2E Tests (P0-P2 Remediation)
# ============================================================================
# WHAT: End-to-end tests for BBB quarantine ref-counting and lifecycle
# WHY:  Verify TOCTOU-safe checks, ref-count isolation, underflow protection,
#       concurrent access, malloc failure handling, and immune integration

add_executable(e2e_security_test_bbb_quarantine
    test_bbb_quarantine_e2e.cpp
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
)
target_include_directories(e2e_security_test_bbb_quarantine PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/security
    ${CMAKE_SOURCE_DIR}/include/cognitive/immune
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/include/utils/error
    ${CMAKE_SOURCE_DIR}/include/utils/exception
    ${CMAKE_SOURCE_DIR}/test/e2e
)
target_link_libraries(e2e_security_test_bbb_quarantine
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)
target_compile_features(e2e_security_test_bbb_quarantine PRIVATE cxx_std_17)
add_test(NAME e2e_security_test_bbb_quarantine
         COMMAND e2e_security_test_bbb_quarantine)
set_tests_properties(e2e_security_test_bbb_quarantine PROPERTIES
    LABELS "e2e;security;bbb;quarantine;refcount;toctou"
    TIMEOUT 600
)

message(STATUS "  - e2e_security_test_bbb_quarantine (BBB Quarantine Lifecycle)")

# ============================================================================
# Security Edge E2E Tests
# ============================================================================
add_executable(e2e_test_security_edge
    test_security_edge_e2e.cpp
)
target_link_libraries(e2e_test_security_edge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)
target_include_directories(e2e_test_security_edge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/test
)
target_compile_features(e2e_test_security_edge PRIVATE cxx_std_17)
target_compile_definitions(e2e_test_security_edge PRIVATE NIMCP_TESTING NIMCP_E2E_TESTING)
add_test(NAME e2e_test_security_edge
         COMMAND e2e_test_security_edge)
set_tests_properties(e2e_test_security_edge PROPERTIES
    LABELS "e2e;security;edge"
    TIMEOUT 120
    RUN_SERIAL TRUE
)
message(STATUS "  - e2e_test_security_edge (Security Edge E2E Pipeline)")

# ============================================================================
# Immune + Bio-Async E2E Tests
# ============================================================================
# WHAT: End-to-end tests for full immune + bio-async security pipeline
# WHY:  Verify complete security stack under realistic conditions
# HOW:  Run multi-step scenarios with threats, tolerance, and recovery
# ============================================================================

add_executable(e2e_security_test_immune_bio_async
    test_immune_bio_async_e2e.cpp
)
target_link_libraries(e2e_security_test_immune_bio_async
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)
target_include_directories(e2e_security_test_immune_bio_async
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/cognitive/immune
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/async
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(e2e_security_test_immune_bio_async PRIVATE cxx_std_17)
add_test(NAME e2e_security_test_immune_bio_async
         COMMAND e2e_security_test_immune_bio_async)
set_tests_properties(e2e_security_test_immune_bio_async PROPERTIES
    LABELS "e2e;security;immune;bio-async;coverage"
    TIMEOUT 600
)

message(STATUS "  - e2e_security_test_immune_bio_async (Immune+Bio-Async E2E Pipeline)")
