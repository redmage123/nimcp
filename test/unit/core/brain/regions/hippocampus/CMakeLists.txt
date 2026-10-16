# Hippocampus Unit Tests
# Episodic memory, spatial navigation, pattern separation

macro(add_hippocampus_test TEST_NAME)
    add_executable(unit_core_brain_regions_hippocampus_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_core_brain_regions_hippocampus_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    add_test(NAME unit_core_brain_regions_hippocampus_${TEST_NAME}
             COMMAND unit_core_brain_regions_hippocampus_${TEST_NAME})
endmacro()

add_hippocampus_test(test_nimcp_hippocampus_adapter)
add_hippocampus_test(test_hippocampus)
