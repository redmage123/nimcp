# Red Nucleus Unit Tests
# Phase 8: Subcortical Structures (BR-25)
# Motor coordination, rubrospinal tract, cerebellar relay

macro(add_red_nucleus_test TEST_NAME)
    add_executable(unit_core_brain_regions_red_nucleus_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_core_brain_regions_red_nucleus_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    add_test(NAME unit_core_brain_regions_red_nucleus_${TEST_NAME}
             COMMAND unit_core_brain_regions_red_nucleus_${TEST_NAME})
endmacro()

add_red_nucleus_test(test_red_nucleus)
