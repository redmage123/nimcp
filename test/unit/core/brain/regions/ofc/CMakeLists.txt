# ============================================================================
# OFC Unit Tests
# ============================================================================
# WHAT: Unit tests for Orbitofrontal Cortex (OFC) module
# WHY:  Verify value computation, decision making, reversal learning, and integration
# HOW:  GTest-based tests for OFC components

message(STATUS "Adding OFC unit tests...")

# Test helper include
include_directories(${CMAKE_SOURCE_DIR}/test/utils)

# OFC Core test
add_executable(unit_ofc_test
    test_ofc.cpp
)

target_include_directories(unit_ofc_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_ofc_test
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
)

add_test(NAME unit_ofc_test
         COMMAND unit_ofc_test)
