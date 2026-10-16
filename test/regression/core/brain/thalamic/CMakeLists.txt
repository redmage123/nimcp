# ============================================================================
# Thalamic Layer Regression Tests
# ============================================================================
#
# WHAT: Regression tests for thalamic layer components
# WHY:  Ensure stability, determinism, and performance of thalamus, router, STN, and bridges
# HOW:  GTest-based tests with benchmarks, stress tests, and consistency checks
#

# ============================================================================
# Thalamus Regression Tests
# ============================================================================

add_executable(regression_thalamus_test
    test_thalamus_regression.cpp
)

target_link_libraries(regression_thalamus_test
    PRIVATE
        nimcp
        GTest::GTest
        GTest::Main
        Threads::Threads
)

target_include_directories(regression_thalamus_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/test
)

add_test(NAME regression_thalamus_test
         COMMAND regression_thalamus_test)
set_tests_properties(regression_thalamus_test PROPERTIES
    LABELS "regression"
    TIMEOUT 240
)

gtest_discover_tests(regression_thalamus_test)

# ============================================================================
# Thalamic Router Regression Tests
# ============================================================================

add_executable(regression_thalamic_router_test
    test_thalamic_router_regression.cpp
)

target_link_libraries(regression_thalamic_router_test
    PRIVATE
        nimcp
        GTest::GTest
        GTest::Main
        Threads::Threads
)

target_include_directories(regression_thalamic_router_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/test
)

add_test(NAME regression_thalamic_router_test
         COMMAND regression_thalamic_router_test)
set_tests_properties(regression_thalamic_router_test PROPERTIES
    LABELS "regression"
    TIMEOUT 300
)

gtest_discover_tests(regression_thalamic_router_test)

# ============================================================================
# Subthalamic Nucleus Regression Tests
# ============================================================================

add_executable(regression_subthalamic_test
    test_subthalamic_regression.cpp
)

target_link_libraries(regression_subthalamic_test
    PRIVATE
        nimcp
        GTest::GTest
        GTest::Main
        Threads::Threads
)

target_include_directories(regression_subthalamic_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/test
)

add_test(NAME regression_subthalamic_test
         COMMAND regression_subthalamic_test)
set_tests_properties(regression_subthalamic_test PROPERTIES
    LABELS "regression"
    TIMEOUT 300
    RESOURCE_LOCK "brain_heavy"
)

gtest_discover_tests(regression_subthalamic_test)

# ============================================================================
# Thalamic-Immune Bridge Regression Tests
# ============================================================================

add_executable(regression_thalamic_immune_bridge_test
    test_thalamic_immune_bridge_regression.cpp
)

target_link_libraries(regression_thalamic_immune_bridge_test
    PRIVATE
        nimcp
        GTest::GTest
        GTest::Main
        Threads::Threads
)

target_include_directories(regression_thalamic_immune_bridge_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/test
)

add_test(NAME regression_thalamic_immune_bridge_test
         COMMAND regression_thalamic_immune_bridge_test)
set_tests_properties(regression_thalamic_immune_bridge_test PROPERTIES
    LABELS "regression"
    TIMEOUT 240
)

gtest_discover_tests(regression_thalamic_immune_bridge_test)
