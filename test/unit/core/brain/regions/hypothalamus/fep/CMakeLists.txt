# ============================================================================
# Hypothalamus FEP Bridge Unit Tests
# ============================================================================
# WHAT: Unit tests for Hypothalamus FEP bridges
# WHY:  Ensure correctness of FEP-Hypothalamus bidirectional integration
# HOW:  GoogleTest-based tests covering all API functions

# ============================================================================
# Helper macro to add Hypothalamus FEP bridge tests
# ============================================================================

macro(add_hypo_fep_test TEST_NAME)
    add_executable(unit_core_brain_regions_hypothalamus_fep_${TEST_NAME}
        ${TEST_NAME}.cpp
    )

    target_link_libraries(unit_core_brain_regions_hypothalamus_fep_${TEST_NAME}
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
            m
    )

    target_include_directories(unit_core_brain_regions_hypothalamus_fep_${TEST_NAME}
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src
    )

    target_compile_features(unit_core_brain_regions_hypothalamus_fep_${TEST_NAME} PRIVATE cxx_std_17)

    gtest_discover_tests(unit_core_brain_regions_hypothalamus_fep_${TEST_NAME}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;core;brain;hypothalamus;fep"
            TIMEOUT 60
    )
endmacro()

# ============================================================================
# Bio-Async FEP Bridge Tests
# ============================================================================

add_hypo_fep_test(test_hypo_bio_async_fep_bridge)

# ============================================================================
# Salience FEP Bridge Tests
# ============================================================================

add_hypo_fep_test(test_hypo_salience_fep_bridge)

# ============================================================================
# Curiosity FEP Bridge Tests
# ============================================================================

add_hypo_fep_test(test_hypo_curiosity_fep_bridge)

# ============================================================================
# Reasoning FEP Bridge Tests
# ============================================================================

add_hypo_fep_test(test_hypo_reasoning_fep_bridge)

# ============================================================================
# Global Workspace FEP Bridge Tests
# ============================================================================

add_hypo_fep_test(test_hypo_global_workspace_fep_bridge)
