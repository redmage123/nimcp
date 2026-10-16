# ============================================================================
# Security Unit Tests - Blood-Brain Barrier (Phase IS-1)
# ============================================================================
# WHAT: Unit tests for Neural Immune System perimeter defense
# WHY:  Verify individual BBB module functionality and security guarantees
# HOW:  Test each module in isolation: input gate, code signing, memory, access
# ============================================================================

# Blood-Brain Barrier Unit Tests (79 tests)
add_executable(unit_security_test_blood_brain_barrier
    test_blood_brain_barrier.cpp
)
target_link_libraries(unit_security_test_blood_brain_barrier
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_security_test_blood_brain_barrier
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME unit_security_test_blood_brain_barrier
         COMMAND unit_security_test_blood_brain_barrier)
set_tests_properties(unit_security_test_blood_brain_barrier PROPERTIES
    LABELS "unit"
    TIMEOUT 60
)

# BBB-Immune Integration Tests
add_executable(unit_security_test_bbb_immune_integration
    test_bbb_immune_integration.cpp
)
target_link_libraries(unit_security_test_bbb_immune_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_security_test_bbb_immune_integration
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME unit_security_test_bbb_immune_integration
         COMMAND unit_security_test_bbb_immune_integration)
set_tests_properties(unit_security_test_bbb_immune_integration PROPERTIES
    LABELS "unit;integration;security;immune"
    TIMEOUT 60
)

# General Security Unit Tests (legacy)
add_executable(unit_security_test_security
    test_security.cpp
)
target_link_libraries(unit_security_test_security
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_security_test_security
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
)
add_test(NAME unit_security_test_security
         COMMAND unit_security_test_security)
set_tests_properties(unit_security_test_security PROPERTIES LABELS "unit")

# Comprehensive Security Unit Tests (legacy)
add_executable(unit_security_test_security_comprehensive
    test_security_comprehensive.cpp
)
target_link_libraries(unit_security_test_security_comprehensive
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_security_test_security_comprehensive
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
)
add_test(NAME unit_security_test_security_comprehensive
         COMMAND unit_security_test_security_comprehensive)
set_tests_properties(unit_security_test_security_comprehensive PROPERTIES
    LABELS "unit"
    TIMEOUT 60
)

# Security Coverage Framework Unit Tests (Phase SC-1)
add_executable(unit_security_test_security_coverage
    test_security_coverage.cpp
)
target_link_libraries(unit_security_test_security_coverage
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_security_test_security_coverage
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
)
add_test(NAME unit_security_test_security_coverage
         COMMAND unit_security_test_security_coverage)
set_tests_properties(unit_security_test_security_coverage PROPERTIES
    LABELS "unit;security;sc1"
    TIMEOUT 120
)

# ============================================================================
# Security Integration Framework Unit Tests (Phase SC-4)
# ============================================================================
# WHAT: Unit tests for Universal Security Integration Framework
# WHY:  Verify module registration, trust management, DP queries, events
# HOW:  Test integration API for all module categories
# ============================================================================

