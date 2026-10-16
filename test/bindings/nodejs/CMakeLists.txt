# ============================================================================
# NIMCP Node.js Bindings Test Suite
# ============================================================================

find_program(NODE_EXECUTABLE node)

# CTest entry: run the Node.js test script
add_test(
    NAME nodejs_bindings_test
    COMMAND ${CMAKE_COMMAND} -E env
            "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:nimcp>"
            ${NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_nimcp.js
)

set_tests_properties(nodejs_bindings_test PROPERTIES
    TIMEOUT 120
    LABELS "bindings;nodejs"
)
