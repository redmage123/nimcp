# Perception Integration Tests

# Cortical tests subdirectory
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cortical/CMakeLists.txt")
    add_subdirectory(cortical)
endif()

# Cochlea integration tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_cochlea_integration.cpp")
    add_executable(integration_perception_test_cochlea_integration
        test_cochlea_integration.cpp
    )
    target_link_libraries(integration_perception_test_cochlea_integration
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    target_include_directories(integration_perception_test_cochlea_integration
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )
    add_test(NAME integration_perception_test_cochlea_integration
             COMMAND integration_perception_test_cochlea_integration)
    set_tests_properties(integration_perception_test_cochlea_integration PROPERTIES
        LABELS "integration;perception;cochlea"
    )
endif()

# Exception handling tests subdirectory
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/exception/CMakeLists.txt")
    add_subdirectory(exception)
endif()

# Lip Reading integration tests (Phase B5: Audiovisual Speech Integration)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_lip_reading_integration.cpp")
    add_executable(integration_perception_test_lip_reading
        test_lip_reading_integration.cpp
    )
    target_link_libraries(integration_perception_test_lip_reading
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    target_include_directories(integration_perception_test_lip_reading
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )
    add_test(NAME integration_perception_test_lip_reading
             COMMAND integration_perception_test_lip_reading)
    set_tests_properties(integration_perception_test_lip_reading PROPERTIES
        LABELS "integration;perception;lip_reading"
    )
endif()
