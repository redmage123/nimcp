# ============================================================================
# Subcortical Brain Structures Regression Tests
# ============================================================================
#
# WHAT: Regression tests for subcortical brain structures
# WHY:  Ensure stability, backward compatibility, and numerical correctness
# HOW:  Test determinism, memory safety, reset consistency, and output bounds
#

# Subcortical Regression Tests
add_executable(regression_core_brain_subcortical_test
    test_subcortical_regression.cpp
)

target_link_libraries(regression_core_brain_subcortical_test
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)

target_include_directories(regression_core_brain_subcortical_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/core/brain/subcortical
)

add_test(NAME regression_core_brain_subcortical_test
         COMMAND regression_core_brain_subcortical_test)
set_tests_properties(regression_core_brain_subcortical_test PROPERTIES LABELS "regression")

# Allow longer timeout for stability tests
set_tests_properties(regression_core_brain_subcortical_test PROPERTIES TIMEOUT 120)

gtest_discover_tests(regression_core_brain_subcortical_test)

# Thalamus Regression Tests
add_executable(regression_core_brain_subcortical_thalamus_test
    test_thalamus_regression.cpp
)

target_link_libraries(regression_core_brain_subcortical_thalamus_test
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)

target_include_directories(regression_core_brain_subcortical_thalamus_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME regression_core_brain_subcortical_thalamus_test
         COMMAND regression_core_brain_subcortical_thalamus_test)
set_tests_properties(regression_core_brain_subcortical_thalamus_test PROPERTIES LABELS "regression")
set_tests_properties(regression_core_brain_subcortical_thalamus_test PROPERTIES TIMEOUT 120)

gtest_discover_tests(regression_core_brain_subcortical_thalamus_test)

# Amygdala Regression Tests
add_executable(regression_core_brain_subcortical_amygdala_test
    test_amygdala_regression.cpp
)

target_link_libraries(regression_core_brain_subcortical_amygdala_test
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)

target_include_directories(regression_core_brain_subcortical_amygdala_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME regression_core_brain_subcortical_amygdala_test
         COMMAND regression_core_brain_subcortical_amygdala_test)
set_tests_properties(regression_core_brain_subcortical_amygdala_test PROPERTIES LABELS "regression")
set_tests_properties(regression_core_brain_subcortical_amygdala_test PROPERTIES TIMEOUT 120)

gtest_discover_tests(regression_core_brain_subcortical_amygdala_test)

# DISABLED: Test needs API updates to match current basal ganglia API
# TODO: Update to use basal_ganglia_process_input, basal_ganglia_step, basal_ganglia_select_action
#
# # Basal Ganglia Regression Tests (bug fixes and enhancements)
# add_executable(regression_core_brain_subcortical_basal_ganglia_test
#     test_basal_ganglia_regression.cpp
# )
#
# target_link_libraries(regression_core_brain_subcortical_basal_ganglia_test
#     PRIVATE
#         GTest::GTest
#         GTest::Main
#         nimcp
#         Threads::Threads
#         m
# )
#
# target_include_directories(regression_core_brain_subcortical_basal_ganglia_test
#     PRIVATE
#         ${CMAKE_SOURCE_DIR}/include
#         ${CMAKE_SOURCE_DIR}/src
# )
#
# add_test(NAME regression_core_brain_subcortical_basal_ganglia_test
#          COMMAND regression_core_brain_subcortical_basal_ganglia_test)
# set_tests_properties(regression_core_brain_subcortical_basal_ganglia_test PROPERTIES LABELS "regression")
# set_tests_properties(regression_core_brain_subcortical_basal_ganglia_test PROPERTIES TIMEOUT 120)
#
# gtest_discover_tests(regression_core_brain_subcortical_basal_ganglia_test)
