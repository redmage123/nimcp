# ============================================================================
# Autobiographical Memory Unit Tests
# ============================================================================

# Helper macro for autobiographical memory tests
macro(add_autobiographical_test TEST_NAME SOURCE_FILE)
    add_executable(unit_cognitive_autobiographical_${TEST_NAME}
        ${SOURCE_FILE}
    )
    target_link_libraries(unit_cognitive_autobiographical_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            pthread
    )
    target_include_directories(unit_cognitive_autobiographical_${TEST_NAME}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )
    add_test(NAME unit_cognitive_autobiographical_${TEST_NAME}
             COMMAND unit_cognitive_autobiographical_${TEST_NAME})
    set_tests_properties(unit_cognitive_autobiographical_${TEST_NAME} PROPERTIES LABELS "unit")
endmacro()

# Test 1: Autobiographical FEP Bridge
add_autobiographical_test(test_fep_bridge test_autobiographical_fep_bridge.cpp)
