# ============================================================================
# Epigenetics Unit Tests
# ============================================================================
# WHAT: Unit tests for epigenetics module
# WHY:  Verify methylation, histone, chromatin, and imprinting operations
# HOW:  GTest-based tests for lifecycle, modification, and critical periods

message(STATUS "Adding epigenetics unit tests...")

# Epigenetics test
add_executable(unit_biology_epigenetics_test_epigenetics
    test_epigenetics.cpp
)

target_include_directories(unit_biology_epigenetics_test_epigenetics PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_biology_epigenetics_test_epigenetics
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME unit_biology_epigenetics_test_epigenetics
         COMMAND unit_biology_epigenetics_test_epigenetics)
