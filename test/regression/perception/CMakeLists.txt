# Perception Regression Tests

# Cortical tests subdirectory
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cortical/CMakeLists.txt")
    add_subdirectory(cortical)
endif()

# Lip Reading backward compatibility tests (Phase B5)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_lip_reading_backward_compat.cpp")
    add_executable(regression_perception_test_lip_reading_backward_compat
        test_lip_reading_backward_compat.cpp
    )
    target_link_libraries(regression_perception_test_lip_reading_backward_compat
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    target_include_directories(regression_perception_test_lip_reading_backward_compat
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )
    add_test(NAME regression_perception_test_lip_reading_backward_compat
             COMMAND regression_perception_test_lip_reading_backward_compat)
    set_tests_properties(regression_perception_test_lip_reading_backward_compat PROPERTIES
        LABELS "regression;perception;lip_reading"
    )
endif()

# Lip Reading signal regression tests (Phase B5)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_lip_reading_signal_regression.cpp")
    add_executable(regression_perception_test_lip_reading_signal_regression
        test_lip_reading_signal_regression.cpp
    )
    target_link_libraries(regression_perception_test_lip_reading_signal_regression
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    target_include_directories(regression_perception_test_lip_reading_signal_regression
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )
    add_test(NAME regression_perception_test_lip_reading_signal_regression
             COMMAND regression_perception_test_lip_reading_signal_regression)
    set_tests_properties(regression_perception_test_lip_reading_signal_regression PROPERTIES
        LABELS "regression;perception;lip_reading"
    )
endif()
