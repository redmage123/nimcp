# ============================================================================
# Nitric Oxide Unit Tests
# ============================================================================
# WHAT: Unit tests for nitric oxide signaling module
# WHY:  Verify NO production, diffusion, and retrograde signaling
# HOW:  GTest-based tests for lifecycle, NOS activation, and effects

message(STATUS "Adding nitric oxide unit tests...")

# Nitric Oxide test
add_executable(unit_chemistry_gasotransmitters_test_nitric_oxide
    test_nitric_oxide.cpp
)

target_include_directories(unit_chemistry_gasotransmitters_test_nitric_oxide PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_chemistry_gasotransmitters_test_nitric_oxide
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME unit_chemistry_gasotransmitters_test_nitric_oxide
         COMMAND unit_chemistry_gasotransmitters_test_nitric_oxide)
