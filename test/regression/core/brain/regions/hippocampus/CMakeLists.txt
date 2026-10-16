# Hippocampus Regression Tests
# Ensures previously working functionality doesn't regress

add_executable(regression_core_brain_regions_hippocampus_test_hippocampus_regression
    test_hippocampus_regression.cpp
)

target_include_directories(regression_core_brain_regions_hippocampus_test_hippocampus_regression
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(regression_core_brain_regions_hippocampus_test_hippocampus_regression
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)

add_test(NAME regression_core_brain_regions_hippocampus_test_hippocampus_regression
         COMMAND regression_core_brain_regions_hippocampus_test_hippocampus_regression)
