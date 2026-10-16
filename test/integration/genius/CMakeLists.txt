# =============================================================================
# Integration Tests for Genius Profiles Module
# =============================================================================
# WHAT: Integration tests for genius profiles cross-system communication
# WHY:  Verify genius profiles work with bio-async, memory systems, etc.
# HOW:  GTest-based integration tests
# =============================================================================

# Genius Profiles Integration Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_genius_integration.cpp")
    add_executable(integration_genius_profiles
        ${CMAKE_CURRENT_SOURCE_DIR}/test_genius_integration.cpp
    )

    target_link_libraries(integration_genius_profiles
        PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )

    target_include_directories(integration_genius_profiles
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    target_compile_features(integration_genius_profiles PRIVATE cxx_std_17)
    target_compile_definitions(integration_genius_profiles PRIVATE NIMCP_TESTING)

    gtest_discover_tests(integration_genius_profiles
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "integration;genius;profiles"
            TIMEOUT 180
    )

    message(STATUS "  Added Integration test: integration_genius_profiles")
endif()

# Eidetic Memory Integration Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_eidetic_integration.cpp")
    add_executable(integration_eidetic_memory
        ${CMAKE_CURRENT_SOURCE_DIR}/test_eidetic_integration.cpp
    )

    target_link_libraries(integration_eidetic_memory
        PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )

    target_include_directories(integration_eidetic_memory
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    target_compile_features(integration_eidetic_memory PRIVATE cxx_std_17)
    target_compile_definitions(integration_eidetic_memory PRIVATE NIMCP_TESTING)

    gtest_discover_tests(integration_eidetic_memory
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "integration;genius;eidetic"
            TIMEOUT 180
    )

    message(STATUS "  Added Integration test: integration_eidetic_memory")
endif()

# Brain Factory Integration Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_brain_factory_integration.cpp")
    add_executable(integration_genius_brain_factory
        ${CMAKE_CURRENT_SOURCE_DIR}/test_brain_factory_integration.cpp
    )

    target_link_libraries(integration_genius_brain_factory
        PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )

    target_include_directories(integration_genius_brain_factory
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    target_compile_features(integration_genius_brain_factory PRIVATE cxx_std_17)
    target_compile_definitions(integration_genius_brain_factory PRIVATE NIMCP_TESTING)

    gtest_discover_tests(integration_genius_brain_factory
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "integration;genius;brain_factory"
            TIMEOUT 900
    )

    message(STATUS "  Added Integration test: integration_genius_brain_factory")
endif()

# Mesh Integration Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_integration.cpp")
    add_executable(integration_genius_mesh
        ${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_integration.cpp
    )

    target_link_libraries(integration_genius_mesh
        PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )

    target_include_directories(integration_genius_mesh
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    target_compile_features(integration_genius_mesh PRIVATE cxx_std_17)
    target_compile_definitions(integration_genius_mesh PRIVATE NIMCP_TESTING)

    gtest_discover_tests(integration_genius_mesh
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "integration;genius;mesh"
            TIMEOUT 180
    )

    message(STATUS "  Added Integration test: integration_genius_mesh")
endif()

# =============================================================================
# Custom Target: Run All Genius Integration Tests
# =============================================================================
add_custom_target(test_genius_integration
    COMMAND ${CMAKE_CTEST_COMMAND} -L "integration" -L "genius" --output-on-failure
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all genius profiles integration tests"
)
