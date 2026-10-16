# ============================================================================
# Fuzzy GPU Unit Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for fuzzy GPU unit tests
# WHY:  Enable testing of fuzzy logic GPU kernels
# HOW:  Link against GTest framework and nimcp library
# ============================================================================

# Find GTest
find_package(GTest QUIET)

if(GTest_FOUND AND CUDAToolkit_FOUND)
    message(STATUS "Adding fuzzy GPU unit tests (GTest)...")

    # ========================================================================
    # Test: Fuzzy GPU Membership Functions
    # ========================================================================
    add_executable(unit_gpu_fuzzy_test_fuzzy_gpu_mf
        test_fuzzy_gpu_mf.cpp
    )

    target_link_libraries(unit_gpu_fuzzy_test_fuzzy_gpu_mf
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(unit_gpu_fuzzy_test_fuzzy_gpu_mf
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    add_test(NAME unit_gpu_fuzzy_mf COMMAND unit_gpu_fuzzy_test_fuzzy_gpu_mf)
    set_tests_properties(unit_gpu_fuzzy_mf PROPERTIES
        TIMEOUT 120
        LABELS "unit;gpu;fuzzy"
    )

    # ========================================================================
    # Test: Fuzzy GPU Defuzzification
    # ========================================================================
    add_executable(unit_gpu_fuzzy_test_fuzzy_gpu_defuzz
        test_fuzzy_gpu_defuzz.cpp
    )

    target_link_libraries(unit_gpu_fuzzy_test_fuzzy_gpu_defuzz
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(unit_gpu_fuzzy_test_fuzzy_gpu_defuzz
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    add_test(NAME unit_gpu_fuzzy_defuzz COMMAND unit_gpu_fuzzy_test_fuzzy_gpu_defuzz)
    set_tests_properties(unit_gpu_fuzzy_defuzz PROPERTIES
        TIMEOUT 120
        LABELS "unit;gpu;fuzzy"
    )

    # ========================================================================
    # Test: Fuzzy GPU Operators
    # ========================================================================
    # DISABLED: Test needs implementation - nimcp_gpu_fuzzy_tnorm_batch,
    #           nimcp_gpu_fuzzy_tconorm_batch, nimcp_gpu_fuzzy_complement_batch,
    #           nimcp_gpu_fuzzy_implication_batch, nimcp_gpu_fuzzy_aggregate_batch
    #           are not implemented yet
    # TODO: Implement GPU fuzzy operator batch functions
    # add_executable(unit_gpu_fuzzy_test_fuzzy_gpu_operators
    #     test_fuzzy_gpu_operators.cpp
    # )
    #
    # target_link_libraries(unit_gpu_fuzzy_test_fuzzy_gpu_operators
    #     PRIVATE
    #         nimcp
    #         GTest::gtest
    #         GTest::gtest_main
    #         ${CUDA_LIBRARIES}
    #         m
    #         pthread
    # )
    #
    # target_include_directories(unit_gpu_fuzzy_test_fuzzy_gpu_operators
    #     PRIVATE
    #         ${CMAKE_SOURCE_DIR}/include
    #         ${CMAKE_SOURCE_DIR}/src
    #         ${CUDA_INCLUDE_DIRS}
    # )
    #
    # add_test(NAME unit_gpu_fuzzy_operators COMMAND unit_gpu_fuzzy_test_fuzzy_gpu_operators)
    # set_tests_properties(unit_gpu_fuzzy_operators PROPERTIES
    #     TIMEOUT 120
    #     LABELS "unit;gpu;fuzzy"
    # )

else()
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found - skipping fuzzy GPU unit tests")
    endif()
    if(NOT CUDAToolkit_FOUND)
        message(STATUS "CUDA toolkit not found - skipping fuzzy GPU unit tests")
    endif()
endif()

# ============================================================================
# Legacy Check-based tests (if available)
# ============================================================================
find_package(Check QUIET)
if(Check_FOUND)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_fuzzy_gpu_mf.c)
        add_executable(test_fuzzy_gpu_mf_check
            test_fuzzy_gpu_mf.c
        )
        target_link_libraries(test_fuzzy_gpu_mf_check
            PRIVATE nimcp Check::check m pthread
        )
        target_include_directories(test_fuzzy_gpu_mf_check
            PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src
        )
        add_test(NAME unit_fuzzy_gpu_mf_check COMMAND test_fuzzy_gpu_mf_check)
    endif()

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_fuzzy_gpu_defuzz.c)
        add_executable(test_fuzzy_gpu_defuzz_check
            test_fuzzy_gpu_defuzz.c
        )
        target_link_libraries(test_fuzzy_gpu_defuzz_check
            PRIVATE nimcp Check::check m pthread
        )
        target_include_directories(test_fuzzy_gpu_defuzz_check
            PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src
        )
        add_test(NAME unit_fuzzy_gpu_defuzz_check COMMAND test_fuzzy_gpu_defuzz_check)
    endif()
endif()
