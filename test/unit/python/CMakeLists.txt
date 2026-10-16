# ============================================================================
# Python Binding Unit Tests
# ============================================================================
# WHAT: Unit tests for Python binding memory safety patterns
# WHY:  Verify correct memory handling in Python C extension code
# HOW:  Test allocation/deallocation patterns, error handling

# Guard: Check GTest availability
if(NOT GTest_FOUND)
    message(STATUS "GTest not found - skipping Python unit tests")
    return()
endif()

# Test for Python memory safety patterns
add_test_binary(
    unit_python_test_python_memory_safety
    ${CMAKE_CURRENT_SOURCE_DIR}/test_python_memory_safety.cpp
    unit
)

# Link Python libraries if Python bindings are enabled
if(Python3_FOUND)
    target_compile_definitions(unit_python_test_python_memory_safety PRIVATE NIMCP_ENABLE_PYTHON)
    target_link_libraries(unit_python_test_python_memory_safety PRIVATE Python3::Python)
endif()

message(STATUS "  Added Python unit test: test_python_memory_safety")
