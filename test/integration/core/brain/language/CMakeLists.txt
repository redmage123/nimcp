# ============================================================================
# Language Region Integration Tests (Broca and Wernicke)
# ============================================================================
#
# WHAT: Integration tests for Broca's and Wernicke's language regions
# WHY:  Verify language production, comprehension, and bidirectional communication
# HOW:  Test adapters, sub-modules, and bridge integration
#

# Build a single executable for all language region integration tests
add_executable(integration_language_regions
    test_broca_integration.cpp
    test_wernicke_integration.cpp
    test_wernicke_broca_bridge_integration.cpp
)

target_link_libraries(integration_language_regions
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_language_regions
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

# Set C++ standard
set_target_properties(integration_language_regions PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Discover tests
gtest_discover_tests(integration_language_regions
    PROPERTIES
        LABELS "integration;brain;language"
        TIMEOUT 180
)

# ============================================================================
# Individual test executables (optional, for parallel execution)
# ============================================================================

# Broca Integration Tests
add_executable(integration_broca_test
    test_broca_integration.cpp
)

target_link_libraries(integration_broca_test
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_broca_test
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

set_target_properties(integration_broca_test PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

gtest_discover_tests(integration_broca_test
    PROPERTIES
        LABELS "integration;brain;language;broca"
        TIMEOUT 120
)

# Wernicke Integration Tests
add_executable(integration_wernicke_test
    test_wernicke_integration.cpp
)

target_link_libraries(integration_wernicke_test
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_wernicke_test
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

set_target_properties(integration_wernicke_test PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

gtest_discover_tests(integration_wernicke_test
    PROPERTIES
        LABELS "integration;brain;language;wernicke"
        TIMEOUT 120
)

# Wernicke-Broca Bridge Integration Tests
add_executable(integration_wernicke_broca_bridge_test
    test_wernicke_broca_bridge_integration.cpp
)

target_link_libraries(integration_wernicke_broca_bridge_test
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_wernicke_broca_bridge_test
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

set_target_properties(integration_wernicke_broca_bridge_test PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

gtest_discover_tests(integration_wernicke_broca_bridge_test
    PROPERTIES
        LABELS "integration;brain;language;bridge"
        TIMEOUT 120
)

