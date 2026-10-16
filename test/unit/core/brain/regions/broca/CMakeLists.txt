# Broca's Region Unit Tests
# Includes immune system integration tests

# Helper macro to add Broca test
macro(add_broca_test TEST_NAME)
    add_executable(unit_core_brain_regions_broca_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_core_brain_regions_broca_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME unit_core_brain_regions_broca_${TEST_NAME}
             COMMAND unit_core_brain_regions_broca_${TEST_NAME})
endmacro()

# Broca immune integration tests
add_broca_test(test_broca_immune_integration)

# Language production PE tests (existing)
add_broca_test(test_language_production_pe)

# Core Broca tests (existing)
add_broca_test(test_nimcp_broca_adapter)
add_broca_test(test_nimcp_language_production_bridge)
add_broca_test(test_nimcp_phonological)
add_broca_test(test_nimcp_speech_motor)
add_broca_test(test_nimcp_syntax_processor)

# Broca integration bridges tests (Phase B3)
add_broca_test(test_broca_bridges)

# Speech Enhancement tests (Phase B4)
add_broca_test(test_nimcp_pragmatics_processor)
add_broca_test(test_nimcp_discourse_manager)
add_broca_test(test_nimcp_emotional_prosody)
add_broca_test(test_nimcp_incremental_processor)
add_broca_test(test_nimcp_speech_repair)
add_broca_test(test_nimcp_multimodal_language)
