# ============================================================================
# Python Bindings Regression Tests
# ============================================================================
# WHAT: Regression tests for Python bindings memory management
# WHY:  Prevent memory leaks and reference counting bugs
# HOW:  Test dict building, cleanup paths, reference counting
# ============================================================================

# Guard: Check Python3 availability
if(NOT Python3_FOUND)
    message(STATUS "Python3 not found - skipping Python regression tests")
    return()
endif()

# Python Memory Regression Test
add_executable(regression_python_memory
    test_python_memory_regression.cpp
)

target_include_directories(regression_python_memory PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${Python3_INCLUDE_DIRS}
)

target_link_libraries(regression_python_memory
    PRIVATE
        GTest::GTest
        GTest::Main
        Python3::Python
        nimcp
        m
        pthread
)

target_compile_features(regression_python_memory PRIVATE cxx_std_17)

# Enable ASan for leak detection if available
if(NIMCP_ASAN)
    target_compile_options(regression_python_memory PRIVATE -fsanitize=address)
    target_link_options(regression_python_memory PRIVATE -fsanitize=address)
endif()

add_test(NAME regression_python_memory COMMAND regression_python_memory)
set_tests_properties(regression_python_memory PROPERTIES
    LABELS "regression;python;memory"
    ENVIRONMENT "PYTHONPATH=${CMAKE_BINARY_DIR}/lib/python"
)
