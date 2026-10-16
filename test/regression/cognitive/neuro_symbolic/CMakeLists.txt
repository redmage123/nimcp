# ============================================================================
# Neuro-Symbolic Mathematics Regression Tests
# ============================================================================
# WHAT: Regression tests for Mathesis-inspired genius-level math system
# WHY:  Ensure API contracts, edge cases, and boundary conditions remain stable
# HOW:  Test edge cases, boundary conditions, memory safety, and API contracts
# ============================================================================

# ============================================================================
# Main Neuro-Symbolic Regression Tests
# ============================================================================

add_executable(regression_neuro_symbolic
    test_neuro_symbolic_regression.cpp
)

target_link_libraries(regression_neuro_symbolic
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_neuro_symbolic
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(regression_neuro_symbolic PRIVATE cxx_std_17)

gtest_discover_tests(regression_neuro_symbolic
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;neuro_symbolic"
        TIMEOUT 120
)

# ============================================================================
# Mathematical Genius Regression Tests
# ============================================================================
# Tests Gauss mode, pattern discovery, mode switching, configuration

add_executable(regression_genius_math
    test_genius_math_regression.cpp
)

target_link_libraries(regression_genius_math
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_genius_math
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(regression_genius_math PRIVATE cxx_std_17)

gtest_discover_tests(regression_genius_math
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;neuro_symbolic;genius_math"
        TIMEOUT 180
)

# ============================================================================
# Evolutionary Proof Regression Tests
# ============================================================================
# Tests population evolution, Q-learning convergence, fitness improvements

add_executable(regression_evolutionary_proof
    test_evolutionary_proof_regression.cpp
)

target_link_libraries(regression_evolutionary_proof
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_evolutionary_proof
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(regression_evolutionary_proof PRIVATE cxx_std_17)

gtest_discover_tests(regression_evolutionary_proof
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;neuro_symbolic;evolutionary_proof"
        TIMEOUT 180
)

# ============================================================================
# Quantum Math Engine Regression Tests
# ============================================================================
# Tests integration accuracy, variance reduction, partition function, path integrals

add_executable(regression_quantum_math_engine
    test_quantum_math_engine_regression.cpp
)

target_link_libraries(regression_quantum_math_engine
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_quantum_math_engine
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(regression_quantum_math_engine PRIVATE cxx_std_17)

gtest_discover_tests(regression_quantum_math_engine
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;neuro_symbolic;quantum_math"
        TIMEOUT 300
)

# ============================================================================
# Energy Consistency Regression Tests
# ============================================================================
# Tests E=0 for consistent proofs, violation detection, thermodynamic costs

add_executable(regression_energy_consistency
    test_energy_consistency_regression.cpp
)

target_link_libraries(regression_energy_consistency
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_energy_consistency
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(regression_energy_consistency PRIVATE cxx_std_17)

gtest_discover_tests(regression_energy_consistency
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;neuro_symbolic;energy_consistency"
        TIMEOUT 120
)

# ============================================================================
# Status Messages
# ============================================================================

message(STATUS "  Added neuro-symbolic mathematics regression tests:")
message(STATUS "    - regression_neuro_symbolic (main API tests)")
message(STATUS "    - regression_genius_math (mathematical genius module)")
message(STATUS "    - regression_evolutionary_proof (evolutionary proof search)")
message(STATUS "    - regression_quantum_math_engine (quantum math engine)")
message(STATUS "    - regression_energy_consistency (energy-based consistency)")
