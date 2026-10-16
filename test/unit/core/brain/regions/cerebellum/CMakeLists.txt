# Cerebellum Unit Tests
# Motor coordination, timing, and learning

macro(add_cerebellum_test TEST_NAME)
    add_executable(unit_core_brain_regions_cerebellum_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_core_brain_regions_cerebellum_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    add_test(NAME unit_core_brain_regions_cerebellum_${TEST_NAME}
             COMMAND unit_core_brain_regions_cerebellum_${TEST_NAME})
endmacro()

add_cerebellum_test(test_nimcp_cerebellum_adapter)
add_cerebellum_test(test_cerebellum_bridges)
add_cerebellum_test(test_cerebellum_synaptic_dynamics)
add_cerebellum_test(test_cerebellum_interneurons)
