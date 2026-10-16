# Reticular Formation Unit Tests
# Phase 8: Subcortical Structures (BR-26)
# Arousal, attention, sleep-wake cycles, autonomic regulation

macro(add_reticular_test TEST_NAME)
    add_executable(unit_core_brain_regions_reticular_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_core_brain_regions_reticular_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    add_test(NAME unit_core_brain_regions_reticular_${TEST_NAME}
             COMMAND unit_core_brain_regions_reticular_${TEST_NAME})
endmacro()

add_reticular_test(test_reticular)
