# ============================================================================
# NIMCP Node.js Bindings - N-API (built via node-gyp)
# ============================================================================
# WHAT: CMake target for Node.js N-API bindings
# WHY:  Provides Node.js language bindings via N-API
# HOW:  Invokes node-gyp rebuild; links against libnimcp.so at runtime
# ============================================================================

find_program(NODE_EXECUTABLE node)
find_program(NODE_GYP node-gyp)

if(NODE_EXECUTABLE AND NODE_GYP)
    message(STATUS "Found node: ${NODE_EXECUTABLE}")
    message(STATUS "Found node-gyp: ${NODE_GYP}")

    add_custom_target(nimcp_nodejs ALL
        COMMAND ${NODE_GYP} rebuild
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS nimcp
        COMMENT "Building NIMCP Node.js bindings (N-API)"
    )
else()
    if(NOT NODE_EXECUTABLE)
        message(STATUS "node not found - skipping Node.js bindings")
    endif()
    if(NOT NODE_GYP)
        message(STATUS "node-gyp not found - skipping Node.js bindings")
    endif()
endif()
