# Middleware Patterns Unit Tests
# Test Oscillation Detector
add_executable(unit_middleware_patterns_oscillation_detector
    test_oscillation_detector.cpp
)
target_link_libraries(unit_middleware_patterns_oscillation_detector
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_patterns_oscillation_detector
         COMMAND unit_middleware_patterns_oscillation_detector)
# Test Pattern Library
add_executable(unit_middleware_patterns_pattern_library
    test_pattern_library.cpp
)
target_link_libraries(unit_middleware_patterns_pattern_library
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_patterns_pattern_library
         COMMAND unit_middleware_patterns_pattern_library)
# Test Sequence Detector
add_executable(unit_middleware_patterns_sequence_detector
    test_sequence_detector.cpp
)
target_link_libraries(unit_middleware_patterns_sequence_detector
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_patterns_sequence_detector
         COMMAND unit_middleware_patterns_sequence_detector)
# Test Synchrony Detector
add_executable(unit_middleware_patterns_synchrony_detector
    test_synchrony_detector.cpp
)
target_link_libraries(unit_middleware_patterns_synchrony_detector
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_patterns_synchrony_detector
         COMMAND unit_middleware_patterns_synchrony_detector)
# Test Oscillation Detector with Phasor Support
add_executable(unit_middleware_patterns_oscillation_detector_phasor
    test_oscillation_detector_phasor.cpp
)
target_link_libraries(unit_middleware_patterns_oscillation_detector_phasor
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_patterns_oscillation_detector_phasor
         COMMAND unit_middleware_patterns_oscillation_detector_phasor)
# Test All Pattern Detectors Integration
add_executable(unit_middleware_patterns_all_detectors
    test_pattern_detectors.cpp
)
target_link_libraries(unit_middleware_patterns_all_detectors
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_patterns_all_detectors
         COMMAND unit_middleware_patterns_all_detectors)
# Test Sequence Detector Positional Encoding
add_executable(unit_middleware_patterns_sequence_detector_pe
    test_sequence_detector_pe.cpp
)
target_link_libraries(unit_middleware_patterns_sequence_detector_pe
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
        m  # Math library
        pthread
    )
add_test(NAME unit_middleware_patterns_sequence_detector_pe
         COMMAND unit_middleware_patterns_sequence_detector_pe)

# Test Quantum Sequence Matcher
add_executable(unit_middleware_patterns_quantum_sequence_matcher
    test_quantum_sequence_matcher.cpp
)
target_link_libraries(unit_middleware_patterns_quantum_sequence_matcher
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
        m
        pthread
    )
target_compile_features(unit_middleware_patterns_quantum_sequence_matcher PRIVATE cxx_std_17)
add_test(NAME unit_middleware_patterns_quantum_sequence_matcher
         COMMAND unit_middleware_patterns_quantum_sequence_matcher)
