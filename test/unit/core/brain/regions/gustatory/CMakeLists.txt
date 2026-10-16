# Gustatory Cortex Unit Tests
# Phase 6: Sensory Processing (BR-11)
# Taste processing, food reward, disgust response

macro(add_gustatory_test TEST_NAME)
    add_executable(unit_core_brain_regions_gustatory_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_core_brain_regions_gustatory_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    add_test(NAME unit_core_brain_regions_gustatory_${TEST_NAME}
             COMMAND unit_core_brain_regions_gustatory_${TEST_NAME})
endmacro()

add_gustatory_test(test_gustatory)
