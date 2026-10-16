# ============================================================================
# Language Layer Unit Tests
# ============================================================================
# WHAT: Unit tests for Language Layer
# WHY:  Verify language orchestrator and bridges functionality
# HOW:  gtest-based tests for lifecycle, config, and operations
# ============================================================================

# ============================================================================
# Language Orchestrator Tests
# ============================================================================
add_executable(unit_language_orchestrator
    test_language_orchestrator.cpp
)
target_link_libraries(unit_language_orchestrator
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_language_orchestrator PRIVATE cxx_std_17)
gtest_discover_tests(unit_language_orchestrator
    PROPERTIES LABELS "unit;language;orchestrator")

# ============================================================================
# Language Bridges Tests
# ============================================================================
add_executable(unit_language_bridges
    test_language_bridges.cpp
)
target_link_libraries(unit_language_bridges
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_language_bridges PRIVATE cxx_std_17)
gtest_discover_tests(unit_language_bridges
    PROPERTIES LABELS "unit;language;bridges")

# ============================================================================
# Grounded Language Unit Tests
# ============================================================================
add_executable(unit_grounded_language
    test_grounded_language.cpp
)
target_link_libraries(unit_grounded_language
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_grounded_language
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_grounded_language PRIVATE cxx_std_17)
gtest_discover_tests(unit_grounded_language
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;language;grounded"
        TIMEOUT 120
)

# ============================================================================
# Grounded Language Diagnostic Accessors (collapse-fix surface)
# ============================================================================
add_executable(unit_grounded_language_diagnostics
    test_grounded_language_diagnostics.cpp
)
target_link_libraries(unit_grounded_language_diagnostics
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_grounded_language_diagnostics
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_grounded_language_diagnostics PRIVATE cxx_std_17)
gtest_discover_tests(unit_grounded_language_diagnostics
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;language;grounded;diagnostics"
        TIMEOUT 60
)

# ============================================================================
# Top-Phrases Public API Tests (curriculum-integration RPC chain)
# ============================================================================
add_executable(unit_top_phrases_public_api
    test_top_phrases_public_api.cpp
)
target_link_libraries(unit_top_phrases_public_api
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_top_phrases_public_api
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_top_phrases_public_api PRIVATE cxx_std_17)
gtest_discover_tests(unit_top_phrases_public_api
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;language;grounded;top_phrases"
        TIMEOUT 60
)

# ============================================================================
# Modality-Counts Public API Tests (curriculum-coverage probe)
# ============================================================================
add_executable(unit_modality_counts
    test_modality_counts.cpp
)
target_link_libraries(unit_modality_counts
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_modality_counts
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_modality_counts PRIVATE cxx_std_17)
gtest_discover_tests(unit_modality_counts
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;language;grounded;modality_counts"
        TIMEOUT 60
)

# ============================================================================
# ground_word_with_emotion C wrapper tests (curriculum RPC chain)
# ============================================================================
add_executable(unit_ground_word_with_emotion
    test_ground_word_with_emotion.cpp
)
target_link_libraries(unit_ground_word_with_emotion
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_ground_word_with_emotion
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_ground_word_with_emotion PRIVATE cxx_std_17)
gtest_discover_tests(unit_ground_word_with_emotion
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;language;grounded;ground_word_emotion"
        TIMEOUT 120
)

# ============================================================================
# Grounded Language Persistence Sidecar Tests (Option A)
# ============================================================================
add_executable(unit_grounded_language_persistence
    test_grounded_language_persistence.cpp
)
target_link_libraries(unit_grounded_language_persistence
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_grounded_language_persistence
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_grounded_language_persistence PRIVATE cxx_std_17)
gtest_discover_tests(unit_grounded_language_persistence
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;language;grounded;persistence"
        TIMEOUT 60
)

# ============================================================================
# Grounded Language ↔ Memory Subsystem Bridge Tests
# ============================================================================
add_executable(unit_grounded_language_memory_bridge
    test_grounded_language_memory_bridge.cpp
)
target_link_libraries(unit_grounded_language_memory_bridge
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_grounded_language_memory_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_grounded_language_memory_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_grounded_language_memory_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;language;grounded;memory_bridge"
        TIMEOUT 60
)

# ============================================================================
# Grounded Language ↔ Broca/Wernicke Region Bridge Tests
# ============================================================================
add_executable(unit_grounded_language_region_bridge
    test_grounded_language_region_bridge.cpp
)
target_link_libraries(unit_grounded_language_region_bridge
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_grounded_language_region_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_grounded_language_region_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_grounded_language_region_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;language;grounded;region_bridge"
        TIMEOUT 60
)

# ============================================================================
# Grounded Language Sleep-State Consolidation Tests
# ============================================================================
add_executable(unit_grounded_language_sleep
    test_grounded_language_sleep.cpp
)
target_link_libraries(unit_grounded_language_sleep
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_grounded_language_sleep
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_grounded_language_sleep PRIVATE cxx_std_17)
gtest_discover_tests(unit_grounded_language_sleep
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;language;grounded;sleep"
        TIMEOUT 60
)

# ============================================================================
# Grounded Language NLP Frontend Tests (morphology + POS + connect APIs)
# ============================================================================
add_executable(unit_grounded_language_nlp
    test_grounded_language_nlp.cpp
)
target_link_libraries(unit_grounded_language_nlp
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_grounded_language_nlp
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_grounded_language_nlp PRIVATE cxx_std_17)
gtest_discover_tests(unit_grounded_language_nlp
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;language;grounded;nlp"
        TIMEOUT 60
)

