# Visual Cortical Bridge Unit Tests
# Test coverage for visual cortical immune bridge integration

# Helper macro to add cortical test
macro(add_cortical_test TEST_NAME)
    add_executable(unit_perception_cortical_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_perception_cortical_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            pthread
    )
    add_test(NAME unit_perception_cortical_${TEST_NAME}
             COMMAND unit_perception_cortical_${TEST_NAME})
endmacro()

# Visual cortical bridge tests
add_cortical_test(test_visual_cortical_bridge)

# Audio cortical bridge tests
add_cortical_test(test_audio_cortical_bridge)

# Speech cortical bridge tests
add_cortical_test(test_speech_cortical_bridge)
