# =============================================================================
# Unit Tests for Brain Bridge Wiring
# =============================================================================
# WHAT: Unit tests for consumer-bridge wiring (Wave 8A+ campaign).
# WHY:  Guard against regressions where a bridge is "wired" at the call site
#       but the update fn early-returns because its input was NULL.
# HOW:  Check bridge_base_t::total_updates advances after brain_decide.
# =============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_halfstatue_wiring.cpp")
    add_executable(unit_brain_bridges_test_halfstatue_wiring
        ${CMAKE_CURRENT_SOURCE_DIR}/test_halfstatue_wiring.cpp
    )

    target_link_libraries(unit_brain_bridges_test_halfstatue_wiring
        PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )

    target_include_directories(unit_brain_bridges_test_halfstatue_wiring
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )

    target_compile_features(unit_brain_bridges_test_halfstatue_wiring
        PRIVATE cxx_std_17)
    # NOTE: do NOT add NIMCP_TESTING/NIMCP_UNIT_TESTING — they toggle a
    # cuBLAS header block that redeclares GemmEx with an incompatible
    # compute-type signature (see test_brain_glial_init for the same
    # workaround).

    add_test(NAME unit_brain_bridges_test_halfstatue_wiring
             COMMAND unit_brain_bridges_test_halfstatue_wiring)
    set_tests_properties(unit_brain_bridges_test_halfstatue_wiring PROPERTIES
        LABELS "unit;brain;bridges;halfstatue;wiring"
        TIMEOUT 120)

    message(STATUS "  Added Unit test: unit_brain_bridges_test_halfstatue_wiring")
endif()
