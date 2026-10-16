# Biology Layer Regression Tests CMakeLists.txt

# Add neurovascular regression tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/neurovascular")
    add_subdirectory(neurovascular)
endif()
