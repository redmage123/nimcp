# Chemistry Layer Regression Tests CMakeLists.txt

# Add pH regression tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ph")
    add_subdirectory(ph)
endif()

# Add gasotransmitters regression tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/gasotransmitters")
    add_subdirectory(gasotransmitters)
endif()
