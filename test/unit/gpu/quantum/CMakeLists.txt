# ============================================================================
# GPU Quantum Recovery Unit Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for GPU quantum module recovery unit tests
# WHY:  Enable testing of GPU recovery in quantum operations
# HOW:  Link against GTest framework and nimcp library
# ============================================================================

# Find GTest
find_package(GTest QUIET)

if(GTest_FOUND AND CUDAToolkit_FOUND)
    message(STATUS "Adding GPU quantum recovery unit tests (GTest)...")

    # ========================================================================
    # Test: Quantum GPU Recovery
    # ========================================================================
    add_executable(unit_gpu_quantum_test_quantum_gpu_recovery
        test_quantum_gpu_recovery.cpp
    )

    target_link_libraries(unit_gpu_quantum_test_quantum_gpu_recovery
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(unit_gpu_quantum_test_quantum_gpu_recovery
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    add_test(NAME unit_gpu_quantum_recovery COMMAND unit_gpu_quantum_test_quantum_gpu_recovery)
    set_tests_properties(unit_gpu_quantum_recovery PROPERTIES
        TIMEOUT 180
        LABELS "unit;gpu;recovery;quantum"
    )

    # ========================================================================
    # Test: QMC GPU Recovery
    # ========================================================================
    add_executable(unit_gpu_quantum_test_qmc_gpu_recovery
        test_qmc_gpu_recovery.cpp
    )

    target_link_libraries(unit_gpu_quantum_test_qmc_gpu_recovery
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(unit_gpu_quantum_test_qmc_gpu_recovery
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    add_test(NAME unit_gpu_qmc_recovery COMMAND unit_gpu_quantum_test_qmc_gpu_recovery)
    set_tests_properties(unit_gpu_qmc_recovery PROPERTIES
        TIMEOUT 180
        LABELS "unit;gpu;recovery;qmc;quantum"
    )

else()
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found - skipping GPU quantum recovery unit tests")
    endif()
    if(NOT CUDAToolkit_FOUND)
        message(STATUS "CUDA toolkit not found - skipping GPU quantum recovery unit tests")
    endif()
endif()
