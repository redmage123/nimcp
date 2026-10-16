# ============================================================================
# Language Layer Integration Tests
# ============================================================================
# WHAT: Integration tests for Language Layer
# WHY:  Verify end-to-end language processing flows and bridge coordination
# HOW:  gtest-based tests for full pipelines and multi-bridge scenarios
# ============================================================================

# ============================================================================
# Language Integration Tests
# ============================================================================
add_executable(integration_language
    test_language_integration.cpp
)
target_link_libraries(integration_language
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(integration_language PRIVATE cxx_std_17)
gtest_discover_tests(integration_language
    PROPERTIES LABELS "integration;language")

# ============================================================================
# Grounded Language Brain Integration Tests
# ============================================================================
add_executable(integration_grounded_language_brain
    test_grounded_language_brain_integration.cpp
)
target_link_libraries(integration_grounded_language_brain
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(integration_grounded_language_brain
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(integration_grounded_language_brain PRIVATE cxx_std_17)
gtest_discover_tests(integration_grounded_language_brain
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;language;grounded"
        TIMEOUT 300
)

# ============================================================================
# Grounded Language Collapse-Fix Integration Tests
# ============================================================================
add_executable(integration_grounded_language_collapse_fix
    test_grounded_language_collapse_fix_integration.cpp
)
target_link_libraries(integration_grounded_language_collapse_fix
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(integration_grounded_language_collapse_fix
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(integration_grounded_language_collapse_fix PRIVATE cxx_std_17)
gtest_discover_tests(integration_grounded_language_collapse_fix
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;language;grounded;collapse"
        TIMEOUT 300
)

# ============================================================================
# Bootstrap Lexicon Integration Tests (Option C)
# ============================================================================
add_executable(integration_bootstrap_lexicon
    test_bootstrap_lexicon_integration.cpp
)
target_link_libraries(integration_bootstrap_lexicon
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(integration_bootstrap_lexicon
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(integration_bootstrap_lexicon PRIVATE cxx_std_17)
gtest_discover_tests(integration_bootstrap_lexicon
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;language;grounded;bootstrap"
        TIMEOUT 300
)

