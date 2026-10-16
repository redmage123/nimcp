# E2E tests for eligibility utils-quantum bidirectional bridge

add_executable(e2e_plasticity_eligibility_utils_quantum
    test_eligibility_utils_quantum_e2e.cpp
)
target_link_libraries(e2e_plasticity_eligibility_utils_quantum
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(e2e_plasticity_eligibility_utils_quantum PRIVATE cxx_std_17)
gtest_discover_tests(e2e_plasticity_eligibility_utils_quantum
    PROPERTIES
        TIMEOUT 300
        LABELS "e2e;plasticity;eligibility;utils;quantum"
)
