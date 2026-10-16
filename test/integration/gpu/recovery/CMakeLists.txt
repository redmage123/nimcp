# ============================================================================
# GPU Recovery Integration Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for GPU recovery integration tests
# WHY:  Enable testing of GPU recovery with other modules
# HOW:  Link against GTest framework, nimcp library, and module dependencies
# ============================================================================

# Find GTest
find_package(GTest QUIET)

if(GTest_FOUND AND CUDAToolkit_FOUND)
    message(STATUS "Adding GPU recovery integration tests (GTest)...")

    # ========================================================================
    # Test: GPU Recovery with Fuzzy Module
    # ========================================================================
    add_executable(integration_gpu_recovery_test_gpu_recovery_with_fuzzy
        test_gpu_recovery_with_fuzzy.cpp
    )

    target_link_libraries(integration_gpu_recovery_test_gpu_recovery_with_fuzzy
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(integration_gpu_recovery_test_gpu_recovery_with_fuzzy
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    add_test(NAME integration_gpu_recovery_fuzzy COMMAND integration_gpu_recovery_test_gpu_recovery_with_fuzzy)
    set_tests_properties(integration_gpu_recovery_fuzzy PROPERTIES
        TIMEOUT 180
        LABELS "integration;gpu;recovery;fuzzy"
    )

    # ========================================================================
    # Test: GPU Recovery with Financial Module
    # ========================================================================
    add_executable(integration_gpu_recovery_test_gpu_recovery_with_financial
        test_gpu_recovery_with_financial.cpp
    )

    target_link_libraries(integration_gpu_recovery_test_gpu_recovery_with_financial
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(integration_gpu_recovery_test_gpu_recovery_with_financial
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    add_test(NAME integration_gpu_recovery_financial COMMAND integration_gpu_recovery_test_gpu_recovery_with_financial)
    set_tests_properties(integration_gpu_recovery_financial PROPERTIES
        TIMEOUT 180
        LABELS "integration;gpu;recovery;financial"
    )

    # ========================================================================
    # Test: GPU Recovery with Statistics Module
    # ========================================================================
    add_executable(integration_gpu_recovery_test_gpu_recovery_with_statistics
        test_gpu_recovery_with_statistics.cpp
    )

    target_link_libraries(integration_gpu_recovery_test_gpu_recovery_with_statistics
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(integration_gpu_recovery_test_gpu_recovery_with_statistics
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    add_test(NAME integration_gpu_recovery_statistics COMMAND integration_gpu_recovery_test_gpu_recovery_with_statistics)
    set_tests_properties(integration_gpu_recovery_statistics PROPERTIES
        TIMEOUT 180
        LABELS "integration;gpu;recovery;statistics"
    )

    # ========================================================================
    # Test: GPU Recovery with Neural Module
    # ========================================================================
    add_executable(integration_gpu_recovery_test_gpu_recovery_with_neural
        test_gpu_recovery_with_neural.cpp
    )

    target_link_libraries(integration_gpu_recovery_test_gpu_recovery_with_neural
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(integration_gpu_recovery_test_gpu_recovery_with_neural
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    add_test(NAME integration_gpu_recovery_neural COMMAND integration_gpu_recovery_test_gpu_recovery_with_neural)
    set_tests_properties(integration_gpu_recovery_neural PROPERTIES
        TIMEOUT 180
        LABELS "integration;gpu;recovery;neural"
    )

    # ========================================================================
    # Test: GPU Recovery Memory Management
    # ========================================================================
    add_executable(integration_gpu_recovery_test_gpu_recovery_memory_management
        test_gpu_recovery_memory_management.cpp
    )

    target_link_libraries(integration_gpu_recovery_test_gpu_recovery_memory_management
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            ${CUDA_LIBRARIES}
            m
            pthread
    )

    target_include_directories(integration_gpu_recovery_test_gpu_recovery_memory_management
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CUDA_INCLUDE_DIRS}
    )

    add_test(NAME integration_gpu_recovery_memory COMMAND integration_gpu_recovery_test_gpu_recovery_memory_management)
    set_tests_properties(integration_gpu_recovery_memory PROPERTIES
        TIMEOUT 180
        LABELS "integration;gpu;recovery;memory"
    )

else()
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found - skipping GPU recovery integration tests")
    endif()
    if(NOT CUDAToolkit_FOUND)
        message(STATUS "CUDA toolkit not found - skipping GPU recovery integration tests")
    endif()
endif()
