# ============================================================================
# GPU Recovery Unit Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for GPU self-healing recovery unit tests
# WHY:  Enable testing of GPU recovery infrastructure
# HOW:  Link against GTest framework and nimcp library
# ============================================================================

# Find GTest
find_package(GTest QUIET)

if(GTest_FOUND AND CUDAToolkit_FOUND)
    message(STATUS "Adding GPU recovery unit tests (GTest)...")

    # ========================================================================
    # Test: GPU Recovery Configuration
    # ========================================================================
    add_executable(unit_gpu_recovery_test_gpu_recovery_config
        test_gpu_recovery_config.cpp
    )

    target_link_libraries(unit_gpu_recovery_test_gpu_recovery_config
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(unit_gpu_recovery_test_gpu_recovery_config
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    add_test(NAME unit_gpu_recovery_config COMMAND unit_gpu_recovery_test_gpu_recovery_config)
    set_tests_properties(unit_gpu_recovery_config PROPERTIES
        TIMEOUT 120
        LABELS "unit;gpu;recovery"
    )

    # ========================================================================
    # Test: GPU Recovery Initialization
    # ========================================================================
    add_executable(unit_gpu_recovery_test_gpu_recovery_init
        test_gpu_recovery_init.cpp
    )

    target_link_libraries(unit_gpu_recovery_test_gpu_recovery_init
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(unit_gpu_recovery_test_gpu_recovery_init
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    add_test(NAME unit_gpu_recovery_init COMMAND unit_gpu_recovery_test_gpu_recovery_init)
    set_tests_properties(unit_gpu_recovery_init PROPERTIES
        TIMEOUT 120
        LABELS "unit;gpu;recovery"
    )

    # ========================================================================
    # Test: GPU Recovery Context Management
    # ========================================================================
    add_executable(unit_gpu_recovery_test_gpu_recovery_context
        test_gpu_recovery_context.cpp
    )

    target_link_libraries(unit_gpu_recovery_test_gpu_recovery_context
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(unit_gpu_recovery_test_gpu_recovery_context
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    add_test(NAME unit_gpu_recovery_context COMMAND unit_gpu_recovery_test_gpu_recovery_context)
    set_tests_properties(unit_gpu_recovery_context PROPERTIES
        TIMEOUT 120
        LABELS "unit;gpu;recovery"
    )

    # ========================================================================
    # Test: GPU Recovery Error Categorization
    # ========================================================================
    add_executable(unit_gpu_recovery_test_gpu_recovery_error_categorization
        test_gpu_recovery_error_categorization.cpp
    )

    target_link_libraries(unit_gpu_recovery_test_gpu_recovery_error_categorization
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(unit_gpu_recovery_test_gpu_recovery_error_categorization
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    add_test(NAME unit_gpu_recovery_error_categorization COMMAND unit_gpu_recovery_test_gpu_recovery_error_categorization)
    set_tests_properties(unit_gpu_recovery_error_categorization PROPERTIES
        TIMEOUT 120
        LABELS "unit;gpu;recovery"
    )

    # ========================================================================
    # Test: GPU Recovery Parameter Correction
    # ========================================================================
    add_executable(unit_gpu_recovery_test_gpu_recovery_param_correction
        test_gpu_recovery_param_correction.cpp
    )

    target_link_libraries(unit_gpu_recovery_test_gpu_recovery_param_correction
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(unit_gpu_recovery_test_gpu_recovery_param_correction
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    add_test(NAME unit_gpu_recovery_param_correction COMMAND unit_gpu_recovery_test_gpu_recovery_param_correction)
    set_tests_properties(unit_gpu_recovery_param_correction PROPERTIES
        TIMEOUT 120
        LABELS "unit;gpu;recovery"
    )

    # ========================================================================
    # Test: GPU Recovery CPU Fallback
    # ========================================================================
    add_executable(unit_gpu_recovery_test_gpu_recovery_cpu_fallback
        test_gpu_recovery_cpu_fallback.cpp
    )

    target_link_libraries(unit_gpu_recovery_test_gpu_recovery_cpu_fallback
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(unit_gpu_recovery_test_gpu_recovery_cpu_fallback
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    add_test(NAME unit_gpu_recovery_cpu_fallback COMMAND unit_gpu_recovery_test_gpu_recovery_cpu_fallback)
    set_tests_properties(unit_gpu_recovery_cpu_fallback PROPERTIES
        TIMEOUT 120
        LABELS "unit;gpu;recovery"
    )

    # ========================================================================
    # Test: GPU Recovery Specialized Modules
    # ========================================================================
    # DISABLED: Test needs implementation - nimcp_sparse_ctx_*, nimcp_graph_gpu_*,
    #           nimcp_topology_compute_*, nimcp_community_detect_* not implemented
    # TODO: Implement sparse, graph, and topology GPU functions
    # add_executable(unit_gpu_recovery_test_gpu_recovery_specialized_modules
    #     test_gpu_recovery_specialized_modules.cpp
    # )
    #
    # target_link_libraries(unit_gpu_recovery_test_gpu_recovery_specialized_modules
    #     PRIVATE
    #         nimcp
    #         GTest::gtest
    #         GTest::gtest_main
    #         ${CUDA_LIBRARIES}
    #         m
    #         pthread
    # )
    #
    # target_include_directories(unit_gpu_recovery_test_gpu_recovery_specialized_modules
    #     PRIVATE
    #         ${CMAKE_SOURCE_DIR}/include
    #         ${CMAKE_SOURCE_DIR}/src
    #         ${CUDA_INCLUDE_DIRS}
    # )
    #
    # add_test(NAME unit_gpu_recovery_specialized_modules COMMAND unit_gpu_recovery_test_gpu_recovery_specialized_modules)
    # set_tests_properties(unit_gpu_recovery_specialized_modules PROPERTIES
    #     TIMEOUT 180
    #     LABELS "unit;gpu;recovery;specialized;dragonfly;portia;sparse;ternary;topology"
    # )

else()
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found - skipping GPU recovery unit tests")
    endif()
    if(NOT CUDAToolkit_FOUND)
        message(STATUS "CUDA toolkit not found - skipping GPU recovery unit tests")
    endif()
endif()
