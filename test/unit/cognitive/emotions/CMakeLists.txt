# Emotional System Unit Tests
# ============================================================================
# Tests for Emotional System Thread Safety and Core Functionality
# ============================================================================

# ============================================================================
# Emotional System Thread Safety Tests
# ============================================================================
# WHAT: Comprehensive thread safety tests for emotional system
# WHY:  Emotional system is accessed by multiple brain modules concurrently
# HOW:  Stress tests with multiple threads performing concurrent operations

add_executable(unit_cognitive_emotions_test_thread_safety
    test_emotional_thread_safety.cpp
)

target_link_libraries(unit_cognitive_emotions_test_thread_safety
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_emotions_test_thread_safety
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_emotions_test_thread_safety PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_emotions_test_thread_safety
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;emotions;thread_safety"
        TIMEOUT 180
)
