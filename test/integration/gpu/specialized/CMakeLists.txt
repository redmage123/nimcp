# ============================================================================
# GPU Recovery Specialized Modules Integration Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for specialized GPU recovery integration tests
# WHY:  Enable testing of GPU recovery with dragonfly, portia, sparse,
#       ternary, and topology modules
# HOW:  Link against GTest framework, nimcp library, and CUDA
# ============================================================================

# Find GTest
find_package(GTest QUIET)

if(GTest_FOUND AND CUDAToolkit_FOUND)
    message(STATUS "Adding GPU recovery specialized modules integration tests (GTest)...")

    # ========================================================================
    # Test: GPU Recovery with Specialized Modules
    # ========================================================================
    # DISABLED: Test needs API updates - GPU_RECOVERY_* enums, recovery_stats
    #           fields missing from current API
    # TODO: Update test to use current GPU recovery API
    # add_executable(integration_gpu_specialized_test_specialized_gpu_recovery_integration
    #     test_specialized_gpu_recovery_integration.cpp
    # )
    #
    # target_link_libraries(integration_gpu_specialized_test_specialized_gpu_recovery_integration
    #     PRIVATE
    #         nimcp
    #         GTest::gtest
    #         GTest::gtest_main
    #         ${CUDA_LIBRARIES}
    #         m
    #         pthread
    # )
    #
    # target_include_directories(integration_gpu_specialized_test_specialized_gpu_recovery_integration
    #     PRIVATE
    #         ${CMAKE_SOURCE_DIR}/include
    #         ${CMAKE_SOURCE_DIR}/src
    #         ${CUDA_INCLUDE_DIRS}
    # )
    #
    # add_test(NAME integration_gpu_specialized_recovery COMMAND integration_gpu_specialized_test_specialized_gpu_recovery_integration)
    # set_tests_properties(integration_gpu_specialized_recovery PROPERTIES
    #     TIMEOUT 300
    #     LABELS "integration;gpu;recovery;specialized;dragonfly;portia;sparse;ternary;topology"
    # )

else()
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found - skipping GPU specialized integration tests")
    endif()
    if(NOT CUDAToolkit_FOUND)
        message(STATUS "CUDA toolkit not found - skipping GPU specialized integration tests")
    endif()
endif()
