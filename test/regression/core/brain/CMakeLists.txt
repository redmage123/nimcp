# Brain Regression Tests
# Test core brain functionality regressions

# Note: factory and processing subdirectories are added separately by main CMakeLists.txt

# Oscillation Backward Compatibility Test
add_executable(regression_core_brain_oscillation_backward_compat
    ../test_oscillation_backward_compat.cpp
)
target_link_libraries(regression_core_brain_oscillation_backward_compat
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
    )
add_test(NAME regression_core_brain_oscillation_backward_compat
         COMMAND regression_core_brain_oscillation_backward_compat)
# FLAKY: OscillationDetectorCompatTest is pre-existing flaky in middleware;
#        timing-sensitive oscillation detection varies with system load
set_tests_properties(regression_core_brain_oscillation_backward_compat PROPERTIES
    LABELS "regression;flaky"
)

# Brain JSON Regression Test
add_executable(regression_core_brain_json_regression
    test_brain_json_regression.cpp
)
target_link_libraries(regression_core_brain_json_regression
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
    )
add_test(NAME regression_core_brain_json_regression
         COMMAND regression_core_brain_json_regression)

# Distributed COW Regression Test
add_executable(regression_core_brain_distributed_cow_regression
    test_distributed_cow_regression.cpp
)
target_link_libraries(regression_core_brain_distributed_cow_regression
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
    )
add_test(NAME regression_core_brain_distributed_cow_regression
         COMMAND regression_core_brain_distributed_cow_regression)

# Memory Tracking Regression Test
add_executable(regression_core_brain_memory_tracking_regression
    test_memory_tracking_regression.cpp
)
target_link_libraries(regression_core_brain_memory_tracking_regression
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
    )
add_test(NAME regression_core_brain_memory_tracking_regression
         COMMAND regression_core_brain_memory_tracking_regression)

# Brain Persistence Regression Test - DISABLED: outdated API
# add_executable(regression_core_brain_persistence_regression
#     test_brain_persistence_regression.cpp
# )
# target_link_libraries(regression_core_brain_persistence_regression
#     PRIVATE
#         GTest::GTest
#         GTest::Main
#         nimcp
#     )
# add_test(NAME regression_core_brain_persistence_regression
#          COMMAND regression_core_brain_persistence_regression)

# Brain Bio-Async Regression Test (DISABLED - needs API fixes)
# add_executable(regression_core_brain_bio_async_regression
#     test_brain_bio_async_regression.cpp
# )
# target_link_libraries(regression_core_brain_bio_async_regression
#     PRIVATE
#         GTest::GTest
#         GTest::Main
#         nimcp
#     )
# add_test(NAME regression_core_brain_bio_async_regression
#          COMMAND regression_core_brain_bio_async_regression)

# Brain Coordinator Regression Test
add_executable(regression_core_brain_coordinator_regression
    test_brain_coordinator_regression.cpp
)
target_link_libraries(regression_core_brain_coordinator_regression
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
    )
target_include_directories(regression_core_brain_coordinator_regression
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME regression_core_brain_coordinator_regression
         COMMAND regression_core_brain_coordinator_regression)
set_tests_properties(regression_core_brain_coordinator_regression PROPERTIES LABELS "regression")

# Set test timeouts for long-running regression tests (5 minutes)
# set_tests_properties(regression_core_brain_bio_async_regression
#     PROPERTIES TIMEOUT 300)
# set_tests_properties(regression_core_brain_persistence_regression
#     PROPERTIES TIMEOUT 300)

# KG Hierarchy Graph Algorithms Regression Test
add_executable(regression_core_brain_kg_hierarchy_graph_algorithms
    test_kg_hierarchy_graph_algorithms_regression.cpp
)
target_link_libraries(regression_core_brain_kg_hierarchy_graph_algorithms
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
    )
target_include_directories(regression_core_brain_kg_hierarchy_graph_algorithms
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME regression_core_brain_kg_hierarchy_graph_algorithms
         COMMAND regression_core_brain_kg_hierarchy_graph_algorithms)
set_tests_properties(regression_core_brain_kg_hierarchy_graph_algorithms PROPERTIES LABELS "regression")

# Add subcortical subdirectory
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/subcortical")
    add_subdirectory(subcortical)
endif()

# Add regions subdirectory (hypothalamus, broca, cerebellum, etc.)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/regions")
    add_subdirectory(regions)
endif()

# Add hemispheric subdirectory (hemispheric brain, corpus callosum, lateralization)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/hemispheric")
    add_subdirectory(hemispheric)
endif()

# Add thalamic subdirectory (thalamus, thalamic router, subthalamic nucleus)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thalamic")
    add_subdirectory(thalamic)
endif()

# Brain Memory Optimization Regression Test (Opt 2+3+6+8)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_brain_memory_opt_regression.cpp")
    add_executable(regression_core_brain_memory_opt
        test_brain_memory_opt_regression.cpp
    )
    target_link_libraries(regression_core_brain_memory_opt
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            Threads::Threads
    )
    target_include_directories(regression_core_brain_memory_opt
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )
    add_test(NAME regression_core_brain_memory_opt
             COMMAND regression_core_brain_memory_opt)
    set_tests_properties(regression_core_brain_memory_opt PROPERTIES
        LABELS "regression"
        TIMEOUT 300)
endif()

# Add language subdirectory (Broca's region, Wernicke's region, language pathway)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/language")
    add_subdirectory(language)
endif()

# Brain Cycle Coordinator Regression Test
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_brain_cycle_coordinator_regression.cpp")
    add_executable(regression_core_brain_cycle_coordinator
        test_brain_cycle_coordinator_regression.cpp
    )
    target_link_libraries(regression_core_brain_cycle_coordinator
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            Threads::Threads
        )
    target_include_directories(regression_core_brain_cycle_coordinator
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )
    add_test(NAME regression_core_brain_cycle_coordinator
             COMMAND regression_core_brain_cycle_coordinator)
    set_tests_properties(regression_core_brain_cycle_coordinator PROPERTIES LABELS "regression")
endif()

# Core Brain Walkthrough Fixes Regression Test (P1/P2/P3 fixes)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_core_brain_walkthrough_fixes.cpp")
    add_executable(regression_core_brain_walkthrough_fixes
        test_core_brain_walkthrough_fixes.cpp
    )
    target_link_libraries(regression_core_brain_walkthrough_fixes
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            Threads::Threads
        )
    target_include_directories(regression_core_brain_walkthrough_fixes
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )
    add_test(NAME regression_core_brain_walkthrough_fixes
             COMMAND regression_core_brain_walkthrough_fixes)
    set_tests_properties(regression_core_brain_walkthrough_fixes PROPERTIES
        LABELS "regression"
        TIMEOUT 120)
endif()

# Core Pass 3 Fixes Regression Test (P1/P2/P3 fixes)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_core_pass3_fixes.cpp")
    add_executable(regression_core_pass3_fixes
        test_core_pass3_fixes.cpp
    )
    target_link_libraries(regression_core_pass3_fixes
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            Threads::Threads
        )
    target_include_directories(regression_core_pass3_fixes
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )
    add_test(NAME regression_core_pass3_fixes
             COMMAND regression_core_pass3_fixes)
    set_tests_properties(regression_core_pass3_fixes PROPERTIES
        LABELS "regression"
        TIMEOUT 120)
endif()
