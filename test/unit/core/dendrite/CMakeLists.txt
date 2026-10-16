# ============================================================================
# NIMCP Dendrite Unit Tests
# ============================================================================
# WHAT: Unit tests for dendrite computation and integration module
# WHY:  Verify dendrite segments, spines, cable theory, and plasticity
# HOW:  GTest-based tests with comprehensive coverage
# ============================================================================

# Unit test for dendrite module
add_executable(unit_core_dendrite_test_dendrite
    test_dendrite.cpp
    ${CMAKE_SOURCE_DIR}/src/core/dendrite/nimcp_dendrite.c
)

target_link_libraries(unit_core_dendrite_test_dendrite
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)

target_compile_features(unit_core_dendrite_test_dendrite PRIVATE cxx_std_17)

target_include_directories(unit_core_dendrite_test_dendrite
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

# Enable NIMCP_TESTING for test APIs
target_compile_definitions(unit_core_dendrite_test_dendrite PRIVATE NIMCP_TESTING)

add_test(NAME unit_core_dendrite_test_dendrite
         COMMAND unit_core_dendrite_test_dendrite)

set_tests_properties(unit_core_dendrite_test_dendrite
    PROPERTIES
        TIMEOUT 120
        LABELS "unit;core;dendrite"
)
