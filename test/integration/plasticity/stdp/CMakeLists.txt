# Integration tests for Triplet STDP

add_executable(test_triplet_stdp_integration
    test_triplet_stdp_integration.cpp
)

target_include_directories(test_triplet_stdp_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/plasticity
    ${CMAKE_SOURCE_DIR}/include/cognitive
    ${CMAKE_SOURCE_DIR}/include/async
    ${CMAKE_SOURCE_DIR}/include/utils
    ${GTEST_INCLUDE_DIRS}
)

target_link_libraries(test_triplet_stdp_integration
    nimcp_triplet_stdp
    nimcp_sleep
    nimcp_memory
    nimcp_logging
    nimcp_platform
    ${GTEST_LIBRARIES}
    pthread
)

add_test(NAME TripletSTDPIntegrationTests
         COMMAND test_triplet_stdp_integration)

set_tests_properties(TripletSTDPIntegrationTests PROPERTIES
    TIMEOUT 60
    LABELS "integration;plasticity;stdp;triplet"
)
