# Unit tests for heterosynaptic plasticity

# Test executable
add_executable(unit_plasticity_heterosynaptic
    test_heterosynaptic.cpp
)

target_link_libraries(unit_plasticity_heterosynaptic
    PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(unit_plasticity_heterosynaptic
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Register test
add_test(
    NAME unit_plasticity_heterosynaptic
    COMMAND unit_plasticity_heterosynaptic --gtest_brief=1
)
