# Memory Unit Tests

# Substrate bridge tests
add_subdirectory(substrate)

# Core memory tests (PR memory node, procedural, prospective)
add_subdirectory(core)

# ============================================================================
# Working Memory - SNN Bridge Tests
# ============================================================================
# Tests for bidirectional working memory - SNN module integration

add_executable(unit_cognitive_memory_test_working_memory_snn_bridge
    test_working_memory_snn_bridge.cpp
)
target_link_libraries(unit_cognitive_memory_test_working_memory_snn_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_include_directories(unit_cognitive_memory_test_working_memory_snn_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(unit_cognitive_memory_test_working_memory_snn_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_memory_test_working_memory_snn_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;memory;snn;bridge"
        TIMEOUT 120
)

# Quantum Semantic Memory Tests
add_executable(unit_cognitive_memory_test_quantum_semantic
    test_quantum_semantic.cpp
)
target_link_libraries(unit_cognitive_memory_test_quantum_semantic
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_cognitive_memory_test_quantum_semantic PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_memory_test_quantum_semantic
    PROPERTIES LABELS "unit;cognitive;memory;quantum")

# Memory Sleep Bridge Tests
add_executable(unit_cognitive_memory_test_memory_sleep_bridge
    test_memory_sleep_bridge.cpp
)
target_link_libraries(unit_cognitive_memory_test_memory_sleep_bridge
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_cognitive_memory_test_memory_sleep_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_memory_test_memory_sleep_bridge
    PROPERTIES LABELS "unit;cognitive;memory;sleep")

# ============================================================================
# Fractal Analysis Exception Handling Tests
# ============================================================================
# WHAT: Unit tests for NIMCP_THROW_TO_IMMUNE exception handling in Fractal module
# WHY:  Ensure errors are properly reported to the brain immune system
# HOW:  Test all NULL pointer and invalid parameter paths
# ============================================================================

add_executable(unit_cognitive_memory_test_fractal_exception_handling
    test_fractal_exception_handling.cpp
)
target_link_libraries(unit_cognitive_memory_test_fractal_exception_handling
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_cognitive_memory_test_fractal_exception_handling PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_memory_test_fractal_exception_handling
    PROPERTIES LABELS "unit;cognitive;memory;fractal;exception"
    TIMEOUT 60
)
