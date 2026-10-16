# ============================================================================
# Core Directives Integration Tests
# ============================================================================
#
# WHAT: Integration tests for Core Directives module interactions
# WHY:  Verify Asimov's Laws, Golden Rule, and combinatorial harm detection
#       work correctly across module boundaries
# HOW:  Test cross-module behavior with realistic scenarios
#

# Test: Core Directives Integration (Command Compliance + Ethics + History)
add_executable(integration_core_directives
    integration_core_directives.cpp
)

target_link_libraries(integration_core_directives
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)

target_include_directories(integration_core_directives
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_core_directives PRIVATE cxx_std_17)

# Register test with CTest
add_test(NAME integration_core_directives
         COMMAND integration_core_directives --gtest_brief=1)

set_tests_properties(integration_core_directives
    PROPERTIES
        TIMEOUT 120
        LABELS "integration;core;directives;ethics;asimov"
)
