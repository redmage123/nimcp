# ============================================================================
# Collective Cognition Unit Tests
# ============================================================================

# ============================================================================
# Test: Collective Cognition Main System
# ============================================================================
add_executable(unit_cognitive_test_collective_cognition
    test_nimcp_collective_cognition.cpp
)

target_link_libraries(unit_cognitive_test_collective_cognition
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_collective_cognition
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_collective_cognition PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_collective_cognition
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;collective_cognition"
        TIMEOUT 120
)

# ============================================================================
# Test: Hyperscanning (Inter-brain synchronization)
# ============================================================================
add_executable(unit_cognitive_test_hyperscanning
    test_nimcp_hyperscanning.cpp
)

target_link_libraries(unit_cognitive_test_hyperscanning
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_hyperscanning
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_hyperscanning PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_hyperscanning
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;collective_cognition;hyperscanning"
        TIMEOUT 120
)

# ============================================================================
# Test: Extended Mind (External cognitive extensions)
# ============================================================================
add_executable(unit_cognitive_test_extended_mind
    test_nimcp_extended_mind.cpp
)

target_link_libraries(unit_cognitive_test_extended_mind
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_extended_mind
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_extended_mind PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_extended_mind
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;collective_cognition;extended_mind"
        TIMEOUT 120
)

# ============================================================================
# Test: Collective Phi (IIT metrics)
# ============================================================================
add_executable(unit_cognitive_test_collective_phi
    test_nimcp_collective_phi.cpp
)

target_link_libraries(unit_cognitive_test_collective_phi
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_collective_phi
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_collective_phi PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_collective_phi
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;collective_cognition;phi"
        TIMEOUT 120
)

# ============================================================================
# Test: Shared Intentionality (Joint goals and we-mode)
# ============================================================================
add_executable(unit_cognitive_test_shared_intentionality
    test_nimcp_shared_intentionality.cpp
)

target_link_libraries(unit_cognitive_test_shared_intentionality
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_shared_intentionality
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_shared_intentionality PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_shared_intentionality
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;collective_cognition;intentionality"
        TIMEOUT 120
)

# ============================================================================
# Test: Collective FEP Bridge (Free Energy Principle integration)
# ============================================================================
add_executable(unit_cognitive_test_collective_fep_bridge
    test_collective_fep_bridge.cpp
)

target_link_libraries(unit_cognitive_test_collective_fep_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_test_collective_fep_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_cognitive_test_collective_fep_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_test_collective_fep_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;collective_cognition;fep;bridge"
        TIMEOUT 120
)
