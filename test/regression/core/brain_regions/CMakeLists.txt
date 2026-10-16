# ============================================================================
# Brain Regions Regression Tests (core/brain_regions)
# ============================================================================
# WHAT: CMakeLists for brain region regression tests under core/
# WHY:  Build and register regression tests for language production bridge
# HOW:  Create test executables linked to nimcp and gtest
# ============================================================================

# Language Production Bridge Regression Test
add_executable(regression_core_brain_regions_test_language_bridge_regression
    test_language_bridge_regression.cpp
)

target_link_libraries(regression_core_brain_regions_test_language_bridge_regression
    PRIVATE
        nimcp
        GTest::gtest
        pthread
        m
)

target_include_directories(regression_core_brain_regions_test_language_bridge_regression
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(regression_core_brain_regions_test_language_bridge_regression PRIVATE cxx_std_17)

add_test(NAME regression_core_brain_regions_test_language_bridge_regression
         COMMAND regression_core_brain_regions_test_language_bridge_regression)

set_tests_properties(regression_core_brain_regions_test_language_bridge_regression
    PROPERTIES
        TIMEOUT 120
        LABELS "regression;core;brain_regions;language;performance"
)

# Language Production Bridge Regression Test (alternate)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_language_production_bridge_regression.cpp")
    add_executable(regression_core_brain_regions_test_language_production_bridge_regression
        test_language_production_bridge_regression.cpp
    )

    target_link_libraries(regression_core_brain_regions_test_language_production_bridge_regression
        PRIVATE
            nimcp
            GTest::gtest
            pthread
            m
    )

    target_include_directories(regression_core_brain_regions_test_language_production_bridge_regression
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
    )

    target_compile_features(regression_core_brain_regions_test_language_production_bridge_regression PRIVATE cxx_std_17)

    add_test(NAME regression_core_brain_regions_test_language_production_bridge_regression
             COMMAND regression_core_brain_regions_test_language_production_bridge_regression)

    set_tests_properties(regression_core_brain_regions_test_language_production_bridge_regression
        PROPERTIES
            TIMEOUT 120
            LABELS "regression;core;brain_regions;language;performance"
    )
endif()
