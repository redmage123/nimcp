#=============================================================================
# CMakeLists.txt - NLP Unit Tests Build Configuration
#=============================================================================
# WHAT: Build configuration for Neural Link Protocol unit tests
# WHY:  Enable GTest-based testing of NLP components
# HOW:  Use NIMCP standard test framework configuration
#=============================================================================

# Guard: Check GTest availability
if(NOT GTest_FOUND)
    message(STATUS "GTest not found - skipping NLP unit tests")
    return()
endif()

# Test source files
set(NLP_UNIT_TEST_SOURCES
    test_nlp_header.cpp
    test_nlp_crypto.cpp
    test_nlp_session.cpp
    test_nlp_message.cpp
    test_nlp_node.cpp
)

# Create test executables using NIMCP standard configuration
foreach(test_src ${NLP_UNIT_TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)
    set(test_target "unit_networking_nlp_${test_name}")

    # Check if source file exists
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${test_src})
        add_executable(${test_target} ${test_src})

        target_include_directories(${test_target} PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/networking/nlp
            ${GTEST_INCLUDE_DIRS}
        )

        # Enable C++17 for NIMCP tests
        target_compile_features(${test_target} PRIVATE cxx_std_17)

        # Enable test-only APIs
        target_compile_definitions(${test_target} PRIVATE NIMCP_TESTING)

        target_link_libraries(${test_target} PRIVATE
            nimcp
            GTest::GTest
            GTest::Main
            pthread
        )

        add_test(NAME ${test_target} COMMAND ${test_target})

        # Set test timeout
        set_tests_properties(${test_target} PROPERTIES TIMEOUT 60)
    else()
        message(STATUS "NLP test source not found: ${test_src}")
    endif()
endforeach()
