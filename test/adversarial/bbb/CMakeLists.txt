# ============================================================================
# BBB Adversarial / PEN Tests
# ============================================================================
# WHAT: Penetration tests for the Blood-Brain Barrier perimeter defense
# WHY:  Verify each defensive layer (input gate, code signing, memory boundary,
#       access control) refuses known classes of attacks
# HOW:  Drive malicious payloads through the BBB API and assert rejection
# ============================================================================

set(BBB_ADVERSARIAL_TESTS
    test_bbb_pentest.cpp
)

add_executable(test_bbb_pentest ${BBB_ADVERSARIAL_TESTS})

target_link_libraries(test_bbb_pentest
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        Threads::Threads
)

target_include_directories(test_bbb_pentest
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME bbb_adversarial_tests COMMAND test_bbb_pentest)

set_tests_properties(bbb_adversarial_tests PROPERTIES
    TIMEOUT 180
    LABELS "adversarial;security;bbb;pentest"
)
