# ============================================================================
# Cortical Interneuron Unit Tests (Check framework, C)
# ============================================================================
# WHAT: Tests for cortical interneuron system (E/I balance, gamma, disinhibition)
# WHY:  Verify correct lifecycle, modulation, and query APIs
# HOW:  Links against nimcp + libcheck
# ============================================================================

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_cortical_interneurons.c)
    add_executable(unit_cortical_test_cortical_interneurons
        test_cortical_interneurons.c
    )
    target_include_directories(unit_cortical_test_cortical_interneurons PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(unit_cortical_test_cortical_interneurons PRIVATE c_std_11)
    target_compile_options(unit_cortical_test_cortical_interneurons PRIVATE
        -Wall -Wextra
    )
    target_link_libraries(unit_cortical_test_cortical_interneurons PRIVATE
        nimcp Threads::Threads check_pic subunit rt m
    )
    add_test(NAME unit_cortical_test_cortical_interneurons
             COMMAND unit_cortical_test_cortical_interneurons
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(unit_cortical_test_cortical_interneurons
        PROPERTIES
            LABELS "unit;cortical;interneurons"
            TIMEOUT 120)
endif()
