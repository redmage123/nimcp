# Claustrum Unit Tests
# Consciousness Integration and Cross-Modal Binding
# Implements Crick's "Conductor of Consciousness" hypothesis

macro(add_claustrum_test TEST_NAME)
    add_executable(unit_core_brain_regions_claustrum_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_core_brain_regions_claustrum_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    add_test(NAME unit_core_brain_regions_claustrum_${TEST_NAME}
             COMMAND unit_core_brain_regions_claustrum_${TEST_NAME})
endmacro()

add_claustrum_test(test_claustrum)
