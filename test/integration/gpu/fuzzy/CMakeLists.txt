# ============================================================================
# Fuzzy GPU Integration Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for fuzzy GPU integration tests
# WHY:  Enable testing of fuzzy GPU inference and training pipelines
# HOW:  Link against GTest framework and nimcp library
# ============================================================================

# Find GTest
find_package(GTest QUIET)

if(GTest_FOUND AND CUDAToolkit_FOUND)
    message(STATUS "Adding fuzzy GPU integration tests (GTest)...")

    # ========================================================================
    # Test: Fuzzy GPU Inference Integration
    # ========================================================================
    # DISABLED: Test includes missing header utils/fuzzy/nimcp_fuzzy_mf.h
    # TODO: Fix header path or implement missing header
    # add_executable(integration_gpu_fuzzy_test_inference
    #     test_fuzzy_gpu_inference.cpp
    # )
    #
    # target_link_libraries(integration_gpu_fuzzy_test_inference
    #     PRIVATE
    #         nimcp
    #         GTest::gtest
    #         GTest::gtest_main
    #         ${CUDA_LIBRARIES}
    #         m
    #         pthread
    # )
    #
    # target_include_directories(integration_gpu_fuzzy_test_inference
    #     PRIVATE
    #         ${CMAKE_SOURCE_DIR}/include
    #         ${CMAKE_SOURCE_DIR}/src
    #         ${CUDA_INCLUDE_DIRS}
    # )
    #
    # add_test(NAME integration_gpu_fuzzy_inference COMMAND integration_gpu_fuzzy_test_inference)
    # set_tests_properties(integration_gpu_fuzzy_inference PROPERTIES
    #     TIMEOUT 300
    #     LABELS "integration;gpu;fuzzy"
    # )

    # ========================================================================
    # Test: Fuzzy GPU ANFIS Training Integration
    # ========================================================================
    # DISABLED: Test includes missing header utils/fuzzy/nimcp_fuzzy_mf.h
    # TODO: Fix header path or implement missing header
    # add_executable(integration_gpu_fuzzy_test_anfis
    #     test_fuzzy_gpu_anfis.cpp
    # )
    #
    # target_link_libraries(integration_gpu_fuzzy_test_anfis
    #     PRIVATE
    #         nimcp
    #         GTest::gtest
    #         GTest::gtest_main
    #         ${CUDA_LIBRARIES}
    #         m
    #         pthread
    # )
    #
    # target_include_directories(integration_gpu_fuzzy_test_anfis
    #     PRIVATE
    #         ${CMAKE_SOURCE_DIR}/include
    #         ${CMAKE_SOURCE_DIR}/src
    #         ${CUDA_INCLUDE_DIRS}
    # )
    #
    # add_test(NAME integration_gpu_fuzzy_anfis COMMAND integration_gpu_fuzzy_test_anfis)
    # set_tests_properties(integration_gpu_fuzzy_anfis PROPERTIES
    #     TIMEOUT 300
    #     LABELS "integration;gpu;fuzzy;training"
    # )

else()
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found - skipping fuzzy GPU integration tests")
    endif()
    if(NOT CUDAToolkit_FOUND)
        message(STATUS "CUDA toolkit not found - skipping fuzzy GPU integration tests")
    endif()
endif()

# ============================================================================
# Legacy Check-based tests (if available)
# ============================================================================
find_package(Check QUIET)
if(Check_FOUND)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_fuzzy_gpu_inference.c)
        add_executable(integration_fuzzy_gpu_inference_check
            test_fuzzy_gpu_inference.c
        )
        target_link_libraries(integration_fuzzy_gpu_inference_check
            PRIVATE nimcp Check::check m pthread
        )
        target_include_directories(integration_fuzzy_gpu_inference_check
            PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src
        )
        add_test(NAME integration_fuzzy_gpu_inference_check COMMAND integration_fuzzy_gpu_inference_check)
    endif()

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_fuzzy_gpu_anfis.c)
        add_executable(integration_fuzzy_gpu_anfis_check
            test_fuzzy_gpu_anfis.c
        )
        target_link_libraries(integration_fuzzy_gpu_anfis_check
            PRIVATE nimcp Check::check m pthread
        )
        target_include_directories(integration_fuzzy_gpu_anfis_check
            PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src
        )
        add_test(NAME integration_fuzzy_gpu_anfis_check COMMAND integration_fuzzy_gpu_anfis_check)
    endif()
endif()
