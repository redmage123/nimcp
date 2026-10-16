# =============================================================================
# End-to-End Tests for Plasticity Module Exception Handling
# =============================================================================
# WHAT: CMakeLists.txt for plasticity module exception e2e tests
# WHY:  Test full plasticity pipeline with exception recovery
# HOW:  Build test executable and register with CTest
#
# E2E SCENARIOS:
# - Full learning cycle with transient neuromodulator failures
# - Attention-modulated STDP with allocation failures
# - Long-running simulation with random exception injection
# - Multi-threaded plasticity with concurrent exceptions
# - System recovery and state preservation under stress
#
# @author NIMCP Development Team
# @date 2026-01-25
# =============================================================================

# Plasticity Module Exception E2E Tests
add_executable(e2e_plasticity_module_exception
    test_plasticity_module_exception_e2e.cpp
)

target_include_directories(e2e_plasticity_module_exception
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(e2e_plasticity_module_exception
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(e2e_plasticity_module_exception PRIVATE cxx_std_17)

# Register with CTest using gtest_discover_tests for automatic test discovery
gtest_discover_tests(e2e_plasticity_module_exception
    PROPERTIES
        TIMEOUT 600
        LABELS "e2e;plasticity;exception;neuromodulators;attention;stdp;immune"
)
