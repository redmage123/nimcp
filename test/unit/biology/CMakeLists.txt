# ============================================================================
# Biology Layer Unit Tests
# ============================================================================
# WHAT: Unit tests for biology layer modules
# WHY:  Verify epigenetics, neurogenesis, neurovascular coupling
# HOW:  Subdirectory-based test organization

message(STATUS "Adding biology layer unit tests...")

# Epigenetics tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/epigenetics/CMakeLists.txt")
    add_subdirectory(epigenetics)
endif()

# Neurogenesis tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/neurogenesis/CMakeLists.txt")
    add_subdirectory(neurogenesis)
endif()

# Neurovascular Coupling tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/neurovascular/CMakeLists.txt")
    add_subdirectory(neurovascular)
endif()

# Exception handling tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/exception/CMakeLists.txt")
    add_subdirectory(exception)
endif()
