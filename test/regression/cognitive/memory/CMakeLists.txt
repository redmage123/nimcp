# ============================================================================
# Memory Regression Tests
# ============================================================================
# WHAT: Regression tests for Working Memory, Engram, and Semantic Memory
# WHY:  Ensure numerical stability, memory safety, and performance bounds
# HOW:  GoogleTest-based tests covering edge cases, bounds, and performance

# ============================================================================
# Engram Backward Compatibility Regression Tests
# ============================================================================

add_executable(regression_cognitive_memory_engram_backward_compat
    test_engram_backward_compat.cpp
)

target_link_libraries(regression_cognitive_memory_engram_backward_compat
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_memory_engram_backward_compat
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_cognitive_memory_engram_backward_compat PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_memory_engram_backward_compat
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;memory;engram;backward_compat"
        TIMEOUT 180
)

# ============================================================================
# Semantic Memory Backward Compatibility Regression Tests
# ============================================================================

add_executable(regression_cognitive_memory_semantic_backward_compat
    test_semantic_memory_backward_compat.cpp
)

target_link_libraries(regression_cognitive_memory_semantic_backward_compat
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_memory_semantic_backward_compat
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_cognitive_memory_semantic_backward_compat PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_memory_semantic_backward_compat
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;memory;semantic;backward_compat"
        TIMEOUT 180
)

# ============================================================================
# Working Memory SNN-Plasticity Regression Tests
# ============================================================================
# WHAT: Regression tests for Working Memory SNN and Plasticity bridges
# WHY:  Ensure stability of memory maintenance, consolidation, capacity learning
# HOW:  Boundary tests, long-running stability, protected synapse integrity
# ============================================================================

add_executable(regression_cognitive_memory_snn_plasticity
    test_memory_snn_plasticity_regression.cpp
)

target_link_libraries(regression_cognitive_memory_snn_plasticity
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_memory_snn_plasticity
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_cognitive_memory_snn_plasticity PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_memory_snn_plasticity
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;memory;snn;plasticity"
        TIMEOUT 180
)

# ============================================================================
# PR Memory Exception Handling Regression Tests
# ============================================================================
# WHAT: Regression tests for PR memory system exception handling
# WHY:  Ensure exception handling behavior remains stable across versions
# HOW:  Test error codes, return values, and edge cases
# ============================================================================

add_executable(regression_cognitive_memory_pr_exception
    test_pr_memory_exception_regression.cpp
)

target_link_libraries(regression_cognitive_memory_pr_exception
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_memory_pr_exception
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_cognitive_memory_pr_exception PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_memory_pr_exception
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;memory;pr_memory;exception"
        TIMEOUT 180
)
