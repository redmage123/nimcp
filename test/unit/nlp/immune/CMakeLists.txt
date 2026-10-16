# NLP-Immune Bridge Unit Tests
# TODO: These tests require API updates to match implementation

option(BUILD_NLP_IMMUNE_TESTS "Build NLP immune tests (requires API fixes)" OFF)

if(BUILD_NLP_IMMUNE_TESTS)
    # NLP Immune Bridges Unit Tests (NLP, Spike NLP, Multimodal NLP)
    add_executable(unit_nlp_immune_bridges
        test_nlp_immune_bridges.cpp
    )
    target_link_libraries(unit_nlp_immune_bridges
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
    )
    target_include_directories(unit_nlp_immune_bridges
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/src/utils/memory
            ${CMAKE_SOURCE_DIR}/src/utils/logging
            ${CMAKE_SOURCE_DIR}/src/cognitive/immune
            ${CMAKE_SOURCE_DIR}/src/nlp
    )
    add_test(NAME unit_nlp_immune_bridges
             COMMAND unit_nlp_immune_bridges)
    set_tests_properties(unit_nlp_immune_bridges PROPERTIES LABELS "unit")
endif()
