# Integration tests for Calcium Dynamics with Sleep and Immune bridges

add_executable(test_calcium_integration
    test_calcium_integration.cpp
)

target_include_directories(test_calcium_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/plasticity
    ${CMAKE_SOURCE_DIR}/include/plasticity/calcium
    ${CMAKE_SOURCE_DIR}/include/cognitive
    ${CMAKE_SOURCE_DIR}/include/cognitive/immune
    ${CMAKE_SOURCE_DIR}/include/async
    ${CMAKE_SOURCE_DIR}/include/utils
    ${GTEST_INCLUDE_DIRS}
)

target_link_libraries(test_calcium_integration
    nimcp
    ${GTEST_LIBRARIES}
    pthread
    m
)

add_test(NAME CalciumIntegrationTests
         COMMAND test_calcium_integration)

set_tests_properties(CalciumIntegrationTests PROPERTIES
    TIMEOUT 120
    LABELS "integration;plasticity;calcium;sleep;immune"
)
