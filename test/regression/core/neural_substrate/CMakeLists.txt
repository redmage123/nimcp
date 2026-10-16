# CMakeLists.txt for Neural Substrate Regression Tests

# Enable testing
enable_testing()

# Find required packages
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/test
    ${GTEST_INCLUDE_DIRS}
)

# =============================================================================
# Neural Substrate Core Regression Tests (~20 tests)
# Performance, determinism, state consistency, memory, null safety, backward compat
# =============================================================================
add_executable(regression_neural_substrate
    test_neural_substrate_regression.cpp
)

target_link_libraries(regression_neural_substrate
    nimcp
    GTest::GTest
    GTest::Main
    Threads::Threads
)

add_test(
    NAME regression_neural_substrate
    COMMAND regression_neural_substrate --gtest_brief=1
)

set_tests_properties(regression_neural_substrate PROPERTIES
    TIMEOUT 180
    LABELS "regression;core;neural_substrate"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# =============================================================================
# Substrate-Immune Bridge Regression Tests (~15 tests)
# Modulation accuracy, state propagation, recovery rates, statistics, stress
# =============================================================================
add_executable(regression_substrate_immune_bridge
    test_substrate_immune_bridge_regression.cpp
)

target_link_libraries(regression_substrate_immune_bridge
    nimcp
    GTest::GTest
    GTest::Main
    Threads::Threads
)

add_test(
    NAME regression_substrate_immune_bridge
    COMMAND regression_substrate_immune_bridge --gtest_brief=1
)

set_tests_properties(regression_substrate_immune_bridge PROPERTIES
    TIMEOUT 240
    LABELS "regression;core;neural_substrate;immune;bridge"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# =============================================================================
# Substrate GPU Regression Tests (~15 tests)
# GPU performance, CPU fallback, memory accuracy, batch processing, cleanup
# =============================================================================
add_executable(regression_substrate_gpu
    test_substrate_gpu_regression.cpp
)

target_link_libraries(regression_substrate_gpu
    nimcp
    GTest::GTest
    GTest::Main
    Threads::Threads
)

add_test(
    NAME regression_substrate_gpu
    COMMAND regression_substrate_gpu --gtest_brief=1
)

set_tests_properties(regression_substrate_gpu PROPERTIES
    TIMEOUT 300
    LABELS "regression;core;neural_substrate;gpu"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# =============================================================================
# Summary
# =============================================================================
message(STATUS "Configured neural substrate regression tests:")
message(STATUS "  - regression_neural_substrate (~20 tests, 180s timeout)")
message(STATUS "  - regression_substrate_immune_bridge (~15 tests, 240s timeout)")
message(STATUS "  - regression_substrate_gpu (~15 tests, 300s timeout)")
