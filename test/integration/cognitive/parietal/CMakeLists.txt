# ============================================================================
# Parietal Lobe Integration Tests
# Tests cross-module interactions within the Parietal subsystem
# ============================================================================

# Main Parietal Integration Test
add_executable(integration_cognitive_parietal
    test_parietal_integration.cpp
)

target_link_libraries(integration_cognitive_parietal
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_parietal
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(integration_cognitive_parietal PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_parietal
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;parietal"
        TIMEOUT 180
)

# ----------------------------------------------------------------------------
# Parietal + FEP Orchestrator Integration Test
# Tests integration between parietal lobe and FEP orchestrator
# ----------------------------------------------------------------------------
add_executable(integration_parietal_fep
    test_parietal_fep_integration.cpp
)

target_link_libraries(integration_parietal_fep
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_parietal_fep
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_parietal_fep PRIVATE cxx_std_17)

gtest_discover_tests(integration_parietal_fep
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;parietal;fep;orchestrator"
        TIMEOUT 240
)

# ----------------------------------------------------------------------------
# Parietal Training Bridge Integration Test
# Tests integration between parietal lobe and training system
# ----------------------------------------------------------------------------
add_executable(integration_parietal_training
    test_parietal_training_integration.cpp
)

target_link_libraries(integration_parietal_training
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_parietal_training
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(integration_parietal_training PRIVATE cxx_std_17)

gtest_discover_tests(integration_parietal_training
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;parietal;training"
        TIMEOUT 180
)

# ----------------------------------------------------------------------------
# Genius Bridges Exception Handling Integration Test
# Tests cross-bridge exception handling and propagation
# ----------------------------------------------------------------------------
add_executable(integration_genius_bridges_exception
    test_genius_bridges_exception_integration.cpp
)

target_link_libraries(integration_genius_bridges_exception
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_genius_bridges_exception
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(integration_genius_bridges_exception PRIVATE cxx_std_17)

gtest_discover_tests(integration_genius_bridges_exception
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;parietal;genius;exception-handling"
        TIMEOUT 180
)

# ----------------------------------------------------------------------------
# Parietal Core Exception Handling Integration Test
# Tests exception propagation from parietal modules to immune system
# ----------------------------------------------------------------------------
add_executable(integration_parietal_exception
    test_parietal_exception_integration.cpp
)

target_link_libraries(integration_parietal_exception
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_parietal_exception
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(integration_parietal_exception PRIVATE cxx_std_17)

gtest_discover_tests(integration_parietal_exception
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;parietal;exception-handling;immune"
        TIMEOUT 180
)

# ----------------------------------------------------------------------------
# Financial Investment Module Integration Test
# Tests cross-module interactions across all 5 financial sub-modules
# ----------------------------------------------------------------------------
add_executable(integration_parietal_financial_investment
    test_financial_integration.cpp
)

target_link_libraries(integration_parietal_financial_investment
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_parietal_financial_investment
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(integration_parietal_financial_investment PRIVATE cxx_std_17)

gtest_discover_tests(integration_parietal_financial_investment
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;parietal;financial"
        TIMEOUT 180
)

# Financial Cognitive Integration Tests (Check Framework)
add_subdirectory(financial)

# ----------------------------------------------------------------------------
# Linguistics Fuzzy & Statistics Integration Test (Phase 2)
# Tests fuzzy bridge and statistics bridge with mesh integration
# ----------------------------------------------------------------------------
add_executable(integration_linguistics_fuzzy_stats
    linguistics/test_linguistics_fuzzy_stats_integration.cpp
)

target_link_libraries(integration_linguistics_fuzzy_stats
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_linguistics_fuzzy_stats
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(integration_linguistics_fuzzy_stats PRIVATE cxx_std_17)

gtest_discover_tests(integration_linguistics_fuzzy_stats
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;parietal;linguistics;fuzzy;statistics"
        TIMEOUT 180
)
