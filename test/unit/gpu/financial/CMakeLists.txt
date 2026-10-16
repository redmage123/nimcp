# ============================================================================
# Financial GPU Unit Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for financial GPU unit tests
# WHY:  Enable testing of financial GPU kernels
# HOW:  Link against GTest framework and nimcp library
# ============================================================================

# Find GTest
find_package(GTest QUIET)

if(GTest_FOUND AND CUDAToolkit_FOUND)
    message(STATUS "Adding financial GPU unit tests (GTest)...")

    # ========================================================================
    # Test: Financial GPU Monte Carlo
    # ========================================================================
    add_executable(unit_gpu_financial_test_monte_carlo
        test_financial_monte_carlo_gpu.cpp
    )

    target_link_libraries(unit_gpu_financial_test_monte_carlo
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(unit_gpu_financial_test_monte_carlo
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    add_test(NAME unit_gpu_financial_monte_carlo COMMAND unit_gpu_financial_test_monte_carlo)
    set_tests_properties(unit_gpu_financial_monte_carlo PROPERTIES
        TIMEOUT 180
        LABELS "unit;gpu;financial"
    )

    # ========================================================================
    # Test: Financial GPU Optimization
    # ========================================================================
    add_executable(unit_gpu_financial_test_optimization
        test_financial_optimization_gpu.cpp
    )

    target_link_libraries(unit_gpu_financial_test_optimization
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(unit_gpu_financial_test_optimization
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    add_test(NAME unit_gpu_financial_optimization COMMAND unit_gpu_financial_test_optimization)
    set_tests_properties(unit_gpu_financial_optimization PROPERTIES
        TIMEOUT 180
        LABELS "unit;gpu;financial"
    )

    # ========================================================================
    # Test: Financial GPU Risk
    # ========================================================================
    add_executable(unit_gpu_financial_test_risk
        test_financial_risk_gpu.cpp
    )

    target_link_libraries(unit_gpu_financial_test_risk
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(unit_gpu_financial_test_risk
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    add_test(NAME unit_gpu_financial_risk COMMAND unit_gpu_financial_test_risk)
    set_tests_properties(unit_gpu_financial_risk PROPERTIES
        TIMEOUT 180
        LABELS "unit;gpu;financial"
    )

    # ========================================================================
    # Test: Financial GPU Derivatives
    # ========================================================================
    add_executable(unit_gpu_financial_test_derivatives
        test_financial_derivatives_gpu.cpp
    )

    target_link_libraries(unit_gpu_financial_test_derivatives
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(unit_gpu_financial_test_derivatives
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    add_test(NAME unit_gpu_financial_derivatives COMMAND unit_gpu_financial_test_derivatives)
    set_tests_properties(unit_gpu_financial_derivatives PROPERTIES
        TIMEOUT 180
        LABELS "unit;gpu;financial"
    )

else()
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found - skipping financial GPU unit tests")
    endif()
    if(NOT CUDAToolkit_FOUND)
        message(STATUS "CUDA toolkit not found - skipping financial GPU unit tests")
    endif()
endif()

# ============================================================================
# Legacy Check-based tests (if available)
# ============================================================================
find_package(Check QUIET)
if(Check_FOUND)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_financial_monte_carlo_gpu.c)
        add_executable(test_financial_monte_carlo_gpu_check
            test_financial_monte_carlo_gpu.c
        )
        target_link_libraries(test_financial_monte_carlo_gpu_check
            PRIVATE nimcp Check::check m pthread
        )
        target_include_directories(test_financial_monte_carlo_gpu_check
            PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src
        )
        add_test(NAME unit_financial_monte_carlo_gpu_check COMMAND test_financial_monte_carlo_gpu_check)
    endif()

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_financial_optimization_gpu.c)
        add_executable(test_financial_optimization_gpu_check
            test_financial_optimization_gpu.c
        )
        target_link_libraries(test_financial_optimization_gpu_check
            PRIVATE nimcp Check::check m pthread
        )
        target_include_directories(test_financial_optimization_gpu_check
            PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src
        )
        add_test(NAME unit_financial_optimization_gpu_check COMMAND test_financial_optimization_gpu_check)
    endif()
endif()
