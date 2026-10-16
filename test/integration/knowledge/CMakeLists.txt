# ============================================================================
# Knowledge Graph Integration Tests
# ============================================================================
# WHAT: Integration tests for subcortical KG wiring with brain KG
# WHY:  Validates knowledge graph integration across brain modules
# HOW:  Test queries across wired modules, KG relationship traversal
# ============================================================================

add_executable(test_kg_wiring_integration
    test_kg_wiring_integration.cpp
)

target_include_directories(test_kg_wiring_integration
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_kg_wiring_integration
    PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)

target_compile_features(test_kg_wiring_integration PRIVATE cxx_std_17)

add_test(NAME KGWiringIntegration
    COMMAND test_kg_wiring_integration
)

set_tests_properties(KGWiringIntegration PROPERTIES
    TIMEOUT 300
    LABELS "integration;knowledge;kg;wiring"
)
