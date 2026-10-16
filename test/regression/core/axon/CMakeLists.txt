# ============================================================================
# NIMCP Axon Performance Regression Tests
# ============================================================================

add_executable(regression_core_axon_performance
    test_axon_performance_regression.cpp
    ${CMAKE_SOURCE_DIR}/src/core/axon/nimcp_axon.c
)

target_link_libraries(regression_core_axon_performance
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)

target_compile_features(regression_core_axon_performance PRIVATE cxx_std_17)

target_include_directories(regression_core_axon_performance
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME regression_core_axon_performance
         COMMAND regression_core_axon_performance)

set_tests_properties(regression_core_axon_performance
    PROPERTIES
        TIMEOUT 180
        LABELS "regression;core;axon;performance"
        RESOURCE_LOCK "brain_heavy"
)
