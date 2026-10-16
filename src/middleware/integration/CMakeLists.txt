#=============================================================================
# Middleware Integration Layer CMakeLists
#=============================================================================
# WHAT: Build configuration for middleware-cognitive integration components
# WHY:  Compile Shannon monitor, Flow tracker, and cognitive adapters
# HOW:  Add source files, link NIMCP utils, create shared library

cmake_minimum_required(VERSION 3.10)

#=============================================================================
# Source Files
#=============================================================================

set(SHANNON_MONITOR_SOURCES
    nimcp_shannon_monitor.c
)

set(FLOW_TRACKER_SOURCES
    nimcp_flow_tracker.c
)

set(INTEGRATION_SOURCES
    ${SHANNON_MONITOR_SOURCES}
    ${FLOW_TRACKER_SOURCES}
)

#=============================================================================
# Integration Library
#=============================================================================

add_library(nimcp_middleware_integration SHARED
    ${INTEGRATION_SOURCES}
)

target_include_directories(nimcp_middleware_integration PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Link NIMCP utilities
target_link_libraries(nimcp_middleware_integration
    nimcp_utils_memory
    nimcp_utils_time
    nimcp_utils_logging
    pthread
    m  # Math library for log2f
)

# Install library
install(TARGETS nimcp_middleware_integration
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Install headers
install(FILES
    ${CMAKE_SOURCE_DIR}/include/middleware/integration/nimcp_shannon_monitor.h
    ${CMAKE_SOURCE_DIR}/include/middleware/integration/nimcp_flow_tracker.h
    DESTINATION include/middleware/integration
)
