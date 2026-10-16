# CMakeLists.txt for Neural Substrate Integration Tests
# Tests integration of neural substrate module components:
# - Neural substrate lifecycle and metabolic/physical state management
# - Substrate-immune bridge bidirectional integration
# - GPU-accelerated substrate operations

# Test executable: integration_neural_substrate
add_executable(integration_neural_substrate
    test_neural_substrate_integration.cpp
    test_substrate_immune_bridge_integration.cpp
    test_substrate_gpu_integration.cpp
)

# Include directories
target_include_directories(integration_neural_substrate PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/test
)

# Link libraries
target_link_libraries(integration_neural_substrate PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)

# Compiler flags
target_compile_options(integration_neural_substrate PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-unused-variable
)

# Add to test suite
add_test(
    NAME integration_neural_substrate
    COMMAND integration_neural_substrate --gtest_brief=1
)

# Test properties
set_tests_properties(integration_neural_substrate PROPERTIES
    TIMEOUT 180
    LABELS "integration;core;neural_substrate"
)
