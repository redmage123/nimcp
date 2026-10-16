# Biology Layer Integration Tests CMakeLists.txt

# Add neurovascular integration tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/neurovascular")
    add_subdirectory(neurovascular)
endif()
