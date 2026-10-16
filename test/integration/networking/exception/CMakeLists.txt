# ============================================================================
# CMakeLists.txt - Networking Exception Handling Integration Tests
# ============================================================================
# WHAT: Build configuration for networking exception handling integration tests
# WHY:  Verify cross-module exception propagation and immune system integration
# HOW:  Use NIMCP standard test framework with add_test_binary function
#
# TEST COVERAGE:
# - Cross-module exception propagation (P2P -> Distributed Cognition)
# - Protocol to immune system integration
# - NLP session exception with handler chain
# - Swarm coordination exception recovery
# - Multi-module aggregate exception handling
# - Recovery strategy execution chains
# - Immune system memory and learning
# - Thread safety under concurrent exceptions
#
# @author NIMCP Development Team
# @date 2026-01-16
# ============================================================================

# Test: Networking Exception Integration
# Tests cross-module exception propagation, immune integration, recovery chains,
# aggregate exceptions, and concurrent exception handling
add_test_binary(
    test_networking_exception_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_networking_exception_integration.cpp
    integration
)

# Set extended timeout for integration tests (may involve async operations)
if(TARGET test_networking_exception_integration)
    set_tests_properties(test_networking_exception_integration PROPERTIES
        TIMEOUT 90
        LABELS "integration;networking;exception;immune"
    )
endif()
