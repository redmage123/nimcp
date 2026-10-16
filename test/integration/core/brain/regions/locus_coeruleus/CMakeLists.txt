# ============================================================================
# Locus Coeruleus Integration Tests
# ============================================================================
# WHAT: Integration tests for Locus Coeruleus (LC) module
# WHY:  Verify LC integrates correctly with arousal, novelty, and projections
# HOW:  GTest-based end-to-end component integration tests

message(STATUS "Adding Locus Coeruleus integration tests...")

# LC Integration test
add_executable(integration_lc_test
    test_lc_integration.cpp
)

target_include_directories(integration_lc_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(integration_lc_test
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME integration_lc_test
         COMMAND integration_lc_test)
