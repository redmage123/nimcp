# ============================================================================
# NIMCP Core Layer End-to-End Tests
# ============================================================================
# WHAT: Build configuration for core layer E2E tests
# WHY:  Test complete core pipelines including neural substrate
# HOW:  Include subdirectories for each core module
# ============================================================================

message(STATUS "")
message(STATUS "-- Configuring Core Layer E2E Tests")

# Include Neural Substrate E2E tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/neural_substrate/CMakeLists.txt)
    add_subdirectory(neural_substrate)
endif()

# Include Brain E2E tests (hemispheric, thalamic, regions)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/brain/CMakeLists.txt)
    add_subdirectory(brain)
endif()

# Include Geometry E2E tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/geometry/CMakeLists.txt)
    add_subdirectory(geometry)
endif()

# Include Cortical Columns E2E tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cortical_columns/CMakeLists.txt)
    add_subdirectory(cortical_columns)
endif()

# Include Medulla Oblongata E2E tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/medulla/CMakeLists.txt)
    add_subdirectory(medulla)
endif()
