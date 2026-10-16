# ============================================================================
# Physics Integration Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for physics module integration tests
# WHY:  Verify physics modules integrate correctly with NIMCP systems
# HOW:  Include subdirectories for each physics submodule
# ============================================================================

# Find GoogleTest
find_package(GTest QUIET)

if(NOT GTest_FOUND)
    message(STATUS "GoogleTest not found - skipping physics integration tests")
    return()
endif()

# Include subdirectories
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/biophysics)
    add_subdirectory(biophysics)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/bridges)
    add_subdirectory(bridges)
endif()

# ============================================================================
# Physics Bio-Async Integration Tests
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_physics_bio_async_integration.cpp)
    add_executable(integration_physics_bio_async_test
        test_physics_bio_async_integration.cpp
    )
    target_include_directories(integration_physics_bio_async_test PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(integration_physics_bio_async_test PRIVATE
        nimcp GTest::gtest GTest::gtest_main m
    )
    gtest_discover_tests(integration_physics_bio_async_test
        PROPERTIES LABELS "integration;physics;bio_async" TIMEOUT 60
    )
endif()

# ============================================================================
# Physics Quantum Integration Tests
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_physics_quantum_integration.cpp)
    add_executable(integration_physics_quantum_test
        test_physics_quantum_integration.cpp
    )
    target_include_directories(integration_physics_quantum_test PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(integration_physics_quantum_test PRIVATE
        nimcp GTest::gtest GTest::gtest_main m
    )
    gtest_discover_tests(integration_physics_quantum_test
        PROPERTIES LABELS "integration;physics;quantum" TIMEOUT 120
    )
endif()

# ============================================================================
# Physics SNN Integration Tests
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_physics_snn_integration.cpp)
    add_executable(integration_physics_snn_test
        test_physics_snn_integration.cpp
    )
    target_include_directories(integration_physics_snn_test PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(integration_physics_snn_test PRIVATE
        nimcp GTest::gtest GTest::gtest_main m
    )
    gtest_discover_tests(integration_physics_snn_test
        PROPERTIES LABELS "integration;physics;snn" TIMEOUT 60
    )
endif()

# ============================================================================
# Physics Immune Integration Tests
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_physics_immune_integration.cpp)
    add_executable(integration_physics_immune_test
        test_physics_immune_integration.cpp
    )
    target_include_directories(integration_physics_immune_test PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(integration_physics_immune_test PRIVATE
        nimcp GTest::gtest GTest::gtest_main m
    )
    gtest_discover_tests(integration_physics_immune_test
        PROPERTIES LABELS "integration;physics;immune" TIMEOUT 60
    )
endif()

# ============================================================================
# Physics KG Integration Tests
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_physics_kg_integration.cpp)
    add_executable(integration_physics_kg_test
        test_physics_kg_integration.cpp
    )
    target_include_directories(integration_physics_kg_test PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(integration_physics_kg_test PRIVATE
        nimcp GTest::gtest GTest::gtest_main m
    )
    gtest_discover_tests(integration_physics_kg_test
        PROPERTIES LABELS "integration;physics;kg" TIMEOUT 60
    )
endif()

# ============================================================================
# Physics Thread Pool Integration Tests
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_physics_thread_pool_integration.cpp)
    add_executable(integration_physics_thread_pool_test
        test_physics_thread_pool_integration.cpp
    )
    target_include_directories(integration_physics_thread_pool_test PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(integration_physics_thread_pool_test PRIVATE
        nimcp GTest::gtest GTest::gtest_main m Threads::Threads
    )
    gtest_discover_tests(integration_physics_thread_pool_test
        PROPERTIES LABELS "integration;physics;thread_pool;flaky" TIMEOUT 120
    )
    # FLAKY: PhysicsThreadPoolIntegrationTest.ParallelSpeedup is CPU-load sensitive
    #        and can fail under parallel ctest (-j4) due to contention
endif()

# ============================================================================
# Physics FFT Integration Tests
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_physics_fft_integration.cpp)
    add_executable(integration_physics_fft_test
        test_physics_fft_integration.cpp
    )
    target_include_directories(integration_physics_fft_test PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(integration_physics_fft_test PRIVATE
        nimcp GTest::gtest GTest::gtest_main m
    )
    gtest_discover_tests(integration_physics_fft_test
        PROPERTIES LABELS "integration;physics;fft" TIMEOUT 60
    )
endif()

# ============================================================================
# Physics Module Integration Tests (Graph Theory, Dynamics, Info Geometry)
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_physics_module_integration.cpp)
    add_executable(integration_physics_module_test
        test_physics_module_integration.cpp
    )
    target_include_directories(integration_physics_module_test PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(integration_physics_module_test PRIVATE
        nimcp GTest::gtest GTest::gtest_main m
    )
    gtest_discover_tests(integration_physics_module_test
        PROPERTIES LABELS "integration;physics;module" TIMEOUT 300
    )
endif()

message(STATUS "Added physics integration tests")
