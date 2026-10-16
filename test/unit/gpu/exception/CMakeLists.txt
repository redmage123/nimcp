# GPU Exception Handling Unit Tests
#
# WHAT: CMake configuration for GPU exception unit tests
# WHY:  Verify GPU-specific exception handling works correctly
# HOW:  Build and link test executable against GTest and nimcp

# GPU Exception Handling Unit Tests
add_executable(unit_gpu_exception_handling
    test_gpu_exception_handling.cpp
)

target_link_libraries(unit_gpu_exception_handling
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)

target_include_directories(unit_gpu_exception_handling
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

# Add compile definitions for testing
target_compile_definitions(unit_gpu_exception_handling
    PRIVATE
        NIMCP_TESTING=1
)

add_test(
    NAME unit_gpu_exception_handling
    COMMAND unit_gpu_exception_handling
)

set_tests_properties(unit_gpu_exception_handling
    PROPERTIES
        LABELS "unit;gpu;exception"
        TIMEOUT 60
)
