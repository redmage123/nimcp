# ============================================================================
# NIMCP Rust Bindings Test Suite
# ============================================================================

find_program(CARGO_EXECUTABLE cargo
    HINTS "$ENV{HOME}/.cargo/bin"
)

set(RUST_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(RUST_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR})

# Build test binary
add_custom_target(rust_bindings_test_compile ALL
    COMMAND ${CMAKE_COMMAND} -E env
            "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:nimcp>"
            ${CARGO_EXECUTABLE} build --release
            --manifest-path ${RUST_TEST_DIR}/Cargo.toml
            --target-dir ${RUST_BIN_DIR}/target
    DEPENDS nimcp_rust
    COMMENT "Building Rust bindings tests"
)

# CTest entry: run the compiled test binary
add_test(
    NAME rust_bindings_test
    COMMAND ${RUST_BIN_DIR}/target/release/test_nimcp_bindings
)

set_tests_properties(rust_bindings_test PROPERTIES
    TIMEOUT 120
    LABELS "bindings;rust"
    ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:nimcp>"
)
