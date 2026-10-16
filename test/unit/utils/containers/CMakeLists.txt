# Utils Containers Unit Tests
# Tests for dynamic arrays, ring buffers, and other container utilities

# Helper macro to add container test
macro(add_container_test TEST_NAME)
    add_executable(unit_utils_containers_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_utils_containers_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    add_test(NAME unit_utils_containers_${TEST_NAME}
             COMMAND unit_utils_containers_${TEST_NAME})
endmacro()

# Dynamic Array Tests
add_container_test(test_darray)

# Ring Buffer Tests
add_container_test(test_ring_buffer)
