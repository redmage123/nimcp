# ============================================================================
# Chemistry Layer Unit Tests
# ============================================================================
# WHAT: Unit tests for chemistry layer modules
# WHY:  Verify pH dynamics, gasotransmitters, and related chemical processes
# HOW:  Subdirectory-based test organization

message(STATUS "Adding chemistry layer unit tests...")

# pH Dynamics tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ph/CMakeLists.txt")
    add_subdirectory(ph)
endif()

# Gasotransmitter tests (Nitric Oxide, etc.)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/gasotransmitters/CMakeLists.txt")
    add_subdirectory(gasotransmitters)
endif()

# Exception handling tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/exception/CMakeLists.txt")
    add_subdirectory(exception)
endif()
