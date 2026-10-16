# ============================================================================
# NIMCP C# Bindings Test Suite
# ============================================================================

find_program(DOTNET_EXECUTABLE dotnet)

set(CSHARP_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(CSHARP_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR})

# Build test project (also builds the library via ProjectReference)
add_custom_target(csharp_bindings_test_compile ALL
    COMMAND ${DOTNET_EXECUTABLE} build -c Release
            ${CSHARP_TEST_DIR}/TestNIMCPBindings.csproj
            -o ${CSHARP_BIN_DIR}
    DEPENDS nimcp_csharp
    COMMENT "Building C# bindings tests"
)

# CTest entry: run the compiled test executable
add_test(
    NAME csharp_bindings_test
    COMMAND ${DOTNET_EXECUTABLE} ${CSHARP_BIN_DIR}/TestNIMCPBindings.dll
    WORKING_DIRECTORY ${CSHARP_BIN_DIR}
)

set_tests_properties(csharp_bindings_test PROPERTIES
    TIMEOUT 120
    LABELS "bindings;csharp"
    ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:nimcp>"
)
