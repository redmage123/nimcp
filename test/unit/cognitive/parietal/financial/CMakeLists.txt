# Financial Module Cognitive Integration Unit Tests (Check Framework)
# Tests for 27 financial cognitive bridges + enhanced core modules
# Part of FM Integration Plan implementation

find_package(PkgConfig REQUIRED)
pkg_check_modules(CHECK check)

if(CHECK_FOUND)
    #=========================================================================
    # Core Financial Modules Enhanced Tests
    #=========================================================================

    add_executable(unit_financial_core_modules
        test_financial_core_modules.c
    )
    target_include_directories(unit_financial_core_modules PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CHECK_INCLUDE_DIRS}
    )
    target_link_libraries(unit_financial_core_modules
        nimcp
        ${CHECK_LIBRARIES}
        pthread
        m
    )
    add_test(NAME unit_financial_core_modules
             COMMAND unit_financial_core_modules)
    set_tests_properties(unit_financial_core_modules PROPERTIES
        LABELS "unit;cognitive;parietal;financial;core;investment;market;bridge"
        TIMEOUT 180
    )

    #=========================================================================
    # Memory System Bridges Tests
    #=========================================================================

    add_executable(unit_financial_memory_bridges
        test_financial_memory_bridges.c
    )
    target_include_directories(unit_financial_memory_bridges PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CHECK_INCLUDE_DIRS}
    )
    target_link_libraries(unit_financial_memory_bridges
        nimcp
        ${CHECK_LIBRARIES}
        pthread
        m
    )
    add_test(NAME unit_financial_memory_bridges
             COMMAND unit_financial_memory_bridges)
    set_tests_properties(unit_financial_memory_bridges PROPERTIES
        LABELS "unit;cognitive;parietal;financial;memory;working-memory;mammillary;resonance;autobio"
        TIMEOUT 120
    )

    #=========================================================================
    # Cognitive Model Bridges Tests
    #=========================================================================

    add_executable(unit_financial_cognitive_bridges
        test_financial_cognitive_bridges.c
    )
    target_include_directories(unit_financial_cognitive_bridges PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CHECK_INCLUDE_DIRS}
    )
    target_link_libraries(unit_financial_cognitive_bridges
        nimcp
        ${CHECK_LIBRARIES}
        pthread
        m
    )
    add_test(NAME unit_financial_cognitive_bridges
             COMMAND unit_financial_cognitive_bridges)
    set_tests_properties(unit_financial_cognitive_bridges PROPERTIES
        LABELS "unit;cognitive;parietal;financial;world-model;tom"
        TIMEOUT 120
    )

    #=========================================================================
    # Emotional System Bridges Tests
    #=========================================================================

    add_executable(unit_financial_emotion_bridges
        test_financial_emotion_bridges.c
    )
    target_include_directories(unit_financial_emotion_bridges PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CHECK_INCLUDE_DIRS}
    )
    target_link_libraries(unit_financial_emotion_bridges
        nimcp
        ${CHECK_LIBRARIES}
        pthread
        m
    )
    add_test(NAME unit_financial_emotion_bridges
             COMMAND unit_financial_emotion_bridges)
    set_tests_properties(unit_financial_emotion_bridges PROPERTIES
        LABELS "unit;cognitive;parietal;financial;emotion;motivation;neuromod;mental-health"
        TIMEOUT 120
    )

    #=========================================================================
    # Attention and Decision Bridges Tests
    #=========================================================================

    add_executable(unit_financial_attention_decision_bridges
        test_financial_attention_decision_bridges.c
    )
    target_include_directories(unit_financial_attention_decision_bridges PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CHECK_INCLUDE_DIRS}
    )
    target_link_libraries(unit_financial_attention_decision_bridges
        nimcp
        ${CHECK_LIBRARIES}
        pthread
        m
    )
    add_test(NAME unit_financial_attention_decision_bridges
             COMMAND unit_financial_attention_decision_bridges)
    set_tests_properties(unit_financial_attention_decision_bridges PROPERTIES
        LABELS "unit;cognitive;parietal;financial;salience;attention;basal-ganglia;predictive;reasoning;jepa"
        TIMEOUT 180
    )

    #=========================================================================
    # Ethics and Learning Bridges Tests
    #=========================================================================

    add_executable(unit_financial_ethics_learning_bridges
        test_financial_ethics_learning_bridges.c
    )
    target_include_directories(unit_financial_ethics_learning_bridges PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CHECK_INCLUDE_DIRS}
    )
    target_link_libraries(unit_financial_ethics_learning_bridges
        nimcp
        ${CHECK_LIBRARIES}
        pthread
        m
    )
    add_test(NAME unit_financial_ethics_learning_bridges
             COMMAND unit_financial_ethics_learning_bridges)
    set_tests_properties(unit_financial_ethics_learning_bridges PROPERTIES
        LABELS "unit;cognitive;parietal;financial;ethics;explanations;consolidation;stdp;temporal-credit"
        TIMEOUT 180
    )

    #=========================================================================
    # Metacognition Bridges Tests
    #=========================================================================

    add_executable(unit_financial_metacognition_bridges
        test_financial_metacognition_bridges.c
    )
    target_include_directories(unit_financial_metacognition_bridges PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CHECK_INCLUDE_DIRS}
    )
    target_link_libraries(unit_financial_metacognition_bridges
        nimcp
        ${CHECK_LIBRARIES}
        pthread
        m
    )
    add_test(NAME unit_financial_metacognition_bridges
             COMMAND unit_financial_metacognition_bridges)
    set_tests_properties(unit_financial_metacognition_bridges PROPERTIES
        LABELS "unit;cognitive;parietal;financial;metacognition;uncertainty;curiosity;regret"
        TIMEOUT 120
    )

    #=========================================================================
    # Orchestrator and Fuzzy Bridge Tests
    #=========================================================================

    add_executable(unit_financial_orchestrator_fuzzy
        test_financial_orchestrator_fuzzy.c
    )
    target_include_directories(unit_financial_orchestrator_fuzzy PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CHECK_INCLUDE_DIRS}
    )
    target_link_libraries(unit_financial_orchestrator_fuzzy
        nimcp
        ${CHECK_LIBRARIES}
        pthread
        m
    )
    add_test(NAME unit_financial_orchestrator_fuzzy
             COMMAND unit_financial_orchestrator_fuzzy)
    set_tests_properties(unit_financial_orchestrator_fuzzy PROPERTIES
        LABELS "unit;cognitive;parietal;financial;orchestrator;fuzzy;cognitive-pipeline"
        TIMEOUT 180
    )

    #=========================================================================
    # Aggregate Target for All Financial Unit Tests
    #=========================================================================

    add_custom_target(unit_financial_cognitive_integration DEPENDS
        unit_financial_core_modules
        unit_financial_memory_bridges
        unit_financial_cognitive_bridges
        unit_financial_emotion_bridges
        unit_financial_attention_decision_bridges
        unit_financial_ethics_learning_bridges
        unit_financial_metacognition_bridges
        unit_financial_orchestrator_fuzzy
    )

else()
    message(STATUS "Check library not found - skipping financial cognitive integration unit tests")
endif()
