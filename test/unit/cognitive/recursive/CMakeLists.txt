# ============================================================================
# Recursive Cognition Unit Tests
# ============================================================================
#
# WHAT: Unit tests for recursive cognition module components
# WHY:  Verify individual component functionality and API contracts
# HOW:  GTest-based tests with comprehensive coverage
#
# COMPONENTS TESTED:
# - Context Store: Variable storage and query patterns
# - Answer Refiner: Diffusion-style answer refinement
# - Orchestrator: Task decomposition and coordination
# - Delegation Pool: Worker thread management
# - Tool Router: Capability-based tool access
# - Engine: Main processing coordinator
# - Bridges: Bio-async, immune, imagination, collective, brain KG

message(STATUS "Adding recursive cognition unit tests...")

# Test 1: Context Store
add_test_binary(
    test_rcog_context_store
    ${CMAKE_CURRENT_SOURCE_DIR}/test_rcog_context_store.cpp
    unit
)

# Test 2: Answer Refiner
add_test_binary(
    test_rcog_answer
    ${CMAKE_CURRENT_SOURCE_DIR}/test_rcog_answer.cpp
    unit
)

# Test 3: All RCOG Bridges
add_test_binary(
    test_rcog_bridges
    ${CMAKE_CURRENT_SOURCE_DIR}/test_rcog_bridges.cpp
    unit
)

# Test 4: Orchestrator
add_test_binary(
    test_rcog_orchestrator
    ${CMAKE_CURRENT_SOURCE_DIR}/test_rcog_orchestrator.cpp
    unit
)

# Test 5: Delegation Pool
add_test_binary(
    test_rcog_delegation_pool
    ${CMAKE_CURRENT_SOURCE_DIR}/test_rcog_delegation_pool.cpp
    unit
)

# Test 6: Tool Router
add_test_binary(
    test_rcog_tool_router
    ${CMAKE_CURRENT_SOURCE_DIR}/test_rcog_tool_router.cpp
    unit
)

# Test 7: Engine
add_test_binary(
    test_rcog_engine
    ${CMAKE_CURRENT_SOURCE_DIR}/test_rcog_engine.cpp
    unit
)

# Test 8: FEP Bridge
add_test_binary(
    test_rcog_fep_bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/test_rcog_fep_bridge.cpp
    unit
)

message(STATUS "  Configured 8 recursive cognition unit tests")
