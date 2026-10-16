# Retrosplenial Cortex Unit Tests
# Phase 5: Memory Circuit

# Test source files
set(RETROSPLENIAL_TEST_SOURCES
    test_retrosplenial.cpp
)

# Create test executable
add_executable(test_retrosplenial ${RETROSPLENIAL_TEST_SOURCES})

# Include directories
target_include_directories(test_retrosplenial PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

# Link against the main library and GTest
target_link_libraries(test_retrosplenial PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
    m
)

# Add test to CTest
gtest_discover_tests(test_retrosplenial
    PROPERTIES
        LABELS "unit;brain;regions;retrosplenial"
)
