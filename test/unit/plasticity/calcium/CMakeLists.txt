# Unit tests for Calcium Dynamics module

add_executable(test_calcium_dynamics
    test_calcium_dynamics.cpp
)

target_include_directories(test_calcium_dynamics PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/plasticity
    ${CMAKE_SOURCE_DIR}/include/plasticity/calcium
    ${CMAKE_SOURCE_DIR}/include/cognitive
    ${CMAKE_SOURCE_DIR}/include/async
    ${CMAKE_SOURCE_DIR}/include/utils
    ${GTEST_INCLUDE_DIRS}
)

target_link_libraries(test_calcium_dynamics
    nimcp
    ${GTEST_LIBRARIES}
    pthread
    m
)

add_test(NAME CalciumDynamicsTests
         COMMAND test_calcium_dynamics)

set_tests_properties(CalciumDynamicsTests PROPERTIES
    TIMEOUT 60
    LABELS "unit;plasticity;calcium;dynamics"
)
