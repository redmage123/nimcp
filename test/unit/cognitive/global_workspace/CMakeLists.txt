# Global Workspace Unit Tests
# WHAT: Unit tests for Global Workspace Theory implementation
# WHY:  Validate workspace creation, competition, broadcasting, coalition formation
# HOW:  Test all APIs: lifecycle, competition, subscription, history, statistics

# ============================================================================
# Global Workspace Core Tests
# ============================================================================

add_executable(unit_cognitive_global_workspace
    test_global_workspace.cpp
)

target_link_libraries(unit_cognitive_global_workspace
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_global_workspace
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_global_workspace PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_global_workspace
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;global_workspace"
        TIMEOUT 120
)

# ============================================================================
# Global Workspace FEP Bridge Tests
# ============================================================================

add_executable(unit_cognitive_global_workspace_fep_bridge
    test_global_workspace_fep_bridge.cpp
)

target_link_libraries(unit_cognitive_global_workspace_fep_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_global_workspace_fep_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_global_workspace_fep_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_global_workspace_fep_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;global_workspace;fep"
        TIMEOUT 120
)

# ============================================================================
# Global Workspace Shannon Information Tests
# ============================================================================

add_executable(unit_cognitive_global_workspace_shannon
    test_global_workspace_shannon.cpp
)

target_link_libraries(unit_cognitive_global_workspace_shannon
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_global_workspace_shannon
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_global_workspace_shannon PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_global_workspace_shannon
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;global_workspace;shannon"
        TIMEOUT 120
)
