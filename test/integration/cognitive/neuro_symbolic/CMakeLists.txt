# ============================================================================
# Neuro-Symbolic Mathematics Integration Tests
# ============================================================================
# WHAT: Integration tests for Mathesis-inspired genius-level math system
# WHY:  Validate component interactions: energy consistency, hypergraph,
#       quantum MCTS, genius module, and orchestrator integration
# HOW:  Test data flow, cross-component operations, stress scenarios
# ============================================================================

# ----------------------------------------------------------------------------
# Main Integration Test
# ----------------------------------------------------------------------------
add_executable(integration_neuro_symbolic
    test_neuro_symbolic_integration.cpp
)

target_link_libraries(integration_neuro_symbolic
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_neuro_symbolic
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(integration_neuro_symbolic PRIVATE cxx_std_17)

gtest_discover_tests(integration_neuro_symbolic
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;neuro_symbolic"
        TIMEOUT 180
)

# ----------------------------------------------------------------------------
# Genius Orchestrator Integration Tests
# ----------------------------------------------------------------------------
add_executable(integration_genius_orchestrator
    test_genius_orchestrator_integration.cpp
)

target_link_libraries(integration_genius_orchestrator
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_genius_orchestrator
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(integration_genius_orchestrator PRIVATE cxx_std_17)

gtest_discover_tests(integration_genius_orchestrator
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;neuro_symbolic;orchestrator"
        TIMEOUT 300
)

# ----------------------------------------------------------------------------
# Quantum MCTS FEP Integration Tests
# ----------------------------------------------------------------------------
add_executable(integration_quantum_mcts_fep
    test_quantum_mcts_fep_integration.cpp
)

target_link_libraries(integration_quantum_mcts_fep
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_quantum_mcts_fep
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(integration_quantum_mcts_fep PRIVATE cxx_std_17)

gtest_discover_tests(integration_quantum_mcts_fep
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;neuro_symbolic;quantum_mcts;fep"
        TIMEOUT 240
)

# ----------------------------------------------------------------------------
# Energy Consistency FEP Integration Tests
# ----------------------------------------------------------------------------
add_executable(integration_energy_consistency_fep
    test_energy_consistency_fep_integration.cpp
)

target_link_libraries(integration_energy_consistency_fep
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_energy_consistency_fep
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(integration_energy_consistency_fep PRIVATE cxx_std_17)

gtest_discover_tests(integration_energy_consistency_fep
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;neuro_symbolic;energy_consistency;fep"
        TIMEOUT 180
)

# ----------------------------------------------------------------------------
# Hypergraph Knowledge Integration Tests
# ----------------------------------------------------------------------------
add_executable(integration_hypergraph_knowledge
    test_hypergraph_knowledge_integration.cpp
)

target_link_libraries(integration_hypergraph_knowledge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_hypergraph_knowledge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(integration_hypergraph_knowledge PRIVATE cxx_std_17)

gtest_discover_tests(integration_hypergraph_knowledge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;neuro_symbolic;hypergraph;knowledge"
        TIMEOUT 180
)

# ----------------------------------------------------------------------------
# Status Message
# ----------------------------------------------------------------------------
message(STATUS "  Added neuro-symbolic mathematics integration tests:")
message(STATUS "    - integration_neuro_symbolic (main integration)")
message(STATUS "    - integration_genius_orchestrator (orchestrator coordination)")
message(STATUS "    - integration_quantum_mcts_fep (quantum MCTS + FEP)")
message(STATUS "    - integration_energy_consistency_fep (energy consistency + FEP)")
message(STATUS "    - integration_hypergraph_knowledge (hypergraph + knowledge systems)")
