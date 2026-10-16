# Regression tests for Triplet STDP

add_executable(test_triplet_stdp_regression
    test_triplet_stdp_regression.cpp
)

target_include_directories(test_triplet_stdp_regression PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/plasticity
    ${CMAKE_SOURCE_DIR}/include/cognitive
    ${CMAKE_SOURCE_DIR}/include/async
    ${CMAKE_SOURCE_DIR}/include/utils
    ${GTEST_INCLUDE_DIRS}
)

target_link_libraries(test_triplet_stdp_regression
    nimcp_triplet_stdp
    nimcp_memory
    nimcp_logging
    nimcp_platform
    ${GTEST_LIBRARIES}
    pthread
)

add_test(NAME TripletSTDPRegressionTests
         COMMAND test_triplet_stdp_regression)

set_tests_properties(TripletSTDPRegressionTests PROPERTIES
    TIMEOUT 60
    LABELS "regression;plasticity;stdp;triplet"
)
