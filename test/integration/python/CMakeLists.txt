# ============================================================================
# Python-C Integration Tests
# ============================================================================
# WHAT: Integration tests for Python bindings with C library
# WHY:  Verify memory management across Python/C boundary
# HOW:  Test Python interpreter embedding and binding functionality

# Find Python for embedding
find_package(Python3 COMPONENTS Development QUIET)

if(Python3_FOUND)
    add_executable(integration_python_c
        test_python_c_integration.cpp
    )

    target_include_directories(integration_python_c PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${Python3_INCLUDE_DIRS}
    )

    target_link_libraries(integration_python_c
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            ${Python3_LIBRARIES}
            m
            pthread
    )

    target_compile_features(integration_python_c PRIVATE cxx_std_17)

    add_test(NAME integration_python_c COMMAND integration_python_c)
    set_tests_properties(integration_python_c PROPERTIES LABELS "integration;python")
else()
    message(STATUS "Python3 Development not found - skipping Python-C integration tests")
endif()
