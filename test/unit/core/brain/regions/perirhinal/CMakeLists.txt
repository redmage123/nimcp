# Perirhinal Cortex Unit Tests
# Phase 5: Memory Systems (BR-6)
# Object recognition, familiarity detection, semantic memory

macro(add_perirhinal_test TEST_NAME)
    add_executable(unit_core_brain_regions_perirhinal_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_core_brain_regions_perirhinal_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    add_test(NAME unit_core_brain_regions_perirhinal_${TEST_NAME}
             COMMAND unit_core_brain_regions_perirhinal_${TEST_NAME})
endmacro()

add_perirhinal_test(test_perirhinal)
