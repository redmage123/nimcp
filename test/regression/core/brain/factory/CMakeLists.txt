# ============================================================================
# Brain Factory Regression Tests
# ============================================================================

# Test 1: Factory Regression - Stability and consistency
add_executable(regression_core_brain_factory_test_factory_regression
    test_factory_regression.cpp
)
target_link_libraries(regression_core_brain_factory_test_factory_regression
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(regression_core_brain_factory_test_factory_regression
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME regression_core_brain_factory_test_factory_regression
         COMMAND regression_core_brain_factory_test_factory_regression)
# FLAKY: factory_regression - OOM-killed under parallel ctest (-j4) due to
#        large brain allocations (10-60GB) competing for memory
set_tests_properties(regression_core_brain_factory_test_factory_regression PROPERTIES
    LABELS "regression;flaky"
    TIMEOUT 600
    RESOURCE_LOCK "brain_heavy"
)

# Test 2: Brain Init Hypothalamus Regression - Hypothalamus initialization stability
add_executable(regression_core_brain_factory_test_brain_init_hypothalamus
    test_brain_init_hypothalamus_regression.cpp
)
target_link_libraries(regression_core_brain_factory_test_brain_init_hypothalamus
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(regression_core_brain_factory_test_brain_init_hypothalamus
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME regression_core_brain_factory_test_brain_init_hypothalamus
         COMMAND regression_core_brain_factory_test_brain_init_hypothalamus)
set_tests_properties(regression_core_brain_factory_test_brain_init_hypothalamus PROPERTIES LABELS "regression")
