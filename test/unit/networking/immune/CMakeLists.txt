# Networking-Immune Bridge Unit Tests
# TODO: These tests require API updates to match implementation

option(BUILD_NETWORKING_IMMUNE_TESTS "Build networking immune tests (requires API fixes)" OFF)

if(BUILD_NETWORKING_IMMUNE_TESTS)
    # Networking Immune Bridges Unit Tests (Distributed, Protocol, P2P)
    add_executable(unit_networking_immune_bridges
        test_networking_immune_bridges.cpp
    )
    target_link_libraries(unit_networking_immune_bridges
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
    )
    target_include_directories(unit_networking_immune_bridges
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/src/utils/memory
            ${CMAKE_SOURCE_DIR}/src/utils/logging
            ${CMAKE_SOURCE_DIR}/src/cognitive/immune
            ${CMAKE_SOURCE_DIR}/src/networking
    )
    add_test(NAME unit_networking_immune_bridges
             COMMAND unit_networking_immune_bridges)
    set_tests_properties(unit_networking_immune_bridges PROPERTIES LABELS "unit")
endif()
