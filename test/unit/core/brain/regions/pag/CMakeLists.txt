# Periaqueductal Gray (PAG) Unit Tests
# Phase 8: Subcortical Structures (BR-24)
# Pain modulation, defensive behaviors, emotional responses

macro(add_pag_test TEST_NAME)
    add_executable(unit_core_brain_regions_pag_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_core_brain_regions_pag_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    add_test(NAME unit_core_brain_regions_pag_${TEST_NAME}
             COMMAND unit_core_brain_regions_pag_${TEST_NAME})
endmacro()

add_pag_test(test_pag)
