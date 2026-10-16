# ============================================================================
# NIMCP C# Bindings - .NET P/Invoke Library
# ============================================================================
# WHAT: CMake target for C# bindings (P/Invoke against libnimcp.so)
# WHY:  Provides C# language bindings via Platform Invoke
# HOW:  Builds .NET class library using dotnet CLI
# ============================================================================

# dotnet is optional — pods without the .NET SDK can still build
# the C/Python/etc. targets. Only build C# bindings when dotnet is
# available.
find_program(DOTNET_EXECUTABLE dotnet)

if(DOTNET_EXECUTABLE)
    add_custom_target(nimcp_csharp ALL
        COMMAND ${DOTNET_EXECUTABLE} build -c Release
                ${CMAKE_CURRENT_SOURCE_DIR}/NIMCP.csproj
                -o ${CMAKE_CURRENT_BINARY_DIR}
        DEPENDS nimcp
        COMMENT "Building NIMCP C# bindings"
    )
else()
    message(STATUS "dotnet not found — skipping nimcp_csharp target")
endif()
