# Olfactory Cortex Unit Tests
# Phase 6: Sensory Processing (BR-10)
# Smell, odor identification, olfactory memory

macro(add_olfactory_test TEST_NAME)
    add_executable(unit_core_brain_regions_olfactory_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_core_brain_regions_olfactory_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    add_test(NAME unit_core_brain_regions_olfactory_${TEST_NAME}
             COMMAND unit_core_brain_regions_olfactory_${TEST_NAME})
endmacro()

add_olfactory_test(test_olfactory)
