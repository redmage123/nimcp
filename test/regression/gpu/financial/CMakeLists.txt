# ============================================================================
# Financial GPU Regression Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for financial GPU regression tests
# WHY:  Ensure financial GPU computations remain stable across updates
# HOW:  GTest framework with CUDA toolkit dependency
# ============================================================================

# Guard: CUDA toolkit required for GPU regression tests
if(NOT CUDAToolkit_FOUND)
    message(STATUS "  Skipping financial GPU regression tests (CUDA toolkit not found)")
    return()
endif()

# Find GTest
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    message(STATUS "  Skipping financial GPU regression tests (GTest not found)")
    return()
endif()

message(STATUS "  Building financial GPU regression tests")

# ============================================================================
# Financial GPU Comprehensive Regression Test
# ============================================================================
add_executable(regression_gpu_financial_test_comprehensive
    test_financial_gpu_regression.cpp
)

target_link_libraries(regression_gpu_financial_test_comprehensive
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)

target_include_directories(regression_gpu_financial_test_comprehensive
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(regression_gpu_financial_test_comprehensive PRIVATE cxx_std_17)
target_compile_definitions(regression_gpu_financial_test_comprehensive PRIVATE NIMCP_TESTING)

add_test(NAME regression_gpu_financial_comprehensive
         COMMAND regression_gpu_financial_test_comprehensive)

set_tests_properties(regression_gpu_financial_comprehensive PROPERTIES
    LABELS "regression;gpu;financial"
    TIMEOUT 300
)
