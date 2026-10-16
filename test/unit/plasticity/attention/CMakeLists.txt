# Attention Positional Encoding Unit Tests
# Test coverage for attention PE integration

# Helper macro to add attention PE test
macro(add_attention_pe_test TEST_NAME)
    add_executable(unit_plasticity_attention_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_plasticity_attention_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME unit_plasticity_attention_${TEST_NAME}
             COMMAND unit_plasticity_attention_${TEST_NAME})
endmacro()

# Attention PE tests
add_attention_pe_test(test_attention_pe)

# Quantum Attention tests
add_attention_pe_test(test_quantum_attention)

# Exception Handling tests
add_attention_pe_test(test_attention_exception_handling)

# Deferred callback buffer overflow tests
add_attention_pe_test(test_attention_deferred_overflow)

# ============================================================================
# Attention Pool Thread Safety Tests
# ============================================================================
# WHAT: Thread safety tests for attention memory pool CAS-based initialization
# WHY:  Verify fix for double-checked locking race condition in pool init
# HOW:  Concurrent creation, forward passes, and entropy from multiple threads
# ============================================================================

add_executable(unit_plasticity_attention_test_attention_pool_thread_safety
    test_attention_pool_thread_safety.cpp
)
target_link_libraries(unit_plasticity_attention_test_attention_pool_thread_safety
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_plasticity_attention_test_attention_pool_thread_safety PRIVATE cxx_std_17)
gtest_discover_tests(unit_plasticity_attention_test_attention_pool_thread_safety
    PROPERTIES
        TIMEOUT 120
        LABELS "unit;plasticity;attention;thread_safety"
)
