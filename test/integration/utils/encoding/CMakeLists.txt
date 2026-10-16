# ============================================================================
# Encoding Utils Integration Tests
# ============================================================================
# WHAT: CMakeLists for positional encoding integration tests
# WHY:  Test PE module integration with attention, working memory, etc.
# HOW:  Define test binaries and link against nimcp library + GTest
# ============================================================================

# Helper macro to add encoding integration test
macro(add_encoding_integration_test TEST_NAME)
    add_executable(integration_utils_encoding_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(integration_utils_encoding_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME integration_utils_encoding_${TEST_NAME}
             COMMAND integration_utils_encoding_${TEST_NAME})
endmacro()

# Positional Encoding + Attention Integration
add_encoding_integration_test(test_pe_attention_integration)
