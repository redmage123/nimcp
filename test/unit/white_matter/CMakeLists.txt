# ============================================================================
# White Matter Tract Unit Tests (Check framework, C)
# ============================================================================
# WHAT: Tests for white matter tract system (myelination, conduction, routing)
# WHY:  Verify correct lifecycle, modulation, signal routing, and query APIs
# HOW:  Links against nimcp + libcheck
# ============================================================================

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_white_matter_tracts.c)
    add_executable(unit_white_matter_test_white_matter_tracts
        test_white_matter_tracts.c
    )
    target_include_directories(unit_white_matter_test_white_matter_tracts PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(unit_white_matter_test_white_matter_tracts PRIVATE c_std_11)
    target_compile_options(unit_white_matter_test_white_matter_tracts PRIVATE
        -Wall -Wextra
    )
    target_link_libraries(unit_white_matter_test_white_matter_tracts PRIVATE
        nimcp Threads::Threads check_pic subunit rt m
    )
    add_test(NAME unit_white_matter_test_white_matter_tracts
             COMMAND unit_white_matter_test_white_matter_tracts
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(unit_white_matter_test_white_matter_tracts
        PROPERTIES
            LABELS "unit;white_matter;tracts"
            TIMEOUT 120)
endif()
