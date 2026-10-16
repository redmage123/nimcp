# ============================================================================
# Cerebellum Core Module Unit Tests
# ============================================================================
# WHAT: Unit tests for cerebellum substrate and thalamic bridges
# WHY:  Verify metabolic modulation and VL thalamic routing
# HOW:  Test bridge lifecycle, signal routing, and modulation effects
# ============================================================================

message(STATUS "  Core Cerebellum Unit Tests")

# Helper macro for cerebellum core tests
macro(add_cerebellum_core_test TEST_NAME)
    add_executable(unit_core_cerebellum_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_core_cerebellum_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    target_compile_features(unit_core_cerebellum_${TEST_NAME} PRIVATE cxx_std_17)
    add_test(NAME unit_core_cerebellum_${TEST_NAME}
             COMMAND unit_core_cerebellum_${TEST_NAME})
    set_tests_properties(unit_core_cerebellum_${TEST_NAME} PROPERTIES
        LABELS "unit;core;cerebellum"
    )
endmacro()

# Substrate bridge tests (metabolic modulation)
add_cerebellum_core_test(test_cerebellum_substrate_bridge)

# Thalamic bridge tests (VL nucleus routing)
add_cerebellum_core_test(test_cerebellum_thalamic_bridge)
