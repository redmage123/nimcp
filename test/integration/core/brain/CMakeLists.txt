# ============================================================================
# Brain Module Bio-Async Integration Tests
# ============================================================================
#
# WHAT: Integration tests for brain module bio-async communication
# WHY:  Verify brain creation, processing, and message routing via bio-async
# HOW:  Create brain instances, send messages, verify proper integration
#

# Test 1: Brain Bio-Async Integration (Cross-module communication)
add_executable(integration_brain_bio_async_test
    test_brain_bio_async_integration.cpp
)

target_link_libraries(integration_brain_bio_async_test
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)

target_include_directories(integration_brain_bio_async_test
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)

gtest_discover_tests(integration_brain_bio_async_test)

# Test 2: Brain Persistence Integration (Bio-async event notifications)
add_executable(integration_brain_persistence_test
    test_brain_persistence_integration.cpp
)

target_link_libraries(integration_brain_persistence_test
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)

target_include_directories(integration_brain_persistence_test
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)

gtest_discover_tests(integration_brain_persistence_test)

# Test 3: Brain Coordinator Integration (All 6 coordinators working together)
add_executable(integration_brain_coordinator_test
    test_brain_coordinator_integration.cpp
)

# Test 3b: Substrate + Thalamic Router Wire-up (Phase 1-4 activation)
# Verifies the brain init wave wires neural_substrate_t and thalamic_router_t
# into SNN/LNN/cortex CNN networks via the *_attach_substrate /
# *_attach_thalamic_router functions. Guards against regressing to the
# dormant state where 95 attach callers existed only in test code.
add_executable(integration_brain_substrate_thalamic_wireup_test
    test_brain_substrate_thalamic_wireup.cpp
)

target_link_libraries(integration_brain_substrate_thalamic_wireup_test
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)

target_include_directories(integration_brain_substrate_thalamic_wireup_test
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)

gtest_discover_tests(integration_brain_substrate_thalamic_wireup_test)

target_link_libraries(integration_brain_coordinator_test
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)

target_include_directories(integration_brain_coordinator_test
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)

gtest_discover_tests(integration_brain_coordinator_test)

# Add subcortical subdirectory
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/subcortical")
    add_subdirectory(subcortical)
endif()

# Test 4: KG Hierarchy Graph Algorithms Integration
add_executable(integration_kg_hierarchy_graph_algorithms_test
    test_kg_hierarchy_graph_algorithms_integration.cpp
)

target_link_libraries(integration_kg_hierarchy_graph_algorithms_test
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)

target_include_directories(integration_kg_hierarchy_graph_algorithms_test
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)

gtest_discover_tests(integration_kg_hierarchy_graph_algorithms_test)

# Add regions subdirectory (hypothalamus, broca, etc.)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regions")
    add_subdirectory(regions)
endif()

# Add hemispheric subdirectory (bilateral brain, corpus callosum)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/hemispheric")
    add_subdirectory(hemispheric)
endif()

# Add thalamic subdirectory (thalamus, thalamic router, thalamic-immune bridge)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thalamic")
    add_subdirectory(thalamic)
endif()

# Add language subdirectory (Broca, Wernicke, Wernicke-Broca bridge)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/language")
    add_subdirectory(language)
endif()

# Test 5: Brain Cycle Coordinator Integration (Unified cycle observability)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_brain_cycle_coordinator_integration.cpp")
    add_executable(integration_brain_cycle_coordinator_test
        test_brain_cycle_coordinator_integration.cpp
    )
    target_link_libraries(integration_brain_cycle_coordinator_test
        PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
    )
    target_include_directories(integration_brain_cycle_coordinator_test
        PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
    )
    gtest_discover_tests(integration_brain_cycle_coordinator_test)
endif()
