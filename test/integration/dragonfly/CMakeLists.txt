# ============================================================================
# Dragonfly Integration Tests
# ============================================================================
# WHAT: Integration tests for dragonfly bio-inspired target tracking
# WHY:  Verify dragonfly works correctly with brain and other subsystems
# HOW:  Test cross-module interactions, signal flow, and coordination
# ============================================================================

message(STATUS "  Adding dragonfly integration tests...")

# ============================================================================
# Brain Integration Test
# Tests dragonfly subsystem initialization and operation within brain
# ============================================================================
add_executable(integration_dragonfly_brain
    test_dragonfly_brain_integration.cpp
)

target_link_libraries(integration_dragonfly_brain
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_dragonfly_brain
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(integration_dragonfly_brain PRIVATE cxx_std_17)

gtest_discover_tests(integration_dragonfly_brain
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;dragonfly;brain"
        TIMEOUT 180
)

# ============================================================================
# Multi-Bridge Integration Test
# Tests dragonfly bridges working together in coordinated fashion
# ============================================================================
add_executable(integration_dragonfly_bridges
    test_dragonfly_multi_bridge_integration.cpp
)

target_link_libraries(integration_dragonfly_bridges
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_dragonfly_bridges
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(integration_dragonfly_bridges PRIVATE cxx_std_17)

gtest_discover_tests(integration_dragonfly_bridges
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;dragonfly;bridges"
        TIMEOUT 180
)

# ============================================================================
# Sensory Integration Test
# Tests visual and audio bridge coordination for target detection
# ============================================================================
add_executable(integration_dragonfly_sensory
    test_dragonfly_sensory_integration.cpp
)

target_link_libraries(integration_dragonfly_sensory
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_dragonfly_sensory
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(integration_dragonfly_sensory PRIVATE cxx_std_17)

gtest_discover_tests(integration_dragonfly_sensory
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;dragonfly;sensory"
        TIMEOUT 180
)

# ============================================================================
# Medulla Integration Test
# Tests dragonfly-medulla bridge modulation effects
# ============================================================================
add_executable(integration_dragonfly_medulla
    test_dragonfly_medulla_integration.cpp
)

target_link_libraries(integration_dragonfly_medulla
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_dragonfly_medulla
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(integration_dragonfly_medulla PRIVATE cxx_std_17)

gtest_discover_tests(integration_dragonfly_medulla
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;dragonfly;medulla"
        TIMEOUT 180
)

# ============================================================================
# Hunting Subsystem Integration Test
# Tests energy, learning, collision, gaze, and other hunting modules together
# ============================================================================
add_executable(integration_dragonfly_hunting
    test_dragonfly_hunting_integration.cpp
)

target_link_libraries(integration_dragonfly_hunting
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_dragonfly_hunting
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/utils
)

target_compile_features(integration_dragonfly_hunting PRIVATE cxx_std_17)

gtest_discover_tests(integration_dragonfly_hunting
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;dragonfly;hunting"
        TIMEOUT 180
)
