# ============================================================================
# NIMCP Perl Bindings Test Suite
# ============================================================================

find_program(PERL_EXECUTABLE perl)

# CTest entry: run the Perl test script
add_test(
    NAME perl_bindings_test
    COMMAND ${CMAKE_COMMAND} -E env
            "PERL5LIB=$ENV{HOME}/perl5/lib/perl5"
            "NIMCP_PM_DIR=${CMAKE_SOURCE_DIR}/src/bindings/perl"
            "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:nimcp>"
            ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_nimcp.pl
)

set_tests_properties(perl_bindings_test PROPERTIES
    TIMEOUT 120
    LABELS "bindings;perl"
)
