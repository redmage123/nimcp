# Mammillary Bodies Unit Tests
# Phase 5: Memory Systems (BR-8)
# Episodic memory, spatial memory, Papez circuit relay

macro(add_mammillary_test TEST_NAME)
    add_executable(unit_core_brain_regions_mammillary_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_core_brain_regions_mammillary_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    add_test(NAME unit_core_brain_regions_mammillary_${TEST_NAME}
             COMMAND unit_core_brain_regions_mammillary_${TEST_NAME})
endmacro()

add_mammillary_test(test_mammillary)
