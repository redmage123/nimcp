# ============================================================================
# NIMCP Go Bindings Test Suite
# ============================================================================

find_program(GO_EXECUTABLE go)

set(GO_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(GO_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR})

# Build test binary
add_custom_target(go_bindings_test_compile ALL
    COMMAND ${CMAKE_COMMAND} -E env
            "CGO_LDFLAGS=-L${CMAKE_BINARY_DIR}/lib -lnimcp -Wl,-rpath,${CMAKE_BINARY_DIR}/lib"
            "CGO_CFLAGS=-I${CMAKE_SOURCE_DIR}/include"
            ${GO_EXECUTABLE} build -o ${GO_BIN_DIR}/test_nimcp_go_bindings ./...
    WORKING_DIRECTORY ${GO_TEST_DIR}
    DEPENDS nimcp_go
    COMMENT "Building Go bindings tests"
)

# CTest entry: run the compiled test binary
add_test(
    NAME go_bindings_test
    COMMAND ${GO_BIN_DIR}/test_nimcp_go_bindings
)

set_tests_properties(go_bindings_test PROPERTIES
    TIMEOUT 120
    LABELS "bindings;go"
    ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:nimcp>"
)
