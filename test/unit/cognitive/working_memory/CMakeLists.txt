# Working Memory Positional Encoding Unit Tests
# Test coverage for working memory PE integration

# Helper macro to add working memory PE test
macro(add_working_memory_pe_test TEST_NAME)
    add_executable(unit_cognitive_working_memory_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_cognitive_working_memory_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME unit_cognitive_working_memory_${TEST_NAME}
             COMMAND unit_cognitive_working_memory_${TEST_NAME})
    set_tests_properties(unit_cognitive_working_memory_${TEST_NAME} PROPERTIES
        TIMEOUT 600
    )
endmacro()

# Working Memory PE tests
add_working_memory_pe_test(test_working_memory_pe)

# Substrate bridge tests
add_subdirectory(substrate)
