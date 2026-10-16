# Quantum Walk Tests

# Ternary Quantum Walk Tests
add_executable(unit_utils_quantum_test_quantum_walk_ternary
    test_quantum_walk_ternary.cpp
)
target_link_libraries(unit_utils_quantum_test_quantum_walk_ternary
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_utils_quantum_test_quantum_walk_ternary PRIVATE cxx_std_17)
gtest_discover_tests(unit_utils_quantum_test_quantum_walk_ternary
    PROPERTIES LABELS "unit;utils;quantum;walk"
)
