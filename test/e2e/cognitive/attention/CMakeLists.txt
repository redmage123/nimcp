# =============================================================================
# E2E Tests for Multi-Attention Exception Handling
# =============================================================================
# WHAT: End-to-end tests for attention exception handling workflow
# WHY:  Verify complete exception-immune-recovery pipeline
# HOW:  Test realistic scenarios with brain context and immune integration
#
# E2E SCENARIOS:
# - Complete attention creation failure -> immune response -> recovery
# - Forward pass error -> logging -> graceful degradation
# - Concurrent attention operations with exceptions
# - Long-running attention pipeline with occasional failures
# - Brain-integrated attention exception flow
#
# @author NIMCP Development Team
# @date 2026-01-25
# =============================================================================

add_executable(e2e_attention_exception
    test_attention_exception_e2e.cpp
)

target_include_directories(e2e_attention_exception
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/lib
)

target_link_libraries(e2e_attention_exception
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(e2e_attention_exception PRIVATE cxx_std_17)

target_compile_definitions(e2e_attention_exception PRIVATE
    NIMCP_TESTING
    NIMCP_E2E_TESTING
)

gtest_discover_tests(e2e_attention_exception
    PROPERTIES
        TIMEOUT 600
        LABELS "e2e;cognitive;attention;exception"
)
