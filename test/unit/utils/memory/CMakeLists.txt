# Memory Utils Unit Tests
# Phase 0: Memory Pool, CoW Manager, Buffer Pool
# Phase 2: Brain Substrate Pools

# Helper macro to add memory test
macro(add_memory_test TEST_NAME)
    add_executable(unit_utils_memory_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_utils_memory_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME unit_utils_memory_${TEST_NAME}
             COMMAND unit_utils_memory_${TEST_NAME})
endmacro()

# Phase 0 Memory Tests
add_memory_test(test_memory_pool)
add_memory_test(test_cow_manager)
add_memory_test(test_buffer_pool)

# Phase 4 Memory Tests
add_memory_test(test_page_cow)
add_memory_test(test_memory)
add_memory_test(test_memory_alignment)
add_memory_test(test_memory_leaks)
add_memory_test(test_working_memory)
add_memory_test(test_brain_working_memory_serialization)

# Phase 2 Brain Pools Tests
add_memory_test(test_brain_pools)

# Phase 3 Layer Pools Tests
add_memory_test(test_layer_pools)

# Phase 5 COW Stress Tests
add_memory_test(test_page_cow_stress)

# Unified Memory Integration Tests
add_memory_test(test_unified_memory)

# P2 Audit: Memory wrapper tests
add_memory_test(test_nimcp_memory_wrapper)

# Atomic recursion guard tests
add_memory_test(test_memory_recursion_guard)

# Hash table tracking tests (linked list → hash table migration)
add_memory_test(test_memory_hash_tracking)
add_memory_test(test_memory_hash_integration)
