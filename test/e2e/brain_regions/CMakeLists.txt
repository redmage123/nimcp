# ============================================================================
# Brain Regions KG Wiring E2E Tests
# ============================================================================
# WHAT: End-to-end tests for brain region KG wiring
# WHY:  Verify complete brain initialization, state updates, cross-region
#       coordination, and graceful shutdown workflows
# HOW:  GTest-based tests for realistic brain simulation scenarios
#
# TESTS:
# 1. FullBrainLifecycle - Brain initialization and shutdown
# 2. MemoryFormationSimulation - Hippocampus memory encoding workflow
# 3. FearResponseSimulation - Amygdala fear response workflow
# 4. ExecutiveControlSimulation - PFC executive control workflow
# 5. EmotionalMemoryCoordination - Cross-region emotional memory
# 6. StressResilience - Rapid state update stress test
# 7. QueryFunctionality - Node and subsystem queries
# ============================================================================

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_kg_wiring_e2e.cpp)
    add_executable(e2e_test_kg_wiring
        ${CMAKE_CURRENT_SOURCE_DIR}/test_kg_wiring_e2e.cpp
        ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
    )
    target_include_directories(e2e_test_kg_wiring PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/test/e2e
        ${CMAKE_SOURCE_DIR}/src/lib
    )
    target_link_libraries(e2e_test_kg_wiring PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_kg_wiring PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_kg_wiring PRIVATE
        NIMCP_TESTING
        NIMCP_E2E_TESTING
    )
    add_test(NAME e2e_test_kg_wiring COMMAND e2e_test_kg_wiring)
    set_tests_properties(e2e_test_kg_wiring PROPERTIES
        LABELS "e2e;brain_regions;kg;wiring;hippocampus;amygdala;pfc"
        TIMEOUT 300
    )
    message(STATUS "    Added: e2e_test_kg_wiring (GTest C++)")
endif()
