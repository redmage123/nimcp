# ============================================================================
# Cognitive Health Integration Unit Tests
# ============================================================================
# WHAT: Unit tests for Phase 8 Cognitive Health Integration
# WHY:  Validate collective, RCOG, meta-health, and cognitive bridge components
# HOW:  Test lifecycle, API functions, null handling, edge cases
# ============================================================================

# ============================================================================
# Test: Collective Health Monitoring
# ============================================================================
add_executable(unit_cognitive_test_collective_health
    test_nimcp_collective_health.cpp
)

target_link_libraries(unit_cognitive_test_collective_health
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_collective_health
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_collective_health PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_collective_health
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;health;collective"
        TIMEOUT 120
)

# ============================================================================
# Test: RCOG Health Integration
# ============================================================================
add_executable(unit_cognitive_test_rcog_health
    test_nimcp_rcog_health.cpp
)

target_link_libraries(unit_cognitive_test_rcog_health
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_rcog_health
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_rcog_health PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_rcog_health
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;health;rcog"
        TIMEOUT 120
)

# ============================================================================
# Test: Meta-Health Self-Reflection
# ============================================================================
add_executable(unit_cognitive_test_meta_health
    test_nimcp_meta_health.cpp
)

target_link_libraries(unit_cognitive_test_meta_health
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_meta_health
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_meta_health PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_meta_health
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;health;meta"
        TIMEOUT 120
)

# ============================================================================
# Test: Health Cognitive Bridge
# ============================================================================
add_executable(unit_cognitive_test_health_cognitive_bridge
    test_nimcp_health_cognitive_bridge.cpp
)

target_link_libraries(unit_cognitive_test_health_cognitive_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_health_cognitive_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_health_cognitive_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_health_cognitive_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;health;bridge"
        TIMEOUT 120
)
