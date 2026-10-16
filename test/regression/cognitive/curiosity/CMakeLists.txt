# ============================================================================
# Enhanced Curiosity Regression Tests
# ============================================================================
# WHAT: Regression tests for Enhanced Curiosity System
# WHY:  Ensure stability, performance, and correctness across updates
# HOW:  Stress tests, memory checks, thread safety, edge cases
# ============================================================================

add_executable(regression_cognitive_curiosity_enhanced
    test_curiosity_enhanced_regression.cpp
)

target_link_libraries(regression_cognitive_curiosity_enhanced
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_curiosity_enhanced
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_cognitive_curiosity_enhanced PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_curiosity_enhanced
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;curiosity;enhanced"
        TIMEOUT 300
)

# ============================================================================
# Information Forager Regression Tests
# ============================================================================
add_executable(regression_cognitive_information_forager
    test_information_forager_regression.cpp
)

target_link_libraries(regression_cognitive_information_forager
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_information_forager
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_cognitive_information_forager PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_information_forager
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;curiosity;forager"
        TIMEOUT 300
)

# ============================================================================
# Curiosity SNN-Plasticity Regression Tests
# ============================================================================
# WHAT: Regression tests for Curiosity SNN and Plasticity bridges
# WHY:  Ensure stability of novelty detection, exploration learning, synapse protection
# HOW:  Boundary tests, protected synapse integrity, long-running stability
# ============================================================================

add_executable(regression_cognitive_curiosity_snn_plasticity
    test_curiosity_snn_plasticity_regression.cpp
)

target_link_libraries(regression_cognitive_curiosity_snn_plasticity
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_curiosity_snn_plasticity
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_cognitive_curiosity_snn_plasticity PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_curiosity_snn_plasticity
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;curiosity;snn;plasticity"
        TIMEOUT 180
)

# ============================================================================
# Curiosity QMC Regression Tests (Step 10 MC Integration)
# ============================================================================
# WHAT: Regression tests for QMC-based uncertainty and empowerment in curiosity
# WHY:  Ensure stability, determinism, performance, memory safety
# HOW:  Stress tests, bounds checking, determinism verification, memory checks
# ============================================================================

add_executable(regression_cognitive_curiosity_qmc
    test_curiosity_qmc_regression.cpp
)

target_link_libraries(regression_cognitive_curiosity_qmc
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_cognitive_curiosity_qmc
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_cognitive_curiosity_qmc PRIVATE cxx_std_17)

gtest_discover_tests(regression_cognitive_curiosity_qmc
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;curiosity;qmc"
        TIMEOUT 300
)
