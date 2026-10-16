# Unit tests for neuro-symbolic mathematics system
#
# Tests the Mathesis-inspired genius-level mathematics components:
# - Energy-based consistency checking (E=0 = consistent)
# - Hypergraph data structure for n-ary relations
# - Quantum Monte Carlo Tree Search
# - Genius Math Orchestrator

# ============================================================================
# Energy Consistency Tests
# ============================================================================

add_executable(unit_test_energy_consistency
    test_energy_consistency.cpp
)

target_link_libraries(unit_test_energy_consistency
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_test_energy_consistency
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(unit_test_energy_consistency PRIVATE cxx_std_17)

gtest_discover_tests(unit_test_energy_consistency
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;neuro_symbolic;energy_consistency"
        TIMEOUT 90
)

# ============================================================================
# Hypergraph Tests
# ============================================================================

add_executable(unit_test_hypergraph
    test_hypergraph.cpp
)

target_link_libraries(unit_test_hypergraph
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_test_hypergraph
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(unit_test_hypergraph PRIVATE cxx_std_17)

gtest_discover_tests(unit_test_hypergraph
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;neuro_symbolic;hypergraph"
        TIMEOUT 90
)

# ============================================================================
# Quantum MCTS Tests
# ============================================================================

add_executable(unit_test_quantum_mcts
    test_quantum_mcts.cpp
)

target_link_libraries(unit_test_quantum_mcts
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_test_quantum_mcts
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(unit_test_quantum_mcts PRIVATE cxx_std_17)

gtest_discover_tests(unit_test_quantum_mcts
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;neuro_symbolic;quantum_mcts"
        TIMEOUT 120
)

# ============================================================================
# Genius Math Orchestrator Tests
# ============================================================================

add_executable(unit_test_genius_orchestrator
    test_genius_orchestrator.cpp
)

target_link_libraries(unit_test_genius_orchestrator
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_test_genius_orchestrator
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(unit_test_genius_orchestrator PRIVATE cxx_std_17)

gtest_discover_tests(unit_test_genius_orchestrator
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;neuro_symbolic;genius_orchestrator"
        TIMEOUT 120
)

# ============================================================================
# Evolutionary Proof Search Tests
# ============================================================================

add_executable(unit_test_evolutionary_proof
    test_evolutionary_proof.cpp
)

target_link_libraries(unit_test_evolutionary_proof
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_test_evolutionary_proof
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(unit_test_evolutionary_proof PRIVATE cxx_std_17)

gtest_discover_tests(unit_test_evolutionary_proof
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;neuro_symbolic;evolutionary_proof"
        TIMEOUT 120
)

# ============================================================================
# Quantum Math Engine Tests
# ============================================================================

add_executable(unit_test_quantum_math_engine
    test_quantum_math_engine.cpp
)

target_link_libraries(unit_test_quantum_math_engine
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_test_quantum_math_engine
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(unit_test_quantum_math_engine PRIVATE cxx_std_17)

gtest_discover_tests(unit_test_quantum_math_engine
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;neuro_symbolic;quantum_math_engine"
        TIMEOUT 120
)
