# Claustrum Regression Tests
# Test Slot Search Regression (false positive NIMCP_THROW_TO_IMMUNE fix)
add_executable(regression_core_brain_regions_claustrum_slot_search
    test_claustrum_slot_search_regression.cpp
)
target_link_libraries(regression_core_brain_regions_claustrum_slot_search
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)
add_test(NAME regression_core_brain_regions_claustrum_slot_search
         COMMAND regression_core_brain_regions_claustrum_slot_search)
