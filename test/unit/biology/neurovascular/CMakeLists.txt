# ============================================================================
# Neurovascular Coupling Unit Tests
# ============================================================================
# WHAT: Unit tests for neurovascular coupling module
# WHY:  Verify blood flow, BOLD signal, and HRF operations
# HOW:  GTest-based tests for lifecycle, CBF dynamics, and BOLD calculation

message(STATUS "Adding neurovascular coupling unit tests...")

# Neurovascular Coupling test
add_executable(unit_biology_neurovascular_test_neurovascular
    test_neurovascular.cpp
)

target_include_directories(unit_biology_neurovascular_test_neurovascular PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_biology_neurovascular_test_neurovascular
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME unit_biology_neurovascular_test_neurovascular
         COMMAND unit_biology_neurovascular_test_neurovascular)
