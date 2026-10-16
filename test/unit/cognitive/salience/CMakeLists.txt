# ============================================================================
# Salience Unit Tests
# ============================================================================
# WHAT: Unit tests for Salience SNN and Plasticity bridges
# WHY:  Ensure correctness of attention/salience neural integration
# HOW:  GoogleTest-based tests covering all API functions

# ============================================================================
# Salience SNN Bridge Unit Tests
# ============================================================================

add_executable(unit_cognitive_salience_snn_bridge
    test_salience_snn_bridge.cpp
)

target_link_libraries(unit_cognitive_salience_snn_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_salience_snn_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_cognitive_salience_snn_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_salience_snn_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;salience;snn"
        TIMEOUT 60
)

# ============================================================================
# Salience Plasticity Bridge Unit Tests
# ============================================================================

add_executable(unit_cognitive_salience_plasticity_bridge
    test_salience_plasticity_bridge.cpp
)

target_link_libraries(unit_cognitive_salience_plasticity_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_salience_plasticity_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_cognitive_salience_plasticity_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_salience_plasticity_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;salience;plasticity"
        TIMEOUT 60
)

# ============================================================================
# Surprise Amplifier Unit Tests (Society of Thought Phase 1)
# ============================================================================

add_executable(unit_cognitive_surprise_amplifier
    test_surprise_amplifier.cpp
)

target_link_libraries(unit_cognitive_surprise_amplifier
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_surprise_amplifier
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_cognitive_surprise_amplifier PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_surprise_amplifier
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;salience;surprise;society_of_thought"
        TIMEOUT 60
)

# ============================================================================
# Surprise Bridges Unit Tests (Society of Thought Phase 1)
# ============================================================================

add_executable(unit_cognitive_surprise_bridges
    test_surprise_bridges.cpp
)

target_link_libraries(unit_cognitive_surprise_bridges
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_surprise_bridges
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_cognitive_surprise_bridges PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_surprise_bridges
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;salience;surprise;bridges;society_of_thought"
        TIMEOUT 60
)

# ============================================================================
# Surprise Bridges Phase 1b Unit Tests (Society of Thought)
# ============================================================================

add_executable(unit_cognitive_surprise_bridges_phase1b
    test_surprise_bridges_phase1b.cpp
)

target_link_libraries(unit_cognitive_surprise_bridges_phase1b
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_surprise_bridges_phase1b
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_cognitive_surprise_bridges_phase1b PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_surprise_bridges_phase1b
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;salience;surprise;bridges;phase1b;society_of_thought"
        TIMEOUT 60
)
