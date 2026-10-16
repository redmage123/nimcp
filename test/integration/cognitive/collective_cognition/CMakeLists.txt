# ============================================================================
# Collective Cognition Integration Tests
# ============================================================================
# Tests integration of collective cognition with:
# - Training layer (meta-learning, federated learning)
# - Cognitive layer (emotion, attention, memory, ToM)
# - Logic gate module (distributed reasoning)
# - A/V/S cortexes (shared perception)
# - Neural substrate (metabolic effects)
# - Thalamic/middleware layer (routing, gating)
# - Swarm/dragonfly modules (multi-agent coordination)
# - Cortical columns (hierarchical processing)
# - Portia module (adaptive intelligence)
# ============================================================================

# ----------------------------------------------------------------------------
# Test 1: Collective Cognition + Training Layer Integration
# ----------------------------------------------------------------------------
add_executable(integration_collective_cognition_training
    test_collective_cognition_training_integration.cpp
)

target_link_libraries(integration_collective_cognition_training
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_collective_cognition_training
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_collective_cognition_training PRIVATE cxx_std_17)

gtest_discover_tests(integration_collective_cognition_training
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;collective_cognition;training"
        TIMEOUT 180
)

# ----------------------------------------------------------------------------
# Test 2: Collective Cognition + Cognitive Layer Integration
# ----------------------------------------------------------------------------
add_executable(integration_collective_cognition_cognitive
    test_collective_cognition_cognitive_integration.cpp
)

target_link_libraries(integration_collective_cognition_cognitive
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_collective_cognition_cognitive
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_collective_cognition_cognitive PRIVATE cxx_std_17)

gtest_discover_tests(integration_collective_cognition_cognitive
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;collective_cognition;emotion;attention;memory"
        TIMEOUT 180
)

# ----------------------------------------------------------------------------
# Test 3: Collective Cognition + Logic Gate Integration
# ----------------------------------------------------------------------------
add_executable(integration_collective_cognition_logic
    test_collective_cognition_logic_integration.cpp
)

target_link_libraries(integration_collective_cognition_logic
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_collective_cognition_logic
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_collective_cognition_logic PRIVATE cxx_std_17)

gtest_discover_tests(integration_collective_cognition_logic
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;collective_cognition;logic;reasoning"
        TIMEOUT 180
)

# ----------------------------------------------------------------------------
# Test 4: Collective Cognition + Sensory Cortex Integration (A/V/S)
# ----------------------------------------------------------------------------
add_executable(integration_collective_cognition_cortex
    test_collective_cognition_cortex_integration.cpp
)

target_link_libraries(integration_collective_cognition_cortex
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_collective_cognition_cortex
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_collective_cognition_cortex PRIVATE cxx_std_17)

gtest_discover_tests(integration_collective_cognition_cortex
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;collective_cognition;visual;auditory;somatosensory"
        TIMEOUT 180
)

# ----------------------------------------------------------------------------
# Test 5: Collective Cognition + Neural Substrate Integration
# ----------------------------------------------------------------------------
add_executable(integration_collective_cognition_substrate
    test_collective_cognition_substrate_integration.cpp
)

target_link_libraries(integration_collective_cognition_substrate
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_collective_cognition_substrate
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_collective_cognition_substrate PRIVATE cxx_std_17)

gtest_discover_tests(integration_collective_cognition_substrate
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;collective_cognition;substrate;metabolic"
        TIMEOUT 180
)

# ----------------------------------------------------------------------------
# Test 6: Collective Cognition + Thalamic/Middleware Integration
# ----------------------------------------------------------------------------
add_executable(integration_collective_cognition_thalamic
    test_collective_cognition_thalamic_integration.cpp
)

target_link_libraries(integration_collective_cognition_thalamic
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_collective_cognition_thalamic
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_collective_cognition_thalamic PRIVATE cxx_std_17)

gtest_discover_tests(integration_collective_cognition_thalamic
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;collective_cognition;thalamic;middleware"
        TIMEOUT 180
)

# ----------------------------------------------------------------------------
# Test 7: Collective Cognition + Swarm/Dragonfly Integration
# ----------------------------------------------------------------------------
add_executable(integration_collective_cognition_swarm
    test_collective_cognition_swarm_integration.cpp
)

target_link_libraries(integration_collective_cognition_swarm
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_collective_cognition_swarm
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_collective_cognition_swarm PRIVATE cxx_std_17)

gtest_discover_tests(integration_collective_cognition_swarm
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;collective_cognition;swarm;dragonfly"
        TIMEOUT 240
)

# ----------------------------------------------------------------------------
# Test 8: Collective Cognition + Cortical Columns Integration
# ----------------------------------------------------------------------------
add_executable(integration_collective_cognition_cortical_columns
    test_collective_cognition_cortical_columns_integration.cpp
)

target_link_libraries(integration_collective_cognition_cortical_columns
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_collective_cognition_cortical_columns
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_collective_cognition_cortical_columns PRIVATE cxx_std_17)

gtest_discover_tests(integration_collective_cognition_cortical_columns
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;collective_cognition;cortical_columns"
        TIMEOUT 180
)

# ----------------------------------------------------------------------------
# Test 9: Collective Cognition + Portia Integration
# ----------------------------------------------------------------------------
add_executable(integration_collective_cognition_portia
    test_collective_cognition_portia_integration.cpp
)

target_link_libraries(integration_collective_cognition_portia
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_collective_cognition_portia
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_collective_cognition_portia PRIVATE cxx_std_17)

gtest_discover_tests(integration_collective_cognition_portia
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;collective_cognition;portia"
        TIMEOUT 240
)

# ----------------------------------------------------------------------------
# Test 10: Collective Cognition + FEP Orchestrator Integration
# ----------------------------------------------------------------------------
add_executable(integration_collective_fep
    test_collective_fep_integration.cpp
)

target_link_libraries(integration_collective_fep
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_collective_fep
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_collective_fep PRIVATE cxx_std_17)

gtest_discover_tests(integration_collective_fep
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;collective_cognition;fep;orchestrator"
        TIMEOUT 180
)

# ----------------------------------------------------------------------------
# Test 11: Collective Cognition + Cognitive Hub Integration
# ----------------------------------------------------------------------------
add_executable(integration_collective_hub
    test_collective_hub_integration.cpp
)

target_link_libraries(integration_collective_hub
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_collective_hub
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(integration_collective_hub PRIVATE cxx_std_17)

gtest_discover_tests(integration_collective_hub
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;collective_cognition;hub;coordination"
        TIMEOUT 180
)

# ----------------------------------------------------------------------------
# Test 12: Full Collective Cognition Cross-Module Integration
# TODO: Create test_collective_cognition_full_integration.cpp
# ----------------------------------------------------------------------------
# add_executable(integration_collective_cognition_full
#     test_collective_cognition_full_integration.cpp
# )
#
# target_link_libraries(integration_collective_cognition_full
#     PRIVATE
#         nimcp
#         GTest::gtest
#         GTest::gtest_main
#         Threads::Threads
#         m
# )
#
# target_include_directories(integration_collective_cognition_full
#     PRIVATE
#         ${PROJECT_SOURCE_DIR}/include
#         ${PROJECT_SOURCE_DIR}/src
#         ${PROJECT_SOURCE_DIR}/test
# )
#
# target_compile_features(integration_collective_cognition_full PRIVATE cxx_std_17)
#
# gtest_discover_tests(integration_collective_cognition_full
#     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#     PROPERTIES
#         LABELS "integration;cognitive;collective_cognition;full"
#         TIMEOUT 300
# )
