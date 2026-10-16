# ============================================================================
# Subcortical Unit Tests (Check framework, C)
# ============================================================================
# WHAT: Tests for inferior colliculus (auditory midbrain)
# WHY:  Verify correct lifecycle, audio processing, localization, and query APIs
# HOW:  Links against nimcp + libcheck
# ============================================================================

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_inferior_colliculus.c)
    add_executable(unit_subcortical_test_inferior_colliculus
        test_inferior_colliculus.c
    )
    target_include_directories(unit_subcortical_test_inferior_colliculus PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(unit_subcortical_test_inferior_colliculus PRIVATE c_std_11)
    target_compile_options(unit_subcortical_test_inferior_colliculus PRIVATE
        -Wall -Wextra
    )
    target_link_libraries(unit_subcortical_test_inferior_colliculus PRIVATE
        nimcp Threads::Threads check_pic subunit rt m
    )
    add_test(NAME unit_subcortical_test_inferior_colliculus
             COMMAND unit_subcortical_test_inferior_colliculus
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(unit_subcortical_test_inferior_colliculus
        PROPERTIES
            LABELS "unit;subcortical;inferior_colliculus"
            TIMEOUT 120)
endif()
