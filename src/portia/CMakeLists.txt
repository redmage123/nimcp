#=============================================================================
# Portia Spider Adaptive Intelligence - Build Configuration
#=============================================================================

# Portia library sources
# Core infrastructure + subsystem modules
set(PORTIA_SOURCES
    nimcp_portia.c
    nimcp_portia_swarm_bridge.c
    nimcp_portia_swarm_logic_bridge.c
    nimcp_portia_logic_bridge.c
    # Subsystem modules - enabled Dec 2024
    nimcp_portia_tier_switch.c
    nimcp_portia_sensor_fusion.c
    nimcp_portia_classification.c
    nimcp_portia_degradation.c
    nimcp_portia_accelerator.c
    nimcp_portia_attention.c
    nimcp_portia_learning.c
    nimcp_portia_planning.c
    nimcp_portia_power.c
    nimcp_portia_deception.c
    nimcp_portia_collective_bridge.c
)

# Create Portia library
add_library(nimcp_portia STATIC ${PORTIA_SOURCES})

# Include directories
target_include_directories(nimcp_portia PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/portia
)

# Link dependencies
# Note: nimcp is a monolithic library containing security, utils, async, etc.
target_link_libraries(nimcp_portia
    nimcp                 # Main NIMCP library (contains security, utils, async)
    pthread
)

# Compiler flags
target_compile_options(nimcp_portia PRIVATE
    -Wall
    -Wextra
    -Werror
    $<$<CONFIG:Debug>:-g3 -O0>
    $<$<CONFIG:Release>:-O3>
)

# Export library
set_target_properties(nimcp_portia PROPERTIES
    OUTPUT_NAME "nimcp_portia"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Installation
install(TARGETS nimcp_portia
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/portia
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# Summary
message(STATUS "Configured Portia Spider Adaptive Intelligence library (core + 10 subsystems)")
