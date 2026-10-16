# ============================================================================
# Cognitive Substrate Bridges Regression Tests
# ============================================================================
# WHAT: Comprehensive regression tests for ALL 8 cognitive substrate bridges
# WHY: Ensure performance, stability, and biological correctness preserved
# HOW: Benchmark throughput, memory leaks, numerical stability, scaling, correctness
#
# Bridges tested:
# - attention_substrate_bridge
# - emotion_substrate_bridge
# - executive_substrate_bridge
# - introspection_substrate_bridge
# - memory_consolidation_substrate_bridge
# - reasoning_substrate_bridge
# - tom_substrate_bridge (unified API)
# - working_memory_substrate_bridge
#
# Test categories (~34 tests total):
# 1. Performance Benchmarks (8 tests)
# 2. Memory Regression (8 tests)
# 3. Numerical Stability (5 tests)
# 4. Scaling Tests (3 tests)
# 5. Correctness Regression (8 tests)
# 6. Thread Safety (2 tests)

add_executable(regression_cognitive_substrate_bridges
    test_cognitive_substrate_bridges_regression.cpp
)

target_link_libraries(regression_cognitive_substrate_bridges
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_substrate_bridges
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(regression_cognitive_substrate_bridges PRIVATE cxx_std_17)

# Enable testing
add_test(NAME regression_cognitive_substrate_bridges
         COMMAND regression_cognitive_substrate_bridges)
set_tests_properties(regression_cognitive_substrate_bridges
    PROPERTIES
        LABELS "regression;cognitive;substrate;bridges;performance"
        TIMEOUT 300  # 5 minutes for comprehensive benchmarking
)
