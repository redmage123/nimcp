# ============================================================================
# Neurogenesis Unit Tests
# ============================================================================
# WHAT: Unit tests for neurogenesis module
# WHY:  Verify adult neurogenesis, proliferation, migration, and integration
# HOW:  GTest-based tests for lifecycle, neural stem cells, and maturation

message(STATUS "Adding neurogenesis unit tests...")

# Neurogenesis test
add_executable(unit_biology_neurogenesis_test_neurogenesis
    test_neurogenesis.cpp
)

target_include_directories(unit_biology_neurogenesis_test_neurogenesis PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_biology_neurogenesis_test_neurogenesis
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME unit_biology_neurogenesis_test_neurogenesis
         COMMAND unit_biology_neurogenesis_test_neurogenesis)
