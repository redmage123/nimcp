# Speech Cortex Positional Encoding Unit Tests
# Test coverage for speech cortex PE integration

# Helper macro to add speech cortex PE test
macro(add_speech_cortex_pe_test TEST_NAME)
    add_executable(unit_perception_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_perception_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
            pthread
    )
    add_test(NAME unit_perception_${TEST_NAME}
             COMMAND unit_perception_${TEST_NAME})
endmacro()

# Speech Cortex PE tests
add_speech_cortex_pe_test(test_speech_cortex_pe)

# Speech JEPA Bridge tests
add_speech_cortex_pe_test(test_speech_jepa_bridge)

# Visual JEPA Bridge tests
add_speech_cortex_pe_test(test_visual_jepa_bridge)

# Cortical tests subdirectory
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cortical/CMakeLists.txt")
    add_subdirectory(cortical)
endif()

# Cochlea core tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_cochlea_core.cpp")
    add_executable(unit_perception_test_cochlea_core
        test_cochlea_core.cpp
    )
    target_link_libraries(unit_perception_test_cochlea_core
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    target_include_directories(unit_perception_test_cochlea_core
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )
    add_test(NAME unit_perception_test_cochlea_core
             COMMAND unit_perception_test_cochlea_core)
    set_tests_properties(unit_perception_test_cochlea_core PROPERTIES
        LABELS "unit;perception;cochlea"
    )
endif()

# Bridges subdirectory
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bridges/CMakeLists.txt")
    add_subdirectory(bridges)
endif()

# Exception handling tests subdirectory
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/exception/CMakeLists.txt")
    add_subdirectory(exception)
endif()

# Lip Reading tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_lip_reading.cpp")
    add_executable(unit_perception_test_lip_reading
        test_lip_reading.cpp
    )
    target_link_libraries(unit_perception_test_lip_reading
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    target_include_directories(unit_perception_test_lip_reading
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )
    target_compile_features(unit_perception_test_lip_reading
        PRIVATE cxx_std_17
    )
    add_test(NAME unit_perception_test_lip_reading
             COMMAND unit_perception_test_lip_reading)
    set_tests_properties(unit_perception_test_lip_reading PROPERTIES
        LABELS "unit;perception;lip_reading"
        TIMEOUT 180
    )
endif()
