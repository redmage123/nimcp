# ============================================================================
# Neural GPU Unit Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for neural network GPU unit tests
# WHY:  Enable testing of SNN, LNN, and CNN GPU kernels with recovery
# HOW:  Link against GTest framework and nimcp library
# ============================================================================

# Find GTest
find_package(GTest QUIET)

if(GTest_FOUND AND CUDAToolkit_FOUND)
    message(STATUS "Adding neural GPU unit tests (GTest)...")

    # ========================================================================
    # Test: SNN GPU Recovery
    # ========================================================================
    add_executable(unit_gpu_neural_test_snn_gpu_recovery
        test_snn_gpu_recovery.cpp
    )

    target_link_libraries(unit_gpu_neural_test_snn_gpu_recovery
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(unit_gpu_neural_test_snn_gpu_recovery
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    target_compile_definitions(unit_gpu_neural_test_snn_gpu_recovery
        PRIVATE
            $<$<BOOL:${CUDAToolkit_FOUND}>:NIMCP_ENABLE_CUDA>
    )

    add_test(NAME unit_gpu_neural_snn_recovery COMMAND unit_gpu_neural_test_snn_gpu_recovery)
    set_tests_properties(unit_gpu_neural_snn_recovery PROPERTIES
        TIMEOUT 180
        LABELS "unit;gpu;neural;snn;recovery"
    )

    # ========================================================================
    # Test: LNN GPU Recovery
    # ========================================================================
    add_executable(unit_gpu_neural_test_lnn_gpu_recovery
        test_lnn_gpu_recovery.cpp
    )

    target_link_libraries(unit_gpu_neural_test_lnn_gpu_recovery
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(unit_gpu_neural_test_lnn_gpu_recovery
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    target_compile_definitions(unit_gpu_neural_test_lnn_gpu_recovery
        PRIVATE
            $<$<BOOL:${CUDAToolkit_FOUND}>:NIMCP_ENABLE_CUDA>
    )

    add_test(NAME unit_gpu_neural_lnn_recovery COMMAND unit_gpu_neural_test_lnn_gpu_recovery)
    set_tests_properties(unit_gpu_neural_lnn_recovery PROPERTIES
        TIMEOUT 180
        LABELS "unit;gpu;neural;lnn;recovery"
    )

    # ========================================================================
    # Test: CNN GPU Recovery
    # ========================================================================
    add_executable(unit_gpu_neural_test_cnn_gpu_recovery
        test_cnn_gpu_recovery.cpp
    )

    target_link_libraries(unit_gpu_neural_test_cnn_gpu_recovery
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(unit_gpu_neural_test_cnn_gpu_recovery
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    target_compile_definitions(unit_gpu_neural_test_cnn_gpu_recovery
        PRIVATE
            $<$<BOOL:${CUDAToolkit_FOUND}>:NIMCP_ENABLE_CUDA>
    )

    add_test(NAME unit_gpu_neural_cnn_recovery COMMAND unit_gpu_neural_test_cnn_gpu_recovery)
    set_tests_properties(unit_gpu_neural_cnn_recovery PROPERTIES
        TIMEOUT 180
        LABELS "unit;gpu;neural;cnn;recovery"
    )

else()
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found - skipping neural GPU unit tests")
    endif()
    if(NOT CUDAToolkit_FOUND)
        message(STATUS "CUDA toolkit not found - skipping neural GPU unit tests")
    endif()
endif()
