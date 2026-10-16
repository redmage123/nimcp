# ============================================================================
# Financial GPU Integration Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for financial GPU integration tests
# WHY:  Enable testing of financial GPU pipelines and workflows
# HOW:  Link against GTest framework, CUDA, and nimcp library
# ============================================================================

# Find GTest
find_package(GTest QUIET)

if(GTest_FOUND AND CUDAToolkit_FOUND)
    message(STATUS "Adding financial GPU integration tests (GTest)...")

    # Common include directories
    # Note: CUDA include directories removed to avoid bf16/fp16 header conflicts
    # Tests only use the C API, not CUDA kernels directly
    set(FINANCIAL_GPU_TEST_INCLUDES
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )

    # Common link libraries
    set(FINANCIAL_GPU_TEST_LIBS
        nimcp
        GTest::gtest
        GTest::gtest_main
        CUDA::cudart
        m
        pthread
    )

    # ========================================================================
    # Test: Financial GPU Integration (Cross-module integration)
    # ========================================================================
    # Tests:
    #   - Monte Carlo + Risk: Simulate returns, compute VaR/CVaR
    #   - Optimization + Risk: Optimize portfolio, analyze risk
    #   - Derivatives + Simulation: Price options using MC
    #   - Multi-asset correlation flow
    #   - CPU vs GPU equivalence
    # ========================================================================
    add_executable(integration_gpu_financial_test_integration
        test_financial_gpu_integration.cpp
    )

    target_link_libraries(integration_gpu_financial_test_integration
        PRIVATE ${FINANCIAL_GPU_TEST_LIBS}
    )

    target_include_directories(integration_gpu_financial_test_integration
        PRIVATE ${FINANCIAL_GPU_TEST_INCLUDES}
    )

    target_compile_definitions(integration_gpu_financial_test_integration
        PRIVATE NIMCP_ENABLE_CUDA=1
    )

    add_test(NAME integration_gpu_financial COMMAND integration_gpu_financial_test_integration)
    set_tests_properties(integration_gpu_financial PROPERTIES
        TIMEOUT 300
        LABELS "integration;gpu;financial"
    )

    # ========================================================================
    # Test: Financial GPU Workflow (Real-world workflows)
    # ========================================================================
    # Tests:
    #   - Portfolio optimization pipeline
    #   - Risk scenario stress testing
    #   - Options pricing and Greeks workflow
    #   - Black-Litterman integration
    #   - GPU recovery during financial operations
    #   - Multi-asset correlation and VaR
    # ========================================================================
    add_executable(integration_gpu_financial_test_workflow
        test_financial_gpu_workflow.cpp
    )

    target_link_libraries(integration_gpu_financial_test_workflow
        PRIVATE ${FINANCIAL_GPU_TEST_LIBS}
    )

    target_include_directories(integration_gpu_financial_test_workflow
        PRIVATE ${FINANCIAL_GPU_TEST_INCLUDES}
    )

    target_compile_definitions(integration_gpu_financial_test_workflow
        PRIVATE NIMCP_ENABLE_CUDA=1
    )

    add_test(NAME integration_gpu_financial_workflow COMMAND integration_gpu_financial_test_workflow)
    set_tests_properties(integration_gpu_financial_workflow PROPERTIES
        TIMEOUT 600
        LABELS "integration;gpu;financial;workflow"
    )

    # ========================================================================
    # Combined test target for all financial GPU integration tests
    # ========================================================================
    add_custom_target(integration_gpu_financial_all
        DEPENDS
            integration_gpu_financial_test_integration
            integration_gpu_financial_test_workflow
        COMMENT "Building all financial GPU integration tests"
    )

else()
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found - skipping financial GPU integration tests")
    endif()
    if(NOT CUDAToolkit_FOUND)
        message(STATUS "CUDA toolkit not found - skipping financial GPU integration tests")
    endif()
endif()

# ============================================================================
# Legacy Check-based tests (if available)
# ============================================================================
find_package(Check QUIET)
if(Check_FOUND)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_financial_gpu_integration.c)
        add_executable(integration_financial_gpu_check
            test_financial_gpu_integration.c
        )
        target_link_libraries(integration_financial_gpu_check
            PRIVATE nimcp Check::check m pthread
        )
        target_include_directories(integration_financial_gpu_check
            PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src
        )
        add_test(NAME integration_financial_gpu_check COMMAND integration_financial_gpu_check)
        set_tests_properties(integration_financial_gpu_check PROPERTIES
            LABELS "integration;gpu;financial;legacy"
        )
    endif()
endif()
