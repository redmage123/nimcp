# ============================================================================
# Subcortical Brain Structures Integration Tests
# ============================================================================
#
# WHAT: Integration tests for subcortical brain structures
# WHY:  Verify basal ganglia pathways, dopamine modulation, and action selection
# HOW:  Create integrated subcortical circuits and test pathway interactions
#

# Subcortical Integration Tests (all pathways)
add_executable(integration_core_brain_subcortical_test
    test_subcortical_integration.cpp
)

target_link_libraries(integration_core_brain_subcortical_test
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)

target_include_directories(integration_core_brain_subcortical_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/core/brain/subcortical
)

add_test(NAME integration_core_brain_subcortical_test
         COMMAND integration_core_brain_subcortical_test)
set_tests_properties(integration_core_brain_subcortical_test PROPERTIES LABELS "integration")

gtest_discover_tests(integration_core_brain_subcortical_test)

# Thalamus Integration Tests
add_executable(integration_core_brain_subcortical_thalamus_test
    test_thalamus_integration.cpp
)

target_link_libraries(integration_core_brain_subcortical_thalamus_test
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)

target_include_directories(integration_core_brain_subcortical_thalamus_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME integration_core_brain_subcortical_thalamus_test
         COMMAND integration_core_brain_subcortical_thalamus_test)
set_tests_properties(integration_core_brain_subcortical_thalamus_test PROPERTIES LABELS "integration")

gtest_discover_tests(integration_core_brain_subcortical_thalamus_test)

# Amygdala Integration Tests
add_executable(integration_core_brain_subcortical_amygdala_test
    test_amygdala_integration.cpp
)

target_link_libraries(integration_core_brain_subcortical_amygdala_test
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)

target_include_directories(integration_core_brain_subcortical_amygdala_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME integration_core_brain_subcortical_amygdala_test
         COMMAND integration_core_brain_subcortical_amygdala_test)
set_tests_properties(integration_core_brain_subcortical_amygdala_test PROPERTIES LABELS "integration")

gtest_discover_tests(integration_core_brain_subcortical_amygdala_test)

# DISABLED: Test needs API updates to match current basal ganglia API
# TODO: Update to use basal_ganglia_process_input, basal_ganglia_step, basal_ganglia_select_action
#
# # Basal Ganglia Full Integration Tests (striosome-matrix, chunking, vigor)
# add_executable(integration_core_brain_subcortical_basal_ganglia_test
#     test_basal_ganglia_integration.cpp
# )
#
# target_link_libraries(integration_core_brain_subcortical_basal_ganglia_test
#     PRIVATE
#         GTest::GTest
#         GTest::Main
#         nimcp
#         Threads::Threads
#         m
# )
#
# target_include_directories(integration_core_brain_subcortical_basal_ganglia_test
#     PRIVATE
#         ${CMAKE_SOURCE_DIR}/include
#         ${CMAKE_SOURCE_DIR}/src
#         ${CMAKE_SOURCE_DIR}/test
# )
#
# add_test(NAME integration_core_brain_subcortical_basal_ganglia_test
#          COMMAND integration_core_brain_subcortical_basal_ganglia_test)
# set_tests_properties(integration_core_brain_subcortical_basal_ganglia_test PROPERTIES LABELS "integration")
#
# gtest_discover_tests(integration_core_brain_subcortical_basal_ganglia_test)
