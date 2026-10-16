# ============================================================================
# Hemispheric Brain Integration Tests
# ============================================================================
# WHAT: Build configuration for hemispheric brain layer integration tests
# WHY:  Test interactions between hemispheres, corpus callosum, lateralization,
#       immune bridge, and FEP bridge
# HOW:  Single executable with all integration tests for parallel execution

add_executable(integration_hemispheric_brain
    test_hemispheric_brain_integration.cpp
    test_corpus_callosum_integration.cpp
    test_hemispheric_immune_bridge_integration.cpp
    test_hemispheric_fep_bridge_integration.cpp
)

target_link_libraries(integration_hemispheric_brain PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)

target_include_directories(integration_hemispheric_brain PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_hemispheric_brain PRIVATE cxx_std_17)
target_compile_definitions(integration_hemispheric_brain PRIVATE NIMCP_TESTING)

gtest_discover_tests(integration_hemispheric_brain
    PROPERTIES LABELS "integration;core;brain;hemispheric"
    TIMEOUT 180
)

# Hemispheric Memory Optimization Integration Tests (Opt 5+7)
add_executable(integration_hemispheric_memory_opt
    test_hemispheric_memory_opt_integration.cpp
)
target_link_libraries(integration_hemispheric_memory_opt PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)
target_include_directories(integration_hemispheric_memory_opt PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/test
)
target_compile_features(integration_hemispheric_memory_opt PRIVATE cxx_std_17)
target_compile_definitions(integration_hemispheric_memory_opt PRIVATE NIMCP_TESTING)
gtest_discover_tests(integration_hemispheric_memory_opt
    PROPERTIES LABELS "integration;core;brain;hemispheric;memory_opt"
    TIMEOUT 300
)
