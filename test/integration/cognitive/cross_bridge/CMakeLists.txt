# ============================================================================
# Cross-Bridge Integration Tests
# ============================================================================
#
# Tests for cross-bridge integration scenarios:
# - Thalamic bridge to substrate bridge communication
# - Multiple cognitive thalamic bridges working together
# - Bio-async message flow between bridge types
# - Metabolic state cascading through all bridges
#
# ============================================================================

# ----------------------------------------------------------------------------
# Thalamic-Substrate Integration Tests
# ----------------------------------------------------------------------------
add_executable(integration_thalamic_substrate_bridges
    test_thalamic_substrate_integration.cpp
)

target_link_libraries(integration_thalamic_substrate_bridges
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_thalamic_substrate_bridges
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_thalamic_substrate_bridges PRIVATE cxx_std_17)

gtest_discover_tests(integration_thalamic_substrate_bridges
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;cross_bridge;thalamic;substrate"
        TIMEOUT 180
)

# ----------------------------------------------------------------------------
# Multi-Thalamic Bridge Integration Tests
# ----------------------------------------------------------------------------
add_executable(integration_multi_thalamic_bridges
    test_multi_thalamic_bridge_integration.cpp
)

target_link_libraries(integration_multi_thalamic_bridges
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_multi_thalamic_bridges
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_multi_thalamic_bridges PRIVATE cxx_std_17)

gtest_discover_tests(integration_multi_thalamic_bridges
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;cross_bridge;thalamic;multi_module"
        TIMEOUT 180
)

# ----------------------------------------------------------------------------
# Bio-Async Cross-Bridge Integration Tests
# ----------------------------------------------------------------------------
add_executable(integration_bio_async_cross_bridge
    test_bio_async_cross_bridge_integration.cpp
)

target_link_libraries(integration_bio_async_cross_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_bio_async_cross_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_bio_async_cross_bridge PRIVATE cxx_std_17)

gtest_discover_tests(integration_bio_async_cross_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;cross_bridge;bio_async"
        TIMEOUT 180
)

# ----------------------------------------------------------------------------
# Metabolic Cascade Integration Tests
# ----------------------------------------------------------------------------
# NOTE: ToM substrate bridge API now unified (cognitive/tom/ provides all functions)
add_executable(integration_metabolic_cascade
    test_metabolic_cascade_integration.cpp
)

target_link_libraries(integration_metabolic_cascade
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_metabolic_cascade
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(integration_metabolic_cascade PRIVATE cxx_std_17)

gtest_discover_tests(integration_metabolic_cascade
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;cross_bridge;metabolic;cascade"
        TIMEOUT 180
)
