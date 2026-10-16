# ============================================================================
# Brain Regression Tests
# ============================================================================
# WHAT: Regression tests for brain knowledge graph module wiring
# WHY:  Catch regressions in module wiring, topology, and KG integration
# HOW:  Test edge cases, error handling, and stress scenarios
# ============================================================================

# Guard: Check GTest availability
if(NOT GTest_FOUND)
    message(STATUS "GTest not found - skipping Brain Regression tests")
    return()
endif()

# ============================================================================
# KG Module Wiring Regression Test
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_kg_module_wiring_regression.cpp")
    message(STATUS "Adding KG module wiring Regression tests...")
    add_executable(regression_test_kg_module_wiring
        ${CMAKE_CURRENT_SOURCE_DIR}/test_kg_module_wiring_regression.cpp
    )

    target_link_libraries(regression_test_kg_module_wiring
        PRIVATE
            nimcp
            GTest::GTest
            GTest::Main
            Threads::Threads
            m
    )

    target_include_directories(regression_test_kg_module_wiring
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
    )

    target_compile_features(regression_test_kg_module_wiring PRIVATE cxx_std_17)

    target_compile_definitions(regression_test_kg_module_wiring PRIVATE
        NIMCP_TESTING
    )

    add_test(NAME regression_test_kg_module_wiring COMMAND regression_test_kg_module_wiring)

    set_tests_properties(regression_test_kg_module_wiring PROPERTIES
        LABELS "regression;brain;kg;wiring"
        TIMEOUT 60
    )

    message(STATUS "  Added Regression test: regression_test_kg_module_wiring")
endif()

# ============================================================================
# Brain State Manager Regression Test (Phase 8)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_brain_state_manager_regression.cpp")
    message(STATUS "Adding Brain State Manager Regression tests...")
    add_executable(regression_test_brain_state_manager
        ${CMAKE_CURRENT_SOURCE_DIR}/test_brain_state_manager_regression.cpp
    )

    target_link_libraries(regression_test_brain_state_manager
        PRIVATE
            nimcp
            GTest::GTest
            GTest::Main
            Threads::Threads
            m
    )

    target_include_directories(regression_test_brain_state_manager
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
    )

    target_compile_features(regression_test_brain_state_manager PRIVATE cxx_std_17)

    target_compile_definitions(regression_test_brain_state_manager PRIVATE
        NIMCP_TESTING
    )

    add_test(NAME regression_test_brain_state_manager COMMAND regression_test_brain_state_manager)

    set_tests_properties(regression_test_brain_state_manager PROPERTIES
        LABELS "regression;brain;state_manager;phase8"
        TIMEOUT 120
    )

    message(STATUS "  Added Regression test: regression_test_brain_state_manager")
endif()
