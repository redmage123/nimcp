# Core Brain Regions Unit Tests

# ============================================================================
# Core Exception Macros Unit Tests
# ============================================================================
# WHAT: Unit tests for NIMCP exception macro integration in core modules
# WHY:  Verify NIMCP_CHECK_THROW, NIMCP_THROW work correctly across core subsystem
# HOW:  Test null pointer handling, invalid parameters, and valid inputs (axon, etc.)
# ============================================================================

add_executable(unit_core_exception_macros
    test_core_exception_macros.cpp
)
target_link_libraries(unit_core_exception_macros
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_core_exception_macros PRIVATE cxx_std_17)
target_include_directories(unit_core_exception_macros
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
gtest_discover_tests(unit_core_exception_macros
    PROPERTIES LABELS "unit;core;exception;macros"
    TIMEOUT 120
)

# Brain Regions subdirectory (PE tests)
add_subdirectory(brain_regions)

# Core Directives subdirectory (Action history, Asimov's Laws integration)
add_subdirectory(directives)

# Medulla Oblongata subdirectory (Arousal, Protection, Circadian)
add_subdirectory(medulla)

# Wernicke's Area subdirectory (Language comprehension)
# Note: wernicke subdirectory is added by test/CMakeLists.txt

# ============================================================================
# Sleep Bridge Integration Tests
# ============================================================================
# WHAT: Unit tests for Sleep-Core integration bridges
# WHY:  Ensure core brain systems correctly modulate based on sleep state
# HOW:  Test helper functions, default config, biological validity
# ============================================================================

# Oscillations Sleep Bridge Tests
add_executable(unit_core_oscillations_sleep_bridge
    brain_oscillations/sleep/test_oscillations_sleep_bridge.cpp
)
target_link_libraries(unit_core_oscillations_sleep_bridge
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_core_oscillations_sleep_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_core_oscillations_sleep_bridge
    PROPERTIES LABELS "unit;core;oscillations;sleep")

# ============================================================================
# Brain KG Message Index Tests (Phase 6: KG Query Optimization)
# ============================================================================
# WHAT: Unit tests for message-type index in brain knowledge graph
# WHY:  Verify efficient O(1) handler lookup by message type
# HOW:  Test CRUD operations on message-type index
# ============================================================================

add_executable(unit_core_brain_kg_message_index
    brain/test_brain_kg_message_index.cpp
)
target_link_libraries(unit_core_brain_kg_message_index
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_core_brain_kg_message_index PRIVATE cxx_std_17)
gtest_discover_tests(unit_core_brain_kg_message_index
    PROPERTIES LABELS "unit;core;brain_kg;phase6")

# ============================================================================
# PR Memory Quantum Bridge Tests (Phase 5: Utils/Quantum Integration)
# ============================================================================
# WHAT: Unit tests for PR Memory quantum integration bridge
# WHY:  Verify quantum algorithms (walk, annealing, Shannon) work with PR Memory
# HOW:  Test quantum-enhanced memory operations and pattern retrieval
# ============================================================================

add_executable(unit_core_pr_memory_quantum_bridge
    brain/internal/test_pr_memory_quantum_bridge.cpp
)
target_link_libraries(unit_core_pr_memory_quantum_bridge
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_core_pr_memory_quantum_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_core_pr_memory_quantum_bridge
    PROPERTIES LABELS "unit;core;brain;internal;pr_memory;quantum")

# ============================================================================
# KG Hierarchy Tests (4-Level Hierarchical View)
# ============================================================================
# WHAT: Unit tests for hierarchical view of brain knowledge graph
# WHY:  Verify 4-level hierarchy (Brain -> Hemisphere -> Layer -> Module)
# HOW:  Test lifecycle, queries at each level, real-time updates, thread safety
# ============================================================================

add_executable(unit_core_brain_kg_hierarchy
    brain/test_kg_hierarchy.cpp
)
target_link_libraries(unit_core_brain_kg_hierarchy
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_core_brain_kg_hierarchy PRIVATE cxx_std_17)
gtest_discover_tests(unit_core_brain_kg_hierarchy
    PROPERTIES LABELS "unit;core;brain_kg;hierarchy")

# ============================================================================
# Synapse Compute Bug Fix Tests (C7, H15, H16)
# ============================================================================
# WHAT: Unit tests for synapse compute + multimodal integration bug fixes
# WHY:  Verify dopamine phasic-tonic accessor, expf overflow, thread safety
# HOW:  Check framework C tests
# ============================================================================

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/synapse_compute/test_synapse_compute_bugs.c)
    add_executable(unit_core_synapse_compute_bugs
        synapse_compute/test_synapse_compute_bugs.c
    )
    target_include_directories(unit_core_synapse_compute_bugs PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(unit_core_synapse_compute_bugs PRIVATE c_std_11)
    target_compile_options(unit_core_synapse_compute_bugs PRIVATE
        -Wall -Wextra
    )
    target_link_libraries(unit_core_synapse_compute_bugs PRIVATE
        nimcp Threads::Threads check_pic subunit rt m
    )
    add_test(NAME unit_core_synapse_compute_bugs
             COMMAND unit_core_synapse_compute_bugs
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(unit_core_synapse_compute_bugs
        PROPERTIES
            LABELS "unit;core;synapse_compute;multimodal;bugfix"
            TIMEOUT 120)
endif()

