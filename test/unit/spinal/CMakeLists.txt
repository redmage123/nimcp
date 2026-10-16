# ============================================================================
# Spinal Cord Unit Tests (Check framework, C)
# ============================================================================
# WHAT: Tests for spinal cord (CPGs, reflex arcs, motor output)
# WHY:  Verify correct lifecycle, CPG control, reflex triggering, and motor output
# HOW:  Links against nimcp + libcheck
# ============================================================================

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_spinal_cord.c)
    add_executable(unit_spinal_test_spinal_cord
        test_spinal_cord.c
    )
    target_include_directories(unit_spinal_test_spinal_cord PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(unit_spinal_test_spinal_cord PRIVATE c_std_11)
    target_compile_options(unit_spinal_test_spinal_cord PRIVATE
        -Wall -Wextra
    )
    target_link_libraries(unit_spinal_test_spinal_cord PRIVATE
        nimcp Threads::Threads check_pic subunit rt m
    )
    add_test(NAME unit_spinal_test_spinal_cord
             COMMAND unit_spinal_test_spinal_cord
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(unit_spinal_test_spinal_cord
        PROPERTIES
            LABELS "unit;spinal;spinal_cord"
            TIMEOUT 120)
endif()
