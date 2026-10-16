# ============================================================================
# JEPA E2E Tests
# ============================================================================
# WHAT: End-to-end tests for JEPA extended predictor types
# WHY:  Verify complete TRANSFORMER and RECURRENT training pipelines,
#       loss convergence, and weight save/restore workflows
# HOW:  GTest-based tests running full training and persistence pipelines

# ----------------------------------------------------------------------------
# JEPA Predictor Extended E2E Tests
# ----------------------------------------------------------------------------
add_executable(e2e_jepa_predictor_extended
    test_jepa_predictor_extended_e2e.cpp
)

target_link_libraries(e2e_jepa_predictor_extended PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)

target_include_directories(e2e_jepa_predictor_extended PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(e2e_jepa_predictor_extended PRIVATE cxx_std_17)
target_compile_definitions(e2e_jepa_predictor_extended PRIVATE NIMCP_TESTING)

gtest_discover_tests(e2e_jepa_predictor_extended
    PROPERTIES LABELS "e2e;cognitive;jepa;predictor;extended"
    TIMEOUT 300
)
