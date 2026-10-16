# ============================================================================
# Brain Regions Unit Tests (Check framework, C)
# ============================================================================
# WHAT: Tests for glymphatic, endocannabinoid, and neuropeptide subsystems
# WHY:  Verify correct lifecycle, dynamics, and query APIs
# HOW:  Each test links against nimcp + libcheck
# ============================================================================

# --- Glymphatic System ---
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_glymphatic.c)
    add_executable(unit_regions_test_glymphatic
        test_glymphatic.c
    )
    target_include_directories(unit_regions_test_glymphatic PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(unit_regions_test_glymphatic PRIVATE c_std_11)
    target_compile_options(unit_regions_test_glymphatic PRIVATE
        -Wall -Wextra
    )
    target_link_libraries(unit_regions_test_glymphatic PRIVATE
        nimcp Threads::Threads check_pic subunit rt m
    )
    add_test(NAME unit_regions_test_glymphatic
             COMMAND unit_regions_test_glymphatic
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(unit_regions_test_glymphatic
        PROPERTIES
            LABELS "unit;regions;glymphatic"
            TIMEOUT 120)
endif()

# --- Endocannabinoid System ---
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_endocannabinoid.c)
    add_executable(unit_regions_test_endocannabinoid
        test_endocannabinoid.c
    )
    target_include_directories(unit_regions_test_endocannabinoid PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(unit_regions_test_endocannabinoid PRIVATE c_std_11)
    target_compile_options(unit_regions_test_endocannabinoid PRIVATE
        -Wall -Wextra
    )
    target_link_libraries(unit_regions_test_endocannabinoid PRIVATE
        nimcp Threads::Threads check_pic subunit rt m
    )
    add_test(NAME unit_regions_test_endocannabinoid
             COMMAND unit_regions_test_endocannabinoid
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(unit_regions_test_endocannabinoid
        PROPERTIES
            LABELS "unit;regions;endocannabinoid"
            TIMEOUT 120)
endif()

# --- Neuropeptide System ---
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_neuropeptide.c)
    add_executable(unit_regions_test_neuropeptide
        test_neuropeptide.c
    )
    target_include_directories(unit_regions_test_neuropeptide PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(unit_regions_test_neuropeptide PRIVATE c_std_11)
    target_compile_options(unit_regions_test_neuropeptide PRIVATE
        -Wall -Wextra
    )
    target_link_libraries(unit_regions_test_neuropeptide PRIVATE
        nimcp Threads::Threads check_pic subunit rt m
    )
    add_test(NAME unit_regions_test_neuropeptide
             COMMAND unit_regions_test_neuropeptide
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(unit_regions_test_neuropeptide
        PROPERTIES
            LABELS "unit;regions;neuropeptide"
            TIMEOUT 120)
endif()
