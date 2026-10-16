# Chemistry Layer Integration Tests CMakeLists.txt

# Add pH integration tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ph")
    add_subdirectory(ph)
endif()

# Add gasotransmitters integration tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/gasotransmitters")
    add_subdirectory(gasotransmitters)
endif()
