# ============================================================================
# RNG Module Integration Tests
# ============================================================================
# WHAT: Integration tests for nimcp_rand across system modules
# WHY:  Verify unified RNG works correctly with UMM, thread pool, tensor, training
# HOW:  Test cross-module RNG usage, thread-local storage, and reproducibility

add_executable(integration_rng_module
    test_rand_module_integration.cpp
)

target_include_directories(integration_rng_module PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(integration_rng_module
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)

target_compile_features(integration_rng_module PRIVATE cxx_std_17)

add_test(NAME integration_rng_module COMMAND integration_rng_module)
set_tests_properties(integration_rng_module PROPERTIES
    LABELS "integration;rng;utils"
    TIMEOUT 120
)
