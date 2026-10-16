# ============================================================================
# Security Integration Tests - Blood-Brain Barrier (Phase IS-1)
# ============================================================================
# WHAT: Integration tests for Neural Immune System with NIMCP brain
# WHY:  Verify BBB integration with brain subsystems and cross-layer operation
# HOW:  Test multi-layer defense, brain coordination, and full system workflows
# ============================================================================

# Blood-Brain Barrier Integration Tests (16 tests)
add_executable(integration_security_test_bbb_integration
    test_bbb_integration.cpp
)
target_link_libraries(integration_security_test_bbb_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(integration_security_test_bbb_integration
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME integration_security_test_bbb_integration
         COMMAND integration_security_test_bbb_integration)
set_tests_properties(integration_security_test_bbb_integration PROPERTIES
    LABELS "integration"
    TIMEOUT 120
)

# ============================================================================
# Mathematical Security Integration Tests (Phase SC-3)
# ============================================================================
# WHAT: Integration tests for Entropy, Bayesian Trust, and Differential Privacy
# WHY:  Verify cross-component workflows and concurrent access patterns
# HOW:  Test full security pipeline with all three mathematical mechanisms
# ============================================================================

# Mathematical Security Integration Tests (15 tests)
add_executable(integration_security_test_math_integration
    test_security_math_integration.cpp
)
target_link_libraries(integration_security_test_math_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(integration_security_test_math_integration
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME integration_security_test_math_integration
         COMMAND integration_security_test_math_integration)
set_tests_properties(integration_security_test_math_integration PROPERTIES
    LABELS "integration"
    TIMEOUT 180
)

# ============================================================================
# Security Integration Framework Tests (Phase SC-4)
# ============================================================================
# WHAT: Integration tests for Universal Security Integration Framework
# WHY:  Verify cross-module security workflows and concurrent operations
# HOW:  Test multi-module registration, trust propagation, and full pipeline
# ============================================================================

# Security Integration Framework Integration Tests (13 tests)
add_executable(integration_security_test_framework
    test_security_integration_framework.cpp
)
target_link_libraries(integration_security_test_framework
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(integration_security_test_framework
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME integration_security_test_framework
         COMMAND integration_security_test_framework)
set_tests_properties(integration_security_test_framework PROPERTIES
    LABELS "integration;security;sc4"
    TIMEOUT 300
)

# ============================================================================
# BBB Bio-Router Integration Tests
# ============================================================================
# WHAT: Integration tests for BBB security validation through bio-router
# WHY:  Verify security checks are properly applied to all bio-async messages
# HOW:  Test BBB validation at message boundaries and cross-module security
# ============================================================================

# BBB Bio-Router Integration Tests (9 tests)
add_executable(integration_security_test_bbb_bio_router
    test_bbb_bio_router_integration.cpp
)
target_link_libraries(integration_security_test_bbb_bio_router
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(integration_security_test_bbb_bio_router
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/async
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME integration_security_test_bbb_bio_router
         COMMAND integration_security_test_bbb_bio_router)
set_tests_properties(integration_security_test_bbb_bio_router PROPERTIES
    LABELS "integration;security;bbb;bio-router"
    TIMEOUT 180
)

# ============================================================================
# Security State Management Integration Tests
# ============================================================================
# TEMPORARILY DISABLED - Needs API migration to bio_router
# TODO: Update test_security_level_integration.cpp to use bio_router API
# ============================================================================

# add_executable(integration_security_test_security_level
#     test_security_level_integration.cpp
# )
# target_link_libraries(integration_security_test_security_level
#     PRIVATE
#         GTest::GTest
#         GTest::Main
#         nimcp
#         pthread
# )
# target_include_directories(integration_security_test_security_level
#     PRIVATE
#         ${CMAKE_SOURCE_DIR}/include
#         ${CMAKE_SOURCE_DIR}/include/security
#         ${CMAKE_SOURCE_DIR}/src/security
#         ${CMAKE_SOURCE_DIR}/src/utils/memory
#         ${CMAKE_SOURCE_DIR}/src/utils/logging
#         ${CMAKE_SOURCE_DIR}/test
#         ${CMAKE_SOURCE_DIR}/test/utils
# )
# add_test(NAME integration_security_test_security_level
#          COMMAND integration_security_test_security_level)
# set_tests_properties(integration_security_test_security_level PROPERTIES
#     LABELS "integration;security"
#     TIMEOUT 180
# )

# ============================================================================
# Distributed Security Consensus Integration Tests
# ============================================================================
# TEMPORARILY DISABLED - Needs API migration
# TODO: Update test_security_consensus_integration.cpp
# ============================================================================

# add_executable(integration_security_test_security_consensus
#     test_security_consensus_integration.cpp
# )
# target_link_libraries(integration_security_test_security_consensus
#     PRIVATE
#         GTest::GTest
#         GTest::Main
#         nimcp
#         pthread
# )
# target_include_directories(integration_security_test_security_consensus
#     PRIVATE
#         ${CMAKE_SOURCE_DIR}/include
#         ${CMAKE_SOURCE_DIR}/include/security
#         ${CMAKE_SOURCE_DIR}/src/security
#         ${CMAKE_SOURCE_DIR}/src/utils/memory
#         ${CMAKE_SOURCE_DIR}/src/utils/logging
#         ${CMAKE_SOURCE_DIR}/test
#         ${CMAKE_SOURCE_DIR}/test/utils
# )
# add_test(NAME integration_security_test_security_consensus
#          COMMAND integration_security_test_security_consensus)
# set_tests_properties(integration_security_test_security_consensus PROPERTIES
#     LABELS "integration;security"
#     TIMEOUT 300
# )

# ============================================================================
# Enhanced Security Framework Integration Tests (Phase SC-5)
# ============================================================================

# Constant-Time Crypto Integration Tests
add_executable(integration_security_test_constant_time test_constant_time_integration.cpp)
target_link_libraries(integration_security_test_constant_time PRIVATE GTest::GTest GTest::Main nimcp pthread)
target_include_directories(integration_security_test_constant_time PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
add_test(NAME integration_security_test_constant_time COMMAND integration_security_test_constant_time)
set_tests_properties(integration_security_test_constant_time PROPERTIES LABELS "integration;security" TIMEOUT 120)

# Key Derivation Integration Tests
add_executable(integration_security_test_key_derivation test_key_derivation_integration.cpp)
target_link_libraries(integration_security_test_key_derivation PRIVATE GTest::GTest GTest::Main nimcp pthread)
target_include_directories(integration_security_test_key_derivation PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
add_test(NAME integration_security_test_key_derivation COMMAND integration_security_test_key_derivation)
set_tests_properties(integration_security_test_key_derivation PROPERTIES LABELS "integration;security" TIMEOUT 180)

# Path Traversal Integration Tests
# TEMPORARILY DISABLED - Needs bio_async API migration
# add_executable(integration_security_test_path_traversal test_path_traversal_integration.cpp)
# target_link_libraries(integration_security_test_path_traversal PRIVATE GTest::GTest GTest::Main nimcp pthread)
# target_include_directories(integration_security_test_path_traversal PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
# add_test(NAME integration_security_test_path_traversal COMMAND integration_security_test_path_traversal)
# set_tests_properties(integration_security_test_path_traversal PROPERTIES LABELS "integration;security" TIMEOUT 120)

# Shell Detector Integration Tests
# TEMPORARILY DISABLED - Needs bio_async API migration
# add_executable(integration_security_test_shell_detector test_shell_detector_integration.cpp)
# target_link_libraries(integration_security_test_shell_detector PRIVATE GTest::GTest GTest::Main nimcp pthread)
# target_include_directories(integration_security_test_shell_detector PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
# add_test(NAME integration_security_test_shell_detector COMMAND integration_security_test_shell_detector)
# set_tests_properties(integration_security_test_shell_detector PROPERTIES LABELS "integration;security" TIMEOUT 120)

# Anomaly Detector Integration Tests
add_executable(integration_security_test_anomaly_detector test_anomaly_detector_integration.cpp)
target_link_libraries(integration_security_test_anomaly_detector PRIVATE GTest::GTest GTest::Main nimcp pthread)
target_include_directories(integration_security_test_anomaly_detector PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
add_test(NAME integration_security_test_anomaly_detector COMMAND integration_security_test_anomaly_detector)
set_tests_properties(integration_security_test_anomaly_detector PROPERTIES LABELS "integration;security" TIMEOUT 180)

# Pattern Database Integration Tests
add_executable(integration_security_test_pattern_db test_pattern_db_integration.cpp)
target_link_libraries(integration_security_test_pattern_db PRIVATE GTest::GTest GTest::Main nimcp pthread)
target_include_directories(integration_security_test_pattern_db PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
add_test(NAME integration_security_test_pattern_db COMMAND integration_security_test_pattern_db)
set_tests_properties(integration_security_test_pattern_db PROPERTIES LABELS "integration;security" TIMEOUT 120)

# Encrypted Audit Integration Tests
add_executable(integration_security_test_encrypted_audit test_encrypted_audit_integration.cpp)
target_link_libraries(integration_security_test_encrypted_audit PRIVATE GTest::GTest GTest::Main nimcp pthread)
target_include_directories(integration_security_test_encrypted_audit PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
add_test(NAME integration_security_test_encrypted_audit COMMAND integration_security_test_encrypted_audit)
set_tests_properties(integration_security_test_encrypted_audit PROPERTIES LABELS "integration;security" TIMEOUT 120)

# Policy Engine Integration Tests
# TEMPORARILY DISABLED - Needs bio_async API migration
# add_executable(integration_security_test_policy_engine test_policy_engine_integration.cpp)
# target_link_libraries(integration_security_test_policy_engine PRIVATE GTest::GTest GTest::Main nimcp pthread)
# target_include_directories(integration_security_test_policy_engine PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
# add_test(NAME integration_security_test_policy_engine COMMAND integration_security_test_policy_engine)
# set_tests_properties(integration_security_test_policy_engine PROPERTIES LABELS "integration;security" TIMEOUT 180)

# Post-Quantum Integration Tests
# TEMPORARILY DISABLED - Needs include path fix
# add_executable(integration_security_test_post_quantum test_post_quantum_integration.cpp)
# target_link_libraries(integration_security_test_post_quantum PRIVATE GTest::GTest GTest::Main nimcp pthread)
# target_include_directories(integration_security_test_post_quantum PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
# add_test(NAME integration_security_test_post_quantum COMMAND integration_security_test_post_quantum)
# set_tests_properties(integration_security_test_post_quantum PROPERTIES LABELS "integration;security" TIMEOUT 180)

# Supply Chain Integration Tests
# TEMPORARILY DISABLED - Needs API fixes
# add_executable(integration_security_test_supply_chain test_supply_chain_integration.cpp)
# target_link_libraries(integration_security_test_supply_chain PRIVATE GTest::GTest GTest::Main nimcp pthread)
# target_include_directories(integration_security_test_supply_chain PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
# add_test(NAME integration_security_test_supply_chain COMMAND integration_security_test_supply_chain)
# set_tests_properties(integration_security_test_supply_chain PROPERTIES LABELS "integration;security" TIMEOUT 120)

# ============================================================================
# Swarm Security Integration Tests
# ============================================================================
# WHAT: Integration tests for BBB security in swarm communications
# WHY:  Verify end-to-end security across all swarm modules
# HOW:  Test complete attack scenarios and security workflows
# ============================================================================

# Swarm Security Integration Tests
add_executable(integration_security_test_swarm_security
    test_swarm_security_integration.cpp
)
target_link_libraries(integration_security_test_swarm_security
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(integration_security_test_swarm_security
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/swarm
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/swarm
        ${CMAKE_SOURCE_DIR}/src/async
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME integration_security_test_swarm_security
         COMMAND integration_security_test_swarm_security)
set_tests_properties(integration_security_test_swarm_security PROPERTIES
    LABELS "integration;security;swarm"
    TIMEOUT 300
)

# ============================================================================
# Security-Logging Bridge Integration Tests
# ============================================================================
# WHAT: Integration tests for Security-Logging Bridge
# WHY:  Verify bidirectional data flow between security and logging systems
# HOW:  Test event flow, pattern detection, audit integration, and feedback loops
# ============================================================================

# Security-Logging Bridge Integration Tests (28 tests)
add_executable(integration_security_test_security_logging
    test_security_logging_integration.cpp
)
target_link_libraries(integration_security_test_security_logging
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(integration_security_test_security_logging
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/logging
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/include/utils/logging
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/async
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME integration_security_test_security_logging
         COMMAND integration_security_test_security_logging)
set_tests_properties(integration_security_test_security_logging PROPERTIES
    LABELS "integration;security;logging"
    TIMEOUT 300
)

# ============================================================================
# Security-Async Bridge Integration Tests
# ============================================================================
# WHAT: Integration tests for Security-Async Bridge (bidirectional)
# WHY:  Verify security event coordination across bio-async system
# HOW:  Test broadcasts, threat intel sharing, emergency mode, concurrent ops
# ============================================================================

# Security-Async Bridge Integration Tests (26 tests)
add_executable(integration_security_test_security_async
    test_security_async_integration.cpp
)
target_link_libraries(integration_security_test_security_async
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(integration_security_test_security_async
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/async
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/async
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME integration_security_test_security_async
         COMMAND integration_security_test_security_async)
set_tests_properties(integration_security_test_security_async PROPERTIES
    LABELS "integration;security;async;bio-router"
    TIMEOUT 300
)

# ============================================================================
# Security-Immune Unified Bridge Integration Tests
# ============================================================================
# WHAT: Integration tests for Security-Immune Unified Bridge
# WHY:  Verify bidirectional coordination between all security components
#       (BBB, anomaly detector, pattern DB, rate limiter, policy engine)
#       and the brain immune system through the unified bridge.
# HOW:  Test threat-to-immune response, immune modulation of security,
#       tolerance system, memory cell formation, and full bidirectional flows
# ============================================================================

# Security-Immune Unified Bridge Integration Tests (24 tests)
add_executable(integration_security_test_security_immune
    test_security_immune_integration.cpp
)
target_link_libraries(integration_security_test_security_immune
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(integration_security_test_security_immune
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/immune
        ${CMAKE_SOURCE_DIR}/include/cognitive/immune
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/async
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME integration_security_test_security_immune
         COMMAND integration_security_test_security_immune)
set_tests_properties(integration_security_test_security_immune PROPERTIES
    LABELS "integration;security;immune"
    TIMEOUT 300
)

# ============================================================================
# Security Sleep Bridge Integration Tests
# ============================================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sleep/CMakeLists.txt")
    add_subdirectory(sleep)
endif()

# ============================================================================
# Security Module Integration Tests (Phase SMB)
# ============================================================================
# WHAT: Integration tests for Security Module Bridges
# WHY:  Verify cross-module security workflows and threat coordination
# HOW:  Test complete security pipelines across perception, language, executive,
#       memory, training, and rcog modules
# ============================================================================

add_executable(integration_security_test_module_integration
    test_security_module_integration.cpp
)
target_link_libraries(integration_security_test_module_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(integration_security_test_module_integration
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/perception
        ${CMAKE_SOURCE_DIR}/include/security/language
        ${CMAKE_SOURCE_DIR}/include/security/executive
        ${CMAKE_SOURCE_DIR}/include/security/memory
        ${CMAKE_SOURCE_DIR}/include/security/training
        ${CMAKE_SOURCE_DIR}/include/security/rcog
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME integration_security_test_module_integration
         COMMAND integration_security_test_module_integration)
set_tests_properties(integration_security_test_module_integration PROPERTIES
    LABELS "integration;security;module;perception;language;executive;memory;training;rcog"
    TIMEOUT 300
)

# ============================================================================
# Security-Knowledge Graph Bridge Integration Tests
# ============================================================================
# WHAT: Integration tests for Security-Knowledge Graph Bridge
# WHY:  Verify knowledge graph protection across multi-query scenarios
# HOW:  Test batch queries, access control, privacy isolation, threat response
# ============================================================================

add_executable(integration_security_test_knowledge_graph
    test_security_knowledge_graph_integration.cpp
)
target_link_libraries(integration_security_test_knowledge_graph
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(integration_security_test_knowledge_graph
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/knowledge
        ${CMAKE_SOURCE_DIR}/include/cognitive/knowledge
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(integration_security_test_knowledge_graph PRIVATE cxx_std_17)
add_test(NAME integration_security_test_knowledge_graph
         COMMAND integration_security_test_knowledge_graph)
set_tests_properties(integration_security_test_knowledge_graph PROPERTIES
    LABELS "integration;security;knowledge;bridge"
    TIMEOUT 300
)

# ============================================================================
# Security-Collective Cognition Bridge Integration Tests
# ============================================================================
# WHAT: Integration tests for Security-Collective Cognition Bridge
# WHY:  Verify security integration with collective cognition systems
# HOW:  Test Byzantine detection, consensus verification, swarm monitoring,
#       emergent pattern validation, trust scoring, and bidirectional effects
# ============================================================================

add_executable(integration_security_test_collective
    test_security_collective_integration.cpp
)
target_link_libraries(integration_security_test_collective
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(integration_security_test_collective
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/collective
        ${CMAKE_SOURCE_DIR}/include/cognitive/collective_cognition
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/include/utils
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(integration_security_test_collective PRIVATE cxx_std_17)
add_test(NAME integration_security_test_collective
         COMMAND integration_security_test_collective)
set_tests_properties(integration_security_test_collective PROPERTIES
    LABELS "integration;security;collective;bridge"
    TIMEOUT 300
)

# ============================================================================
# Security-Game Theory Bridge Integration Tests
# ============================================================================
# WHAT: Integration tests for Security-Game Theory Bridge
# WHY:  Verify game-theoretic security integration across multi-player scenarios
# HOW:  Test payoff validation, coalition monitoring, mechanism verification,
#       equilibrium checking, manipulation detection, and bidirectional effects
# ============================================================================

add_executable(integration_security_test_game_theory
    test_security_game_theory_integration.cpp
)
target_link_libraries(integration_security_test_game_theory
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(integration_security_test_game_theory
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/game_theory
        ${CMAKE_SOURCE_DIR}/include/cognitive/game_theory
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(integration_security_test_game_theory PRIVATE cxx_std_17)
add_test(NAME integration_security_test_game_theory
         COMMAND integration_security_test_game_theory)
set_tests_properties(integration_security_test_game_theory PROPERTIES
    LABELS "integration;security;game_theory;bridge"
    TIMEOUT 300
)

# ============================================================================
# Security-Imagination Bridge Integration Tests
# ============================================================================
# WHAT: Integration tests for Security-Imagination Bridge
# WHY:  Verify imagination security integration across multi-simulation scenarios
# HOW:  Test sandbox management, confabulation detection, reasoning bounds,
#       reality grounding, simulation integrity, and bidirectional effects
# ============================================================================

add_executable(integration_security_test_imagination
    test_security_imagination_integration.cpp
)
target_link_libraries(integration_security_test_imagination
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(integration_security_test_imagination
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/imagination
        ${CMAKE_SOURCE_DIR}/include/cognitive/imagination
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(integration_security_test_imagination PRIVATE cxx_std_17)
add_test(NAME integration_security_test_imagination
         COMMAND integration_security_test_imagination)
set_tests_properties(integration_security_test_imagination PROPERTIES
    LABELS "integration;security;imagination;bridge"
    TIMEOUT 300
)

# ============================================================================
# Security-Continual Learning Bridge Integration Tests
# ============================================================================
# WHAT: Integration tests for Security-Continual Learning Bridge
# WHY:  Verify security integration with always-on learning systems
# HOW:  Test forgetting protection, drift validation, replay integrity,
#       retention monitoring, LR manipulation detection, and bidirectional effects
# ============================================================================

add_executable(integration_security_test_continual_learning
    test_security_continual_learning_integration.cpp
)
target_link_libraries(integration_security_test_continual_learning
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(integration_security_test_continual_learning
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/continual
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(integration_security_test_continual_learning PRIVATE cxx_std_17)
add_test(NAME integration_security_test_continual_learning
         COMMAND integration_security_test_continual_learning)
set_tests_properties(integration_security_test_continual_learning PROPERTIES
    LABELS "integration;security;continual_learning;bridge"
    TIMEOUT 300
)

# ============================================================================
# Security-Epistemic Bridge Integration Tests
# ============================================================================
# WHAT: Integration tests for Security-Epistemic Bridge
# WHY:  Verify security integration with epistemic reasoning systems
# HOW:  Test confidence validation, belief verification, uncertainty enforcement,
#       evidence chain validation, attack detection, and bidirectional effects
# ============================================================================

add_executable(integration_security_test_epistemic
    test_security_epistemic_integration.cpp
)
target_link_libraries(integration_security_test_epistemic
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(integration_security_test_epistemic
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/epistemic
        ${CMAKE_SOURCE_DIR}/include/cognitive/epistemic
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/include/utils/bridge
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(integration_security_test_epistemic PRIVATE cxx_std_17)
add_test(NAME integration_security_test_epistemic
         COMMAND integration_security_test_epistemic)
set_tests_properties(integration_security_test_epistemic PROPERTIES
    LABELS "integration;security;epistemic;bridge"
    TIMEOUT 300
)

# ============================================================================
# Security-Distributed Training Bridge Integration Tests
# ============================================================================
# WHAT: Integration tests for Security-Distributed Training Bridge
# WHY:  Verify distributed/federated training protection across multi-worker scenarios
# HOW:  Test Byzantine fault tolerance, gradient validation, worker trust scoring,
#       secure checkpointing, threat escalation, and bidirectional effects
# ============================================================================

add_executable(integration_security_test_distributed_training
    test_security_distributed_training_integration.cpp
)
target_link_libraries(integration_security_test_distributed_training
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(integration_security_test_distributed_training
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/distributed
        ${CMAKE_SOURCE_DIR}/include/training
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(integration_security_test_distributed_training PRIVATE cxx_std_17)
add_test(NAME integration_security_test_distributed_training
         COMMAND integration_security_test_distributed_training)
set_tests_properties(integration_security_test_distributed_training PROPERTIES
    LABELS "integration;security;distributed;training;bridge"
    TIMEOUT 300
)

# ============================================================================
# Security-Hippocampus Bridge Integration Tests
# ============================================================================
# WHAT: Integration tests for Security-Hippocampus Bridge (Memory Consolidation Defense)
# WHY:  Verify security integration with hippocampus/memory consolidation systems
# HOW:  Test sleep-phase protection workflows, consolidation verification,
#       memory injection detection and blocking, replay validation, coherence
#       checking, bio-async integration, and bidirectional effects
# ============================================================================

add_executable(integration_security_test_hippocampus
    test_security_hippocampus_integration.cpp
)
target_link_libraries(integration_security_test_hippocampus
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(integration_security_test_hippocampus
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/hippocampus
        ${CMAKE_SOURCE_DIR}/include/core/hippocampus
        ${CMAKE_SOURCE_DIR}/include/cognitive/memory
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(integration_security_test_hippocampus PRIVATE cxx_std_17)
add_test(NAME integration_security_test_hippocampus
         COMMAND integration_security_test_hippocampus)
set_tests_properties(integration_security_test_hippocampus PROPERTIES
    LABELS "integration;security;hippocampus;bridge"
    TIMEOUT 300
)

# ============================================================================
# Security Orchestrator Integration Tests
# ============================================================================
# WHAT: Integration tests for Security Orchestrator
# WHY:  Verify coordination of all security bridges, FEP bridges, and the
#       Unified Security Facade. The orchestrator is the central hub for
#       security event routing and threat assessment.
# HOW:  Test multi-bridge registration, event subscription/publishing,
#       threat aggregation, lockdown coordination, multi-module correlation,
#       recovery procedures, thread safety, and performance
# ============================================================================

add_executable(integration_security_test_orchestrator
    test_security_orchestrator_integration.cpp
)
target_link_libraries(integration_security_test_orchestrator
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(integration_security_test_orchestrator
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/cognitive/integration
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(integration_security_test_orchestrator PRIVATE cxx_std_17)
add_test(NAME integration_security_test_orchestrator
         COMMAND integration_security_test_orchestrator)
set_tests_properties(integration_security_test_orchestrator PROPERTIES
    LABELS "integration;security;orchestrator;bridge"
    TIMEOUT 300
)

# ============================================================================
# Security Exception Integration Tests
# ============================================================================
# WHAT: Integration tests for security-exception-immune system pipelines
# WHY:  Verify security modules, exception system, and immune system work together
# HOW:  Test complete flow from security threat detection through exception handling
#       to immune response and recovery actions
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/exception/CMakeLists.txt")
    add_subdirectory(exception)
endif()

# ============================================================================
# LGSS (Layered Governance Safety System) Integration Tests
# ============================================================================
# WHAT: Integration tests for LGSS with brain and cognitive systems
# WHY:  Verify safety enforcement across full brain lifecycle
# HOW:  Test brain creation with LGSS enabled, action interception,
#       ethics bridge integration, bio-async safety messaging, and
#       coordinated lockdown scenarios
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lgss/CMakeLists.txt")
    add_subdirectory(lgss)
endif()

# DISABLED: Test needs API updates to match current alignment_monitor API
# TODO: Update alignment_monitor_observe to current API and proposed_action_t struct fields
#
# # ============================================================================
# # Safety Enhancement Integration Tests (AI Safety Infrastructure)
# # ============================================================================
# # WHAT: Integration tests for 10 AI safety enhancement modules
# # WHY:  Verify cross-module communication and safety pipeline integration
# # HOW:  Test tripwire->halt escalation, alignment->tripwire triggering,
# #       corrigibility<->capability enforcement, red team attacks,
# #       graduated autonomy, and value commitment verification
# # ============================================================================
#
# add_executable(integration_security_test_safety_enhancement
#     test_safety_enhancement_integration.cpp
# )
# target_link_libraries(integration_security_test_safety_enhancement
#     PRIVATE
#         GTest::GTest
#         GTest::Main
#         nimcp
#         pthread
# )
# target_include_directories(integration_security_test_safety_enhancement
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
# target_compile_features(integration_security_test_safety_enhancement PRIVATE cxx_std_17)
# add_test(NAME integration_security_test_safety_enhancement
#          COMMAND integration_security_test_safety_enhancement)
# set_tests_properties(integration_security_test_safety_enhancement PROPERTIES
#     LABELS "integration;security;safety;enhancement"
#     TIMEOUT 300
# )

# ============================================================================
# BBB + Immune Integration Tests
# ============================================================================
# WHAT: Integration tests for BBB and Brain Immune System integration
# WHY:  Verify bidirectional coordination between security and immune systems
# HOW:  Test connection/disconnection, quarantine with immune response,
#       concurrent operations, memory pressure, timeout and recovery,
#       tripwires with statistics
# ============================================================================

add_executable(integration_security_test_bbb_immune
    test_bbb_immune_integration.cpp
)
target_link_libraries(integration_security_test_bbb_immune
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(integration_security_test_bbb_immune
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/cognitive
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(integration_security_test_bbb_immune PRIVATE cxx_std_17)
add_test(NAME integration_security_test_bbb_immune
         COMMAND integration_security_test_bbb_immune)
set_tests_properties(integration_security_test_bbb_immune PROPERTIES
    LABELS "integration;security;bbb;immune;tripwire"
    TIMEOUT 180
)

# ============================================================================
# Immune Error Routing Integration Tests (P1-2/3/5/6 + P2-6)
# ============================================================================
add_executable(integration_security_test_immune_error_routing
    test_immune_error_routing.cpp
)
target_link_libraries(integration_security_test_immune_error_routing
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(integration_security_test_immune_error_routing
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
add_test(NAME integration_security_test_immune_error_routing
         COMMAND integration_security_test_immune_error_routing)
set_tests_properties(integration_security_test_immune_error_routing PROPERTIES
    LABELS "integration;security;immune;error-routing"
    TIMEOUT 120
)

# ============================================================================
# BBB Ref-Count Integration Tests (P0-P2 Remediation)
# ============================================================================
# WHAT: Integration tests for BBB quarantine reference counting
# WHY:  Verify TOCTOU-safe quarantine checks with reference acquisition
# HOW:  Test quarantine + safe check + ref acquire/release integration

add_executable(integration_security_test_bbb_refcount
    test_bbb_refcount_integration.cpp
)
target_link_libraries(integration_security_test_bbb_refcount
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(integration_security_test_bbb_refcount
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/utils/error
)
target_compile_features(integration_security_test_bbb_refcount PRIVATE cxx_std_17)
add_test(NAME integration_security_test_bbb_refcount
         COMMAND integration_security_test_bbb_refcount)
set_tests_properties(integration_security_test_bbb_refcount PROPERTIES
    LABELS "integration;security;bbb;refcount;quarantine"
    TIMEOUT 120
)

# ============================================================================
# Security Edge Integration Tests
# ============================================================================
add_executable(integration_security_test_security_edge_integration
    test_security_edge_integration.cpp
)
target_link_libraries(integration_security_test_security_edge_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(integration_security_test_security_edge_integration
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/test
)
target_compile_features(integration_security_test_security_edge_integration PRIVATE cxx_std_17)
add_test(NAME integration_security_test_security_edge_integration
         COMMAND integration_security_test_security_edge_integration)
set_tests_properties(integration_security_test_security_edge_integration PROPERTIES
    LABELS "integration;security;edge"
    TIMEOUT 120
)

# ============================================================================
# Immune + Bio-Async Integration Tests
# ============================================================================
# WHAT: Integration tests for immune + bio-async working together
# WHY:  Verify cross-system threat detection, response, and recovery
# HOW:  Create BBB + immune + bio-router, test end-to-end workflows
# ============================================================================

add_executable(integration_security_test_immune_bio_async
    test_immune_bio_async_integration.cpp
)
target_link_libraries(integration_security_test_immune_bio_async
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(integration_security_test_immune_bio_async
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
target_compile_features(integration_security_test_immune_bio_async PRIVATE cxx_std_17)
add_test(NAME integration_security_test_immune_bio_async
         COMMAND integration_security_test_immune_bio_async)
set_tests_properties(integration_security_test_immune_bio_async PROPERTIES
    LABELS "integration;security;immune;bio-async;coverage"
    TIMEOUT 300
)
