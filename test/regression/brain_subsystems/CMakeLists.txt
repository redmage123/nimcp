# =============================================================================
# Brain Subsystems Regression Tests (C — libcheck)
# =============================================================================
# WHAT: Regression tests for the 7 new brain subsystem modules
# WHY:  Guard against initialization failures, training NaN/Inf, and
#       create/destroy crashes after subsystem refactors
# HOW:  C test files linked against nimcp + libcheck (Check framework)
#
# SUBSYSTEMS COVERED:
#   white_matter, inferior_colliculus, spinal_cord, cortical_interneurons,
#   neuropeptide, endocannabinoid, glymphatic
# =============================================================================

# ---------------------------------------------------------------------------
# Test 1: Brain creation initializes all 7 subsystem fields
# ---------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_brain_create_with_new_subsystems.c)
    add_executable(regression_brain_subsystems_create_test
        test_brain_create_with_new_subsystems.c
    )
    target_include_directories(regression_brain_subsystems_create_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_brain_subsystems_create_test PRIVATE c_std_11)
    target_link_libraries(regression_brain_subsystems_create_test PRIVATE
        nimcp_middleware nimcp Threads::Threads check_pic subunit rt m
    )
    add_test(NAME regression_brain_subsystems_create_test
             COMMAND regression_brain_subsystems_create_test
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(regression_brain_subsystems_create_test
        PROPERTIES
            LABELS "regression;brain;subsystems;create"
            TIMEOUT 300
            RESOURCE_LOCK "brain_heavy")
endif()

# ---------------------------------------------------------------------------
# Test 2: Training with all 7 subsystems active produces valid loss
# ---------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_training_with_new_subsystems.c)
    add_executable(regression_brain_subsystems_training_test
        test_training_with_new_subsystems.c
    )
    target_include_directories(regression_brain_subsystems_training_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_brain_subsystems_training_test PRIVATE c_std_11)
    target_link_libraries(regression_brain_subsystems_training_test PRIVATE
        nimcp_middleware nimcp Threads::Threads check_pic subunit rt m
    )
    add_test(NAME regression_brain_subsystems_training_test
             COMMAND regression_brain_subsystems_training_test
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(regression_brain_subsystems_training_test
        PROPERTIES
            LABELS "regression;brain;subsystems;training"
            TIMEOUT 300
            RESOURCE_LOCK "brain_heavy")
endif()

# ---------------------------------------------------------------------------
# Test 3: Standalone create/update/destroy lifecycle for each subsystem
# ---------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_new_subsystem_bio_async.c)
    add_executable(regression_brain_subsystems_bio_async_test
        test_new_subsystem_bio_async.c
    )
    target_include_directories(regression_brain_subsystems_bio_async_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_brain_subsystems_bio_async_test PRIVATE c_std_11)
    target_link_libraries(regression_brain_subsystems_bio_async_test PRIVATE
        nimcp_middleware nimcp Threads::Threads check_pic subunit rt m
    )
    add_test(NAME regression_brain_subsystems_bio_async_test
             COMMAND regression_brain_subsystems_bio_async_test
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(regression_brain_subsystems_bio_async_test
        PROPERTIES
            LABELS "regression;brain;subsystems;bio_async;lifecycle"
            TIMEOUT 120)
endif()

# ---------------------------------------------------------------------------
# Test 4: Parallel init regression (wave-based subsystem initialization)
# ---------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_parallel_init_regression.c)
    add_executable(regression_brain_subsystems_parallel_init_test
        test_parallel_init_regression.c
    )
    target_include_directories(regression_brain_subsystems_parallel_init_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_brain_subsystems_parallel_init_test PRIVATE c_std_11)
    target_link_libraries(regression_brain_subsystems_parallel_init_test PRIVATE
        nimcp_middleware nimcp Threads::Threads check_pic subunit rt m
    )
    add_test(NAME regression_brain_subsystems_parallel_init_test
             COMMAND regression_brain_subsystems_parallel_init_test
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(regression_brain_subsystems_parallel_init_test
        PROPERTIES
            LABELS "regression;brain;subsystems;parallel_init"
            TIMEOUT 600
            RESOURCE_LOCK "brain_heavy")
endif()

# ---------------------------------------------------------------------------
# Test 5: Walkthrough #17 regression tests (CRITICALs + HIGHs)
# ---------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_walkthrough17_fixes.c)
    add_executable(regression_walkthrough17_fixes
        test_walkthrough17_fixes.c
    )
    target_include_directories(regression_walkthrough17_fixes PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_walkthrough17_fixes PRIVATE c_std_11)
    target_link_libraries(regression_walkthrough17_fixes PRIVATE
        nimcp Threads::Threads check_pic subunit rt m
    )
    add_test(NAME regression_walkthrough17_fixes
             COMMAND regression_walkthrough17_fixes
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(regression_walkthrough17_fixes
        PROPERTIES
            LABELS "regression;walkthrough17;bugfix"
            TIMEOUT 60)
endif()
