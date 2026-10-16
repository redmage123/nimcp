# ============================================================================
# Parietal Lobe FEP Orchestrator Bridge Regression Tests
# ============================================================================
# WHAT: Build configuration for parietal-FEP orchestrator bridge regression tests
# WHY:  Verify stability, performance, and numerical accuracy of parietal-FEP orchestrator integration
# HOW:  Single executable with all regression tests for long-running validation
#
# NOTE: This tests the parietal_fep_bridge (FEP orchestrator integration)
#       The older fep_parietal_bridge tests are in test/regression/parietal/

# Financial Cognitive Regression Tests (Check Framework)
add_subdirectory(financial)

# Parietal FEP Orchestrator Bridge Regression Tests
add_executable(regression_parietal_fep_orchestrator_bridge
    test_parietal_fep_regression.cpp
)

target_link_libraries(regression_parietal_fep_orchestrator_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_parietal_fep_orchestrator_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_parietal_fep_orchestrator_bridge PRIVATE cxx_std_17)
target_compile_definitions(regression_parietal_fep_orchestrator_bridge PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_parietal_fep_orchestrator_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;parietal;fep;orchestrator;bridge"
        TIMEOUT 600
)

# ============================================================================
# Genius Bridges Exception Handling Regression Tests
# ============================================================================
# WHAT: Verify exception handling behavior remains consistent across versions
# WHY:  Prevent regressions in error codes, messages, and handler behavior
# HOW:  Check exact error codes, message patterns, and function context

add_executable(regression_genius_bridges_exception
    test_genius_bridges_exception_regression.cpp
)

target_link_libraries(regression_genius_bridges_exception
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_genius_bridges_exception
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_genius_bridges_exception PRIVATE cxx_std_17)
target_compile_definitions(regression_genius_bridges_exception PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_genius_bridges_exception
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;parietal;genius;exception-handling"
        TIMEOUT 300
)

# ============================================================================
# Parietal Core Exception Handling Regression Tests
# ============================================================================
# WHAT: Verify API contract stability for exception handling in parietal modules
# WHY:  Ensure error codes, return values, and exception messages remain stable
# HOW:  Test all exception-throwing functions for consistent behavior

add_executable(regression_parietal_exception
    test_parietal_exception_regression.cpp
)

target_link_libraries(regression_parietal_exception
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_parietal_exception
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_parietal_exception PRIVATE cxx_std_17)
target_compile_definitions(regression_parietal_exception PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_parietal_exception
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;parietal;exception-handling"
        TIMEOUT 300
)

# ============================================================================
# Financial Investment Module Regression Tests
# ============================================================================
# WHAT: Verify API contract stability, NULL safety, output bounds, error codes
# WHY:  Prevent regressions in financial module correctness and safety
# HOW:  Test all public functions for consistent behavior across versions

add_executable(regression_parietal_financial_investment
    test_financial_regression.cpp
)

target_link_libraries(regression_parietal_financial_investment
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_parietal_financial_investment
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_parietal_financial_investment PRIVATE cxx_std_17)
target_compile_definitions(regression_parietal_financial_investment PRIVATE NIMCP_TESTING)

gtest_discover_tests(regression_parietal_financial_investment
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;cognitive;parietal;financial"
        TIMEOUT 120
)

# DISABLED: Test needs API updates to match current parietal training domain/state enums
# TODO: Update PARIETAL_DOMAIN_* and PARIETAL_TRAIN_STATE_* enum values
#
# # ============================================================================
# # Parietal Training Regression Tests
# # ============================================================================
# # WHAT: Verify training function behavior remains consistent across versions
# # WHY:  Prevent regressions in training infrastructure
# # HOW:  Test training function patterns and behaviors
#
# add_executable(regression_parietal_training
#     test_parietal_training_regression.cpp
# )
#
# target_link_libraries(regression_parietal_training
#     PRIVATE
#         nimcp
#         GTest::gtest
#         GTest::gtest_main
#         Threads::Threads
#         m
# )
#
# target_include_directories(regression_parietal_training
#     PRIVATE
#         ${PROJECT_SOURCE_DIR}/include
#         ${PROJECT_SOURCE_DIR}/src
# )
#
# target_compile_features(regression_parietal_training PRIVATE cxx_std_17)
# target_compile_definitions(regression_parietal_training PRIVATE NIMCP_TESTING)
#
# gtest_discover_tests(regression_parietal_training
#     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#     PROPERTIES
#         LABELS "regression;cognitive;parietal;training"
#         TIMEOUT 120
# )
