# CMakeLists.txt for Core Integration Tests
# Only add new subdirectories here - existing ones are added by test/CMakeLists.txt

# =============================================================================
# Neural Substrate Integration Tests
# =============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/neural_substrate/CMakeLists.txt)
    message(STATUS "Adding Neural Substrate Integration tests...")
    add_subdirectory(neural_substrate)
endif()

# Cognitive dispatch integration tests
