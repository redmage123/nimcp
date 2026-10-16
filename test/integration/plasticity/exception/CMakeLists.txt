# =============================================================================
# Integration Tests for Plasticity Exception Handling
# =============================================================================
# WHAT: CMakeLists.txt for plasticity exception integration tests
# WHY:  Test end-to-end exception flow with immune system integration
# HOW:  Build test executable and register with CTest
#
# INTEGRATION SCENARIOS:
# - Plasticity error -> Exception -> Handler -> Immune presentation
# - Batch exception processing
# - Cross-module exception propagation
# - Concurrent exception handling
# - Recovery strategy execution
#
# @author NIMCP Development Team
# @date 2026-01-16
# =============================================================================

# Plasticity Exception Integration Tests
add_executable(integration_plasticity_exception
    test_plasticity_exception_integration.cpp
)

target_include_directories(integration_plasticity_exception
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(integration_plasticity_exception
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(integration_plasticity_exception PRIVATE cxx_std_17)

# Register with CTest using gtest_discover_tests for automatic test discovery
gtest_discover_tests(integration_plasticity_exception
    PROPERTIES
        TIMEOUT 300
        LABELS "integration;plasticity;exception;immune"
)

# Alternative manual test registration (if gtest_discover_tests has issues)
# add_test(
#     NAME integration_plasticity_exception
#     COMMAND integration_plasticity_exception --gtest_brief=1
# )
#
# set_tests_properties(integration_plasticity_exception PROPERTIES
#     TIMEOUT 300
#     LABELS "integration;plasticity;exception;immune"
# )

# =============================================================================
# Plasticity Module Exception Integration Tests
# =============================================================================
# WHAT: Integration tests for NIMCP_THROW_TO_IMMUNE cross-module exception handling
# WHY:  Test neuromodulator-STDP interaction and exception recovery
# HOW:  Test complete exception flow from plasticity modules to immune system
#
# @author NIMCP Development Team
# @date 2026-01-25
# =============================================================================

add_executable(integration_plasticity_module_exception
    test_plasticity_module_exception_integration.cpp
)

target_include_directories(integration_plasticity_module_exception
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(integration_plasticity_module_exception
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(integration_plasticity_module_exception PRIVATE cxx_std_17)

gtest_discover_tests(integration_plasticity_module_exception
    PROPERTIES
        TIMEOUT 300
        LABELS "integration;plasticity;exception;neuromodulators;stdp;immune"
)

# =============================================================================
# Multi-Attention Exception Integration Tests
# =============================================================================
# WHAT: Integration tests for multi-attention exception handling with immune system
# WHY:  Test end-to-end exception flow from attention modules to immune system
# HOW:  Test exception paths, recovery, concurrent handling, and stats
#
# @author NIMCP Development Team
# @date 2026-01-25
# =============================================================================

add_executable(integration_attention_exception
    test_attention_exception_integration.cpp
)

target_include_directories(integration_attention_exception
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(integration_attention_exception
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(integration_attention_exception PRIVATE cxx_std_17)

gtest_discover_tests(integration_attention_exception
    PROPERTIES
        TIMEOUT 300
        LABELS "integration;plasticity;attention;exception;immune"
)
