# =============================================================================
# Distributed Unit Tests
# =============================================================================
# WHAT: CMakeLists.txt for distributed module unit tests
# WHY:  Organize distributed-specific tests
# HOW:  Include subdirectories
#
# @author NIMCP Development Team
# @date 2026-01-20
# =============================================================================

# Exception Handling Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/exception")
    add_subdirectory(exception)
endif()
