# ============================================================================
# NIMCP Core Brain Layer End-to-End Tests
# ============================================================================
# WHAT: Build configuration for core brain layer E2E tests
# WHY:  Test complete brain pipelines including hemispheric and thalamic layers
# HOW:  Include subdirectories for each brain module
# ============================================================================

message(STATUS "")
message(STATUS "-- Configuring Core Brain Layer E2E Tests")

# Include Hemispheric Brain E2E tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/hemispheric/CMakeLists.txt)
    add_subdirectory(hemispheric)
endif()

# Include Brain Regions E2E tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/regions/CMakeLists.txt)
    add_subdirectory(regions)
endif()

# Include Thalamic Layer E2E tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/thalamic/CMakeLists.txt)
    add_subdirectory(thalamic)
endif()

# Include Subcortical E2E tests (basal ganglia pipeline)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/subcortical/CMakeLists.txt)
    add_subdirectory(subcortical)
endif()

# Include Language Processing E2E tests (Broca/Wernicke)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/language/CMakeLists.txt)
    add_subdirectory(language)
endif()

# Brain Cycle Coordinator E2E Test (Full system health tracking)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_brain_cycle_coordinator_e2e.cpp)
    add_executable(e2e_brain_cycle_coordinator
        test_brain_cycle_coordinator_e2e.cpp
    )
    target_link_libraries(e2e_brain_cycle_coordinator
        PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
    )
    target_include_directories(e2e_brain_cycle_coordinator
        PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
    )
    gtest_discover_tests(e2e_brain_cycle_coordinator
        PROPERTIES LABELS "e2e;core;brain;cycle_coordinator")
endif()

# ============================================================================
# Brain Lifecycle E2E Test (GTest C++)
# ============================================================================
# WHAT: End-to-end tests for complete brain lifecycle
# WHY:  Verify brain creation, initialization, operation, persistence, shutdown
# HOW:  Tests full brain lifecycle with error recovery and KG integration
#
# TESTS:
# 1. CompleteBrainCreationDestruction - Create, init, run, stop, destroy
# 2. BrainCreationDifferentSizes - Size variants (TINY, SMALL, MEDIUM)
# 3. BrainCreationDifferentTasks - Task types (classification, regression, etc.)
# 4. ModuleLazyInitialization - Lazy init of subsystems
# 5. BrainOperationInference - Run inference cycles
# 6. BrainOperationStress - Stress test with 100 iterations
# 7. BrainStatePersistence - Save/restore brain state
# 8. GracefulShutdownDuringOperations - Shutdown during active ops
# 9. MultipleCreateDestroyCycles - Memory leak detection
# 10. KGIntegrationLifecycle - Full KG workflow
# 11. CompleteLifecyclePipeline - Complete pipeline test
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_brain_lifecycle_e2e.cpp)
    add_executable(e2e_test_brain_lifecycle
        ${CMAKE_CURRENT_SOURCE_DIR}/test_brain_lifecycle_e2e.cpp
        ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
    )
    target_include_directories(e2e_test_brain_lifecycle PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/test/e2e
        ${CMAKE_SOURCE_DIR}/src/lib
    )
    target_link_libraries(e2e_test_brain_lifecycle PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_definitions(e2e_test_brain_lifecycle PRIVATE
        NIMCP_TESTING
        NIMCP_E2E_TESTING
    )
    gtest_discover_tests(e2e_test_brain_lifecycle
        PROPERTIES LABELS "e2e;core;brain;lifecycle"
    )
    message(STATUS "    Added: e2e_test_brain_lifecycle (GTest)")
endif()

# ============================================================================
# Knowledge Graph Full E2E Test (GTest C++)
# ============================================================================
# WHAT: End-to-end tests for complete KG workflow
# WHY:  Verify KG CRUD operations, traversal, persistence, and security
# HOW:  Tests full KG pipeline with stress testing and security validation
#
# TESTS:
# 1. CompleteKGWorkflow - Basic CRUD workflow
# 2. KGNodeTypes - Node type categorization
# 3. MultiModuleRegistration - Module node registration
# 4. ModuleMetadata - Metadata handling
# 5. KGPathFinding - Path finding and traversal
# 6. KGNeighborQueries - Neighbor query operations
# 7. KGSearchQueries - Search pattern matching
# 8. KGStressLoad - 100 nodes, 500 edges performance test
# 9. KGRapidUpdates - Rapid state updates
# 10. KGSecurityFeatures - Security token and integrity
# 11. KGEmergencyLock - Emergency lock/unlock
# 12. KGMessageHandlerIndex - Message handler index operations
# 13. CompleteKGPipeline - Complete integrated pipeline
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_kg_full_e2e.cpp)
    add_executable(e2e_test_kg_full
        ${CMAKE_CURRENT_SOURCE_DIR}/test_kg_full_e2e.cpp
        ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
    )
    target_include_directories(e2e_test_kg_full PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/test/e2e
        ${CMAKE_SOURCE_DIR}/src/lib
    )
    target_link_libraries(e2e_test_kg_full PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_definitions(e2e_test_kg_full PRIVATE
        NIMCP_TESTING
        NIMCP_E2E_TESTING
    )
    gtest_discover_tests(e2e_test_kg_full
        PROPERTIES LABELS "e2e;core;brain;kg"
    )
    message(STATUS "    Added: e2e_test_kg_full (GTest)")
endif()

# ============================================================================
# Memory Footprint Optimization E2E Test (All 8 Opts)
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_memory_footprint_e2e.cpp)
    add_executable(e2e_test_memory_footprint
        ${CMAKE_CURRENT_SOURCE_DIR}/test_memory_footprint_e2e.cpp
    )
    target_include_directories(e2e_test_memory_footprint PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(e2e_test_memory_footprint PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_definitions(e2e_test_memory_footprint PRIVATE
        NIMCP_TESTING
        NIMCP_E2E_TESTING
    )
    gtest_discover_tests(e2e_test_memory_footprint
        PROPERTIES LABELS "e2e;core;brain;memory_footprint"
        TIMEOUT 300
    )
    message(STATUS "    Added: e2e_test_memory_footprint (GTest)")
endif()
