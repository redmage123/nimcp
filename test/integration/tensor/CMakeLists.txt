# ============================================================================
# Tensor Operations Integration Tests
# ============================================================================
# WHAT: Integration tests for tensor operations, autodiff, and SIMD
# WHY:  Verify tensor operations chain correctly and gradients work
# HOW:  Test operation chains, broadcasting, and numerical stability

# Guard: Check GTest availability
if(NOT GTest_FOUND)
    message(STATUS "GTest not found - skipping tensor integration tests")
    return()
endif()

# Tensor Operations Integration Test
add_executable(integration_tensor_operations
    test_tensor_operations_integration.cpp
)

target_link_libraries(integration_tensor_operations
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_tensor_operations
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(integration_tensor_operations PRIVATE cxx_std_17)

gtest_discover_tests(integration_tensor_operations
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;tensor;autodiff"
        TIMEOUT 180
)

message(STATUS "  Added tensor operations integration test")
