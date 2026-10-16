# ============================================================================
# GPU Recovery Regression Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for GPU recovery regression tests
# WHY:  Prevent regressions in GPU recovery functionality
# HOW:  Link against GTest framework and nimcp library
# ============================================================================

# Find GTest
find_package(GTest QUIET)

if(GTest_FOUND AND CUDAToolkit_FOUND)
    message(STATUS "Adding GPU recovery regression tests (GTest)...")

    # ========================================================================
    # Test: GPU Recovery Regression Suite
    # ========================================================================
    add_executable(regression_gpu_recovery_test_gpu_recovery_regression
        test_gpu_recovery_regression.cpp
    )

    target_link_libraries(regression_gpu_recovery_test_gpu_recovery_regression
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(regression_gpu_recovery_test_gpu_recovery_regression
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    add_test(NAME regression_gpu_recovery COMMAND regression_gpu_recovery_test_gpu_recovery_regression)
    set_tests_properties(regression_gpu_recovery PROPERTIES
        TIMEOUT 300
        LABELS "regression;gpu;recovery"
    )

else()
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found - skipping GPU recovery regression tests")
    endif()
    if(NOT CUDAToolkit_FOUND)
        message(STATUS "CUDA toolkit not found - skipping GPU recovery regression tests")
    endif()
endif()
