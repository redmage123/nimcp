# ============================================================================
# NIMCP Rust Bindings - Cargo Build Integration
# ============================================================================
# WHAT: CMake target for Rust FFI bindings (links against libnimcp.so)
# WHY:  Provides Rust language bindings via FFI
# HOW:  Builds Rust crate using cargo CLI
# ============================================================================

find_program(CARGO_EXECUTABLE cargo
    HINTS "$ENV{HOME}/.cargo/bin"
)

if(CARGO_EXECUTABLE)
    add_custom_target(nimcp_rust ALL
        COMMAND ${CMAKE_COMMAND} -E env
                "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:nimcp>"
                ${CARGO_EXECUTABLE} build --release
                --manifest-path ${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml
                --target-dir ${CMAKE_CURRENT_BINARY_DIR}/target
        DEPENDS nimcp
        COMMENT "Building NIMCP Rust bindings"
    )
else()
    message(STATUS "cargo not found — skipping nimcp_rust target")
endif()