# ============================================================================
# Grounded Language Chunker + NER + Chinking Tests
# ============================================================================
add_executable(unit_grounded_language_chunker
    test_grounded_language_chunker.cpp
)
target_link_libraries(unit_grounded_language_chunker
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_grounded_language_chunker
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_grounded_language_chunker PRIVATE cxx_std_17)
gtest_discover_tests(unit_grounded_language_chunker
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;language;grounded;chunker;ner"
        TIMEOUT 60
)

# ============================================================================
# Grounded Language Cognitive Subscriber Bus Tests
# ============================================================================
add_executable(unit_grounded_language_cognitive_bridge
    test_grounded_language_cognitive_bridge.cpp
)
target_link_libraries(unit_grounded_language_cognitive_bridge
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_grounded_language_cognitive_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_grounded_language_cognitive_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_grounded_language_cognitive_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;language;grounded;cognitive_bridge"
        TIMEOUT 60
)

# ============================================================================
# Grounded Language Enhancement Wave (#1, #3, #4, #11, #15 + probes)
# ============================================================================
add_executable(unit_grounded_language_enhancements
    test_grounded_language_enhancements.cpp
)
target_link_libraries(unit_grounded_language_enhancements
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_grounded_language_enhancements
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_grounded_language_enhancements PRIVATE cxx_std_17)
gtest_discover_tests(unit_grounded_language_enhancements
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;language;enhancements" TIMEOUT 60
)

# ============================================================================
# Grounded Language Per-Network Bridge Tests (LNN/CNN/FNO/ANN)
# ============================================================================
add_executable(unit_grounded_language_network_bridge
    test_grounded_language_network_bridge.cpp
)
target_link_libraries(unit_grounded_language_network_bridge
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_grounded_language_network_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_grounded_language_network_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_grounded_language_network_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;language;grounded;network_bridge"
        TIMEOUT 60
)

# ============================================================================
# Grounded Language Deferred Wave 1 (#7 negative + #14 dialect + #10 needs)
# ============================================================================
add_executable(unit_grounded_language_deferred_wave1
    test_grounded_language_deferred_wave1.cpp
)
target_link_libraries(unit_grounded_language_deferred_wave1
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_grounded_language_deferred_wave1
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_grounded_language_deferred_wave1 PRIVATE cxx_std_17)
gtest_discover_tests(unit_grounded_language_deferred_wave1
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;language;deferred;wave1" TIMEOUT 60
)

# ============================================================================
# Grounded Language Deferred Wave 2 (#13 phonological + #5 LRU eviction)
# ============================================================================
add_executable(unit_grounded_language_deferred_wave2
    test_grounded_language_deferred_wave2.cpp
)
target_link_libraries(unit_grounded_language_deferred_wave2
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_grounded_language_deferred_wave2
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_grounded_language_deferred_wave2 PRIVATE cxx_std_17)
gtest_discover_tests(unit_grounded_language_deferred_wave2
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;language;deferred;wave2" TIMEOUT 60
)

# ============================================================================
# Grounded Language Deferred Wave 4 (#8 cross-modal disambiguation)
# ============================================================================
add_executable(unit_grounded_language_deferred_wave4
    test_grounded_language_deferred_wave4.cpp
)
target_link_libraries(unit_grounded_language_deferred_wave4
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_grounded_language_deferred_wave4
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_grounded_language_deferred_wave4 PRIVATE cxx_std_17)
gtest_discover_tests(unit_grounded_language_deferred_wave4
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;language;deferred;wave4" TIMEOUT 60
)

# ============================================================================
# Grounded Language Deferred Wave 5 (#9 compositional phrases)
# ============================================================================
add_executable(unit_grounded_language_deferred_wave5
    test_grounded_language_deferred_wave5.cpp
)
target_link_libraries(unit_grounded_language_deferred_wave5
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_grounded_language_deferred_wave5
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_grounded_language_deferred_wave5 PRIVATE cxx_std_17)
gtest_discover_tests(unit_grounded_language_deferred_wave5
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;language;deferred;wave5" TIMEOUT 60
)

# ============================================================================
# Grounded Language Deferred Wave 6+7+8 (#2 audio + #12 SNN spike + #6 zip)
# ============================================================================
add_executable(unit_grounded_language_deferred_wave678
    test_grounded_language_deferred_wave678.cpp
)
target_link_libraries(unit_grounded_language_deferred_wave678
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_grounded_language_deferred_wave678
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_grounded_language_deferred_wave678 PRIVATE cxx_std_17)
gtest_discover_tests(unit_grounded_language_deferred_wave678
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;language;deferred;wave678" TIMEOUT 60
)

# ============================================================================
# Grounded Language Cortex Modulation Tests
# ============================================================================
add_executable(unit_grounded_language_cortex_modulation
    test_grounded_language_cortex_modulation.cpp
)
target_link_libraries(unit_grounded_language_cortex_modulation
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_grounded_language_cortex_modulation
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_grounded_language_cortex_modulation PRIVATE cxx_std_17)
gtest_discover_tests(unit_grounded_language_cortex_modulation
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;language;grounded;cortex"
        TIMEOUT 60
)