add_executable(unit_security_test_security_integration
    test_security_integration.cpp
)
target_link_libraries(unit_security_test_security_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(unit_security_test_security_integration
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME unit_security_test_security_integration
         COMMAND unit_security_test_security_integration)
set_tests_properties(unit_security_test_security_integration PROPERTIES
    LABELS "unit;security;sc4"
    TIMEOUT 120
)

# ============================================================================
# Security State Management Unit Tests
# ============================================================================
# WHAT: Unit tests for Security Level Management
# WHY:  Verify downgrade prevention, level locking, component levels
# HOW:  Test security level API, feature enablement, audit trail
# ============================================================================

add_executable(unit_security_test_security_level
    test_security_level.cpp
)
target_link_libraries(unit_security_test_security_level
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(unit_security_test_security_level
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
)
add_test(NAME unit_security_test_security_level
         COMMAND unit_security_test_security_level)
set_tests_properties(unit_security_test_security_level PROPERTIES
    LABELS "unit;security"
    TIMEOUT 60
)

# ============================================================================
# Distributed Security Consensus Unit Tests
# ============================================================================
# WHAT: Unit tests for Raft-like Security Consensus
# WHY:  Verify leader election, policy replication, threat sharing
# HOW:  Test consensus protocol, role transitions, cluster coordination
# ============================================================================

add_executable(unit_security_test_security_consensus
    test_security_consensus.cpp
)
target_link_libraries(unit_security_test_security_consensus
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(unit_security_test_security_consensus
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/security
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
)
add_test(NAME unit_security_test_security_consensus
         COMMAND unit_security_test_security_consensus)
set_tests_properties(unit_security_test_security_consensus PROPERTIES
    LABELS "unit;security"
    TIMEOUT 120
)

# ============================================================================
# Enhanced Security Framework Unit Tests (Phase SC-5)
# ============================================================================

# TOCTOU + Rate Limiting Unit Tests
# add_executable(unit_security_test_rate_limiter test_rate_limiter.cpp)
# target_link_libraries(unit_security_test_rate_limiter PRIVATE GTest::GTest GTest::Main nimcp pthread)
# target_include_directories(unit_security_test_rate_limiter PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
# add_test(NAME unit_security_test_rate_limiter COMMAND unit_security_test_rate_limiter)
# set_tests_properties(unit_security_test_rate_limiter PROPERTIES LABELS "unit;security" TIMEOUT 60)

# Constant-Time Crypto Unit Tests
add_executable(unit_security_test_constant_time test_constant_time.cpp)
target_link_libraries(unit_security_test_constant_time PRIVATE GTest::GTest GTest::Main nimcp pthread)
target_include_directories(unit_security_test_constant_time PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
add_test(NAME unit_security_test_constant_time COMMAND unit_security_test_constant_time)
set_tests_properties(unit_security_test_constant_time PROPERTIES LABELS "unit;security" TIMEOUT 60)

# Key Derivation Unit Tests
add_executable(unit_security_test_key_derivation test_key_derivation.cpp)
target_link_libraries(unit_security_test_key_derivation PRIVATE GTest::GTest GTest::Main nimcp pthread)
target_include_directories(unit_security_test_key_derivation PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
add_test(NAME unit_security_test_key_derivation COMMAND unit_security_test_key_derivation)
set_tests_properties(unit_security_test_key_derivation PROPERTIES LABELS "unit;security" TIMEOUT 120)

# Path Traversal Detection Unit Tests
add_executable(unit_security_test_path_traversal test_path_traversal.cpp)
target_link_libraries(unit_security_test_path_traversal PRIVATE GTest::GTest GTest::Main nimcp pthread)
target_include_directories(unit_security_test_path_traversal PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
add_test(NAME unit_security_test_path_traversal COMMAND unit_security_test_path_traversal)
set_tests_properties(unit_security_test_path_traversal PROPERTIES LABELS "unit;security" TIMEOUT 60)

# Shell Command Injection Detection Unit Tests
add_executable(unit_security_test_shell_detector test_shell_detector.cpp)
target_link_libraries(unit_security_test_shell_detector PRIVATE GTest::GTest GTest::Main nimcp pthread)
target_include_directories(unit_security_test_shell_detector PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
add_test(NAME unit_security_test_shell_detector COMMAND unit_security_test_shell_detector)
set_tests_properties(unit_security_test_shell_detector PROPERTIES LABELS "unit;security" TIMEOUT 60)

# ML-Based Anomaly Detection Unit Tests
add_executable(unit_security_test_anomaly_detector test_anomaly_detector.cpp)
target_link_libraries(unit_security_test_anomaly_detector PRIVATE GTest::GTest GTest::Main nimcp pthread)
target_include_directories(unit_security_test_anomaly_detector PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
add_test(NAME unit_security_test_anomaly_detector COMMAND unit_security_test_anomaly_detector)
set_tests_properties(unit_security_test_anomaly_detector PROPERTIES LABELS "unit;security" TIMEOUT 120)

# Bayesian Network Unit Tests
add_executable(unit_security_test_bayesian_network test_bayesian_network.cpp)
target_link_libraries(unit_security_test_bayesian_network PRIVATE GTest::GTest GTest::Main nimcp pthread)
target_include_directories(unit_security_test_bayesian_network PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
add_test(NAME unit_security_test_bayesian_network COMMAND unit_security_test_bayesian_network)
set_tests_properties(unit_security_test_bayesian_network PROPERTIES LABELS "unit;security" TIMEOUT 60)

# Dynamic Pattern Database Unit Tests
add_executable(unit_security_test_pattern_db test_pattern_db.cpp)
target_link_libraries(unit_security_test_pattern_db PRIVATE GTest::GTest GTest::Main nimcp pthread)
target_include_directories(unit_security_test_pattern_db PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
add_test(NAME unit_security_test_pattern_db COMMAND unit_security_test_pattern_db)
set_tests_properties(unit_security_test_pattern_db PROPERTIES LABELS "unit;security" TIMEOUT 60)

# Encrypted Audit Buffer Unit Tests
add_executable(unit_security_test_encrypted_audit test_encrypted_audit.cpp)
target_link_libraries(unit_security_test_encrypted_audit PRIVATE GTest::GTest GTest::Main nimcp pthread)
target_include_directories(unit_security_test_encrypted_audit PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
add_test(NAME unit_security_test_encrypted_audit COMMAND unit_security_test_encrypted_audit)
set_tests_properties(unit_security_test_encrypted_audit PROPERTIES LABELS "unit;security" TIMEOUT 60)

# Policy Parser Unit Tests
add_executable(unit_security_test_policy_parser test_policy_parser.cpp)
target_link_libraries(unit_security_test_policy_parser PRIVATE GTest::GTest GTest::Main nimcp pthread)
target_include_directories(unit_security_test_policy_parser PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
add_test(NAME unit_security_test_policy_parser COMMAND unit_security_test_policy_parser)
set_tests_properties(unit_security_test_policy_parser PROPERTIES LABELS "unit;security" TIMEOUT 60)

# Policy Evaluator Unit Tests
# TEMPORARILY DISABLED - Policy compiler has unimplemented node types
# TODO: Implement policy compiler for all AST node types
# add_executable(unit_security_test_policy_evaluator test_policy_evaluator.cpp)
# target_link_libraries(unit_security_test_policy_evaluator PRIVATE GTest::GTest GTest::Main nimcp pthread)
# target_include_directories(unit_security_test_policy_evaluator PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
# add_test(NAME unit_security_test_policy_evaluator COMMAND unit_security_test_policy_evaluator)
# set_tests_properties(unit_security_test_policy_evaluator PROPERTIES LABELS "unit;security" TIMEOUT 60)

# Post-Quantum Cryptography Unit Tests
add_executable(unit_security_test_post_quantum test_post_quantum.cpp)
target_link_libraries(unit_security_test_post_quantum PRIVATE GTest::GTest GTest::Main nimcp pthread)
target_include_directories(unit_security_test_post_quantum PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
add_test(NAME unit_security_test_post_quantum COMMAND unit_security_test_post_quantum)
set_tests_properties(unit_security_test_post_quantum PROPERTIES LABELS "unit;security" TIMEOUT 120)

# Supply Chain Security Unit Tests
add_executable(unit_security_test_supply_chain test_supply_chain.cpp)
target_link_libraries(unit_security_test_supply_chain PRIVATE GTest::GTest GTest::Main nimcp pthread)
target_include_directories(unit_security_test_supply_chain PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
add_test(NAME unit_security_test_supply_chain COMMAND unit_security_test_supply_chain)
set_tests_properties(unit_security_test_supply_chain PROPERTIES LABELS "unit;security" TIMEOUT 60)

# NaN/Inf Validation Unit Tests
add_executable(unit_security_test_nan_inf_validation unit_security_test_nan_inf_validation.cpp)
target_link_libraries(unit_security_test_nan_inf_validation PRIVATE GTest::GTest GTest::Main nimcp pthread)
target_include_directories(unit_security_test_nan_inf_validation PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
add_test(NAME unit_security_test_nan_inf_validation COMMAND unit_security_test_nan_inf_validation)
set_tests_properties(unit_security_test_nan_inf_validation PROPERTIES LABELS "unit;security" TIMEOUT 60)

# ============================================================================
# Security FEP Bridge Tests
# ============================================================================
# WHAT: Unit tests for Security-FEP bidirectional integration bridges
# WHY:  Ensure threat detection integrates with FEP predictions
# HOW:  Test lifecycle, effects, and security-specific FEP modulations
# ============================================================================

add_executable(unit_security_test_fep_bridges test_security_fep_bridges.cpp)
target_link_libraries(unit_security_test_fep_bridges PRIVATE GTest::GTest GTest::Main nimcp pthread)
target_include_directories(unit_security_test_fep_bridges PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
add_test(NAME unit_security_test_fep_bridges COMMAND unit_security_test_fep_bridges)
set_tests_properties(unit_security_test_fep_bridges PROPERTIES LABELS "unit;security;fep" TIMEOUT 60)

# ============================================================================
# Security-Perception Bridge Unit Tests
# ============================================================================
# WHAT: Unit tests for Security-Perception Bridge (Sensory Threat Defense)
# WHY:  Verify sensory threat detection, quarantine, signature learning
# HOW:  Test visual/audio/multimodal analysis, cross-modal consistency
# ============================================================================

add_executable(unit_security_test_security_perception_bridge
    test_security_perception_bridge.cpp
)
target_link_libraries(unit_security_test_security_perception_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(unit_security_test_security_perception_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/perception
        ${CMAKE_SOURCE_DIR}/include/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME unit_security_test_security_perception_bridge
         COMMAND unit_security_test_security_perception_bridge)
set_tests_properties(unit_security_test_security_perception_bridge PROPERTIES
    LABELS "unit;security;perception;bridge"
    TIMEOUT 120
)

# ============================================================================
# Security-Async Bridge Unit Tests
# ============================================================================
# WHAT: Unit tests for Security-Async Bridge (Bio-Async Integration)
# WHY:  Verify security event broadcasting, threat intel sharing, emergency modes
# HOW:  Test lifecycle, connections, bidirectional effects, threat intelligence
# ============================================================================

add_executable(unit_security_test_security_async_bridge
    test_security_async_bridge.cpp
)
target_link_libraries(unit_security_test_security_async_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(unit_security_test_security_async_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/async
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(unit_security_test_security_async_bridge PRIVATE cxx_std_17)
add_test(NAME unit_security_test_security_async_bridge
         COMMAND unit_security_test_security_async_bridge)
set_tests_properties(unit_security_test_security_async_bridge PROPERTIES
    LABELS "unit;security;async;bridge"
    TIMEOUT 120
)

# ============================================================================
# Security-Logging Bridge Unit Tests
# ============================================================================
# WHAT: Unit tests for Security-Logging Bridge (Audit & Pattern Detection)
# WHY:  Verify security event logging, pattern analysis, bidirectional effects
# HOW:  Test lifecycle, connections, core logging, streaming, queries, export
# ============================================================================

add_executable(unit_security_test_security_logging_bridge
    test_security_logging_bridge.cpp
)
target_link_libraries(unit_security_test_security_logging_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(unit_security_test_security_logging_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/logging
        ${CMAKE_SOURCE_DIR}/include/utils/logging
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(unit_security_test_security_logging_bridge PRIVATE cxx_std_17)
add_test(NAME unit_security_test_security_logging_bridge
         COMMAND unit_security_test_security_logging_bridge)
set_tests_properties(unit_security_test_security_logging_bridge PROPERTIES
    LABELS "unit;security;logging;bridge"
    TIMEOUT 120
)

# Security Immune Bridge Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/immune/CMakeLists.txt")
    add_subdirectory(immune)
endif()

# Security Sleep Bridge Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sleep/CMakeLists.txt")
    add_subdirectory(sleep)
endif()

# ============================================================================
# Security Module Bridge Unit Tests (Phase SMB)
# ============================================================================
# WHAT: Unit tests for Security Module Bridges
# WHY:  Verify security integration with perception, language, executive,
#       memory, training, and rcog modules
# HOW:  Test lifecycle, validation, threat detection, and bidirectional effects
# ============================================================================

# Security-Perception Input Bridge Unit Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/perception/CMakeLists.txt")
    add_subdirectory(perception)
endif()

# Security-Language Bridge Unit Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/language/CMakeLists.txt")
    add_subdirectory(language)
endif()

# Security-Executive Bridge Unit Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/executive/CMakeLists.txt")
    add_subdirectory(executive)
endif()

# Security-Memory Bridge Unit Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/memory/CMakeLists.txt")
    add_subdirectory(memory)
endif()

# Security-Training Bridge Unit Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/training/CMakeLists.txt")
    add_subdirectory(training)
endif()

# Security-RCog Bridge Unit Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/rcog/CMakeLists.txt")
    add_subdirectory(rcog)
endif()

# ============================================================================
# Security Cognitive Bridge Unit Tests (Phase SCB)
# ============================================================================
# WHAT: Unit tests for Security Cognitive Bridges
# WHY:  Verify security integration with cognitive modules (distributed training,
#       knowledge graph, game theory, imagination, continual learning, epistemic,
#       collective cognition, hippocampus)
# HOW:  Test lifecycle, validation, threat detection, and bidirectional effects
# ============================================================================

# Security-Distributed Training Bridge Unit Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/distributed/CMakeLists.txt")
    add_subdirectory(distributed)
endif()

# Security-Knowledge Graph Bridge Unit Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/knowledge/CMakeLists.txt")
    add_subdirectory(knowledge)
endif()

# Security-Game Theory Bridge Unit Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/game_theory/CMakeLists.txt")
    add_subdirectory(game_theory)
endif()

# Security-Imagination Bridge Unit Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/imagination/CMakeLists.txt")
    add_subdirectory(imagination)
endif()

# Security-Continual Learning Bridge Unit Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/continual/CMakeLists.txt")
    add_subdirectory(continual)
endif()

# Security-Epistemic Bridge Unit Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/epistemic/CMakeLists.txt")
    add_subdirectory(epistemic)
endif()

# Security-Collective Cognition Bridge Unit Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/collective/CMakeLists.txt")
    add_subdirectory(collective)
endif()

# Security-Hippocampus Bridge Unit Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/hippocampus/CMakeLists.txt")
    add_subdirectory(hippocampus)
endif()

# Security Orchestrator Unit Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/orchestrator/CMakeLists.txt")
    add_subdirectory(orchestrator)
endif()

# ============================================================================
# Capability System Thread Safety Unit Tests
# ============================================================================
# WHAT: Unit tests for Capability-Based Access Control thread safety
# WHY:  Verify mutex protection for concurrent capability operations
# HOW:  Test concurrent grant/revoke, check, create/destroy, stress tests
# ============================================================================

add_executable(unit_security_test_capability_thread_safety
    test_capability_thread_safety.cpp
)
target_link_libraries(unit_security_test_capability_thread_safety
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(unit_security_test_capability_thread_safety
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(unit_security_test_capability_thread_safety PRIVATE cxx_std_17)
add_test(NAME unit_security_test_capability_thread_safety
         COMMAND unit_security_test_capability_thread_safety)
set_tests_properties(unit_security_test_capability_thread_safety PROPERTIES
    LABELS "unit;security;thread-safety;capability"
    TIMEOUT 120
)

# ============================================================================
# Security Exception Handling Unit Tests
# ============================================================================
# WHAT: Unit tests for security module exception handling
# WHY:  Verify security exceptions are properly created, dispatched, and handled
# HOW:  Test exception creation, handler registration, dispatch chain,
#       immune presentation, and recovery actions for security threats
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/exception/CMakeLists.txt")
    add_subdirectory(exception)
endif()

# ============================================================================
# LGSS (Layered Governance Safety System) Unit Tests
# ============================================================================
# WHAT: Unit tests for LGSS safety subsystem
# WHY:  Verify safety knowledge base, action interceptor, and all safety guards
# HOW:  Test safety KB CRUD, immutability after lock, evaluation logic,
#       integrity verification, and fail-safe behavior
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lgss/CMakeLists.txt")
    add_subdirectory(lgss)
endif()

# ============================================================================
# Safety Enhancement Module Unit Tests
# ============================================================================
# WHAT: Unit tests for AI safety enhancement modules
# WHY:  Verify emergency halt, tripwires, corrigibility, alignment monitoring,
#       capability control, interpretability, verification, red team,
#       graduated autonomy, and value commitment
# HOW:  Test each module lifecycle, core functionality, and integration
# ============================================================================

# Emergency Halt Unit Tests
add_executable(unit_security_test_emergency_halt
    test_emergency_halt.cpp
)
target_link_libraries(unit_security_test_emergency_halt
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(unit_security_test_emergency_halt
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/test
)
target_compile_features(unit_security_test_emergency_halt PRIVATE cxx_std_17)
add_test(NAME unit_security_test_emergency_halt
         COMMAND unit_security_test_emergency_halt)
set_tests_properties(unit_security_test_emergency_halt PROPERTIES
    LABELS "unit;security;safety;emergency-halt"
    TIMEOUT 60
)

# Tripwire Detection Unit Tests
add_executable(unit_security_test_tripwires
    test_tripwires.cpp
)
target_link_libraries(unit_security_test_tripwires
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_security_test_tripwires
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/test
)
target_compile_features(unit_security_test_tripwires PRIVATE cxx_std_17)
add_test(NAME unit_security_test_tripwires
         COMMAND unit_security_test_tripwires)
set_tests_properties(unit_security_test_tripwires PROPERTIES
    LABELS "unit;security;safety;tripwires"
    TIMEOUT 60
)

# Corrigibility Unit Tests
add_executable(unit_security_test_corrigibility
    test_corrigibility.cpp
)
target_link_libraries(unit_security_test_corrigibility
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_security_test_corrigibility
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/test
)
target_compile_features(unit_security_test_corrigibility PRIVATE cxx_std_17)
add_test(NAME unit_security_test_corrigibility
         COMMAND unit_security_test_corrigibility)
set_tests_properties(unit_security_test_corrigibility PROPERTIES
    LABELS "unit;security;safety;corrigibility"
    TIMEOUT 60
)

# Alignment Monitor Unit Tests
add_executable(unit_security_test_alignment_monitor
    test_alignment_monitor.cpp
)
target_link_libraries(unit_security_test_alignment_monitor
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_security_test_alignment_monitor
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/test
)
target_compile_features(unit_security_test_alignment_monitor PRIVATE cxx_std_17)
add_test(NAME unit_security_test_alignment_monitor
         COMMAND unit_security_test_alignment_monitor)
set_tests_properties(unit_security_test_alignment_monitor PROPERTIES
    LABELS "unit;security;safety;alignment"
    TIMEOUT 60
)

# Capability Control Unit Tests
add_executable(unit_security_test_capability_control
    test_capability_control.cpp
)
target_link_libraries(unit_security_test_capability_control
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_security_test_capability_control
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/test
)
target_compile_features(unit_security_test_capability_control PRIVATE cxx_std_17)
add_test(NAME unit_security_test_capability_control
         COMMAND unit_security_test_capability_control)
set_tests_properties(unit_security_test_capability_control PROPERTIES
    LABELS "unit;security;safety;capability"
    TIMEOUT 60
)

# Interpretability Unit Tests
add_executable(unit_security_test_interpretability
    test_interpretability.cpp
)
target_link_libraries(unit_security_test_interpretability
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_security_test_interpretability
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/test
)
target_compile_features(unit_security_test_interpretability PRIVATE cxx_std_17)
add_test(NAME unit_security_test_interpretability
         COMMAND unit_security_test_interpretability)
set_tests_properties(unit_security_test_interpretability PROPERTIES
    LABELS "unit;security;safety;interpretability"
    TIMEOUT 60
)

# Safety Verification Unit Tests
add_executable(unit_security_test_safety_verification
    test_safety_verification.cpp
)
target_link_libraries(unit_security_test_safety_verification
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_security_test_safety_verification
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/test
)
target_compile_features(unit_security_test_safety_verification PRIVATE cxx_std_17)
add_test(NAME unit_security_test_safety_verification
         COMMAND unit_security_test_safety_verification)
set_tests_properties(unit_security_test_safety_verification PROPERTIES
    LABELS "unit;security;safety;verification"
    TIMEOUT 60
)

# Red Team Unit Tests
add_executable(unit_security_test_red_team
    test_red_team.cpp
)
target_link_libraries(unit_security_test_red_team
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_security_test_red_team
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/test
)
target_compile_features(unit_security_test_red_team PRIVATE cxx_std_17)
add_test(NAME unit_security_test_red_team
         COMMAND unit_security_test_red_team)
set_tests_properties(unit_security_test_red_team PROPERTIES
    LABELS "unit;security;safety;red-team"
    TIMEOUT 60
)

# Graduated Autonomy Unit Tests
add_executable(unit_security_test_graduated_autonomy
    test_graduated_autonomy.cpp
)
target_link_libraries(unit_security_test_graduated_autonomy
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_security_test_graduated_autonomy
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/test
)
target_compile_features(unit_security_test_graduated_autonomy PRIVATE cxx_std_17)
add_test(NAME unit_security_test_graduated_autonomy
         COMMAND unit_security_test_graduated_autonomy)
set_tests_properties(unit_security_test_graduated_autonomy PROPERTIES
    LABELS "unit;security;safety;autonomy"
    TIMEOUT 60
)

# Value Commitment Unit Tests
add_executable(unit_security_test_value_commitment
    test_value_commitment.cpp
)
target_link_libraries(unit_security_test_value_commitment
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_security_test_value_commitment
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/test
)
target_compile_features(unit_security_test_value_commitment PRIVATE cxx_std_17)
add_test(NAME unit_security_test_value_commitment
         COMMAND unit_security_test_value_commitment)
set_tests_properties(unit_security_test_value_commitment PROPERTIES
    LABELS "unit;security;safety;value-commitment"
    TIMEOUT 60
)

# ============================================================================
# Pattern Compiler Unit Tests
# ============================================================================
# WHAT: Unit tests for pattern compilation, validation, and security checks
# WHY:  Verify pattern compiler correctly validates, compiles, and secures patterns
# HOW:  Test valid/invalid patterns, ReDoS protection, complexity, optimizations
# ============================================================================

add_executable(unit_security_test_pattern_compiler
    test_pattern_compiler.cpp
)
target_link_libraries(unit_security_test_pattern_compiler
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(unit_security_test_pattern_compiler
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/test
)
target_compile_features(unit_security_test_pattern_compiler PRIVATE cxx_std_17)
add_test(NAME unit_security_test_pattern_compiler
         COMMAND unit_security_test_pattern_compiler)
set_tests_properties(unit_security_test_pattern_compiler PROPERTIES
    LABELS "unit;security;pattern-compiler"
    TIMEOUT 60
)

# ============================================================================
# Artifact Verification Unit Tests
# ============================================================================
# WHAT: Unit tests for artifact signature and hash verification
# WHY:  Verify artifact integrity checks work correctly for supply chain security
# HOW:  Test signature/hash verification, trusted sources, certificates, concurrency
# ============================================================================

add_executable(unit_security_test_artifact_verify
    test_artifact_verify.cpp
)
target_link_libraries(unit_security_test_artifact_verify
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(unit_security_test_artifact_verify
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/test
)
target_compile_features(unit_security_test_artifact_verify PRIVATE cxx_std_17)
add_test(NAME unit_security_test_artifact_verify
         COMMAND unit_security_test_artifact_verify)
set_tests_properties(unit_security_test_artifact_verify PROPERTIES
    LABELS "unit;security;artifact-verify;supply-chain"
    TIMEOUT 120
)

# BBB (Blood-Brain Barrier) Sublayer Unit Tests
# Note: Pure C tests are managed in test/CMakeLists.txt to avoid GTest dependency
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bbb/CMakeLists.txt")
    add_subdirectory(bbb)
endif()

# ============================================================================
# BBB API Validation Tests (P1-4)
# ============================================================================
add_executable(unit_security_test_bbb_api_validation
    test_bbb_api_validation.cpp
)
target_link_libraries(unit_security_test_bbb_api_validation
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_security_test_bbb_api_validation
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME unit_security_test_bbb_api_validation
         COMMAND unit_security_test_bbb_api_validation)
set_tests_properties(unit_security_test_bbb_api_validation PROPERTIES
    LABELS "unit;security;bbb;p1-4"
    TIMEOUT 60
)

# ============================================================================
# Security Error Codes Tests (P2-7)
# ============================================================================
add_executable(unit_security_test_error_codes
    test_security_error_codes.cpp
)
target_link_libraries(unit_security_test_error_codes
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_security_test_error_codes
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/utils/error
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME unit_security_test_error_codes
         COMMAND unit_security_test_error_codes)
set_tests_properties(unit_security_test_error_codes PROPERTIES
    LABELS "unit;security;error-codes;p2-7"
    TIMEOUT 60
)

# ============================================================================
# BBB Quarantine RefCount Tests (P0-1, P0-2, P0-3, P1-1)
# ============================================================================
add_executable(unit_security_test_bbb_quarantine_refcount
    test_bbb_quarantine_refcount.cpp
)
target_link_libraries(unit_security_test_bbb_quarantine_refcount
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_security_test_bbb_quarantine_refcount
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
add_test(NAME unit_security_test_bbb_quarantine_refcount
         COMMAND unit_security_test_bbb_quarantine_refcount)
set_tests_properties(unit_security_test_bbb_quarantine_refcount PROPERTIES
    LABELS "unit;security;bbb;quarantine;refcount;p0;p1"
    TIMEOUT 60
)

# Session security tests: BBB edge validation, gradient validation, audit trail
foreach(TEST_NAME bbb_edge_validation gradient_validation audit_trail)
    add_executable(unit_security_test_${TEST_NAME}
        test_${TEST_NAME}.cpp
    )
    target_link_libraries(unit_security_test_${TEST_NAME}
        PRIVATE GTest::GTest GTest::Main nimcp nimcp_lnn
                Python3::Python
    )
    target_include_directories(unit_security_test_${TEST_NAME}
        PRIVATE ${CMAKE_SOURCE_DIR}/include
                ${CMAKE_SOURCE_DIR}/src
                ${CMAKE_SOURCE_DIR}/test
    )
    target_compile_features(unit_security_test_${TEST_NAME} PRIVATE cxx_std_20)
    target_compile_definitions(unit_security_test_${TEST_NAME} PRIVATE NIMCP_TESTING)
    target_sources(unit_security_test_${TEST_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/src/python/nimcp_module.c
    )
    add_test(NAME unit_security_test_${TEST_NAME}
             COMMAND unit_security_test_${TEST_NAME})
    set_tests_properties(unit_security_test_${TEST_NAME} PROPERTIES
        LABELS "unit;security;session" TIMEOUT 60)
endforeach()

# ============================================================================
# Immune System Coverage Unit Tests
# ============================================================================
# WHAT: Unit tests for immune system coverage improvements
# WHY:  Verify tolerance, memory decay, secondary response, NULL handling, stats
# HOW:  Test standalone immune system functions directly
# ============================================================================

add_executable(unit_security_test_immune_coverage
    test_immune_coverage.cpp
)
target_link_libraries(unit_security_test_immune_coverage
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(unit_security_test_immune_coverage
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/cognitive/immune
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(unit_security_test_immune_coverage PRIVATE cxx_std_17)
add_test(NAME unit_security_test_immune_coverage
         COMMAND unit_security_test_immune_coverage)
set_tests_properties(unit_security_test_immune_coverage PROPERTIES
    LABELS "unit;security;immune;coverage"
    TIMEOUT 120
)

# ============================================================================
# LGSS Enhanced Tests
# ============================================================================
add_executable(unit_security_test_lgss_enhanced test_lgss_enhanced.cpp)
target_link_libraries(unit_security_test_lgss_enhanced PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_security_test_lgss_enhanced PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(unit_security_test_lgss_enhanced PRIVATE cxx_std_17)
gtest_discover_tests(unit_security_test_lgss_enhanced WORKING_DIRECTORY ${CMAKE_BINARY_DIR} PROPERTIES LABELS "unit;security;lgss" TIMEOUT 60)

# ============================================================================
# Audit Log Tests
# ============================================================================
add_executable(unit_security_test_audit_log test_audit_log.cpp)
target_link_libraries(unit_security_test_audit_log PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_security_test_audit_log PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(unit_security_test_audit_log PRIVATE cxx_std_17)
gtest_discover_tests(unit_security_test_audit_log WORKING_DIRECTORY ${CMAKE_BINARY_DIR} PROPERTIES LABELS "unit;security;audit" TIMEOUT 60)
