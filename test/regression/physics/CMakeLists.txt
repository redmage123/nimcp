# ============================================================================
# Physics Regression Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for physics module regression tests
# WHY:  Verify physics modules maintain consistent behavior across changes
# HOW:  Use GoogleTest framework with linked physics modules
# ============================================================================

# Find GoogleTest
find_package(GTest QUIET)

if(NOT GTest_FOUND)
    message(STATUS "GoogleTest not found - skipping physics regression tests")
    return()
endif()

# Include subdirectories
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/biophysics)
    add_subdirectory(biophysics)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/thermodynamics)
    add_subdirectory(thermodynamics)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ephaptic)
    add_subdirectory(ephaptic)
endif()

# ============================================================================
# Physics Bridges Regression Tests
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_physics_bridges_regression.cpp)
    add_executable(regression_physics_bridges_test
        test_physics_bridges_regression.cpp
    )
    target_include_directories(regression_physics_bridges_test PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(regression_physics_bridges_test PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(regression_physics_bridges_test PRIVATE cxx_std_17)
    gtest_discover_tests(regression_physics_bridges_test
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;physics;bridges"
            TIMEOUT 120
    )
    message(STATUS "Added physics bridges regression test")
endif()

# ============================================================================
# Physics Modules Regression Tests (Graph Theory, Dynamical Systems, Info Geometry)
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_physics_modules_regression.cpp)
    add_executable(regression_physics_modules_test
        test_physics_modules_regression.cpp
    )
    target_include_directories(regression_physics_modules_test PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(regression_physics_modules_test PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(regression_physics_modules_test PRIVATE cxx_std_17)
    gtest_discover_tests(regression_physics_modules_test
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;physics;modules"
            TIMEOUT 180
    )
    message(STATUS "Added physics modules regression test")
endif()
