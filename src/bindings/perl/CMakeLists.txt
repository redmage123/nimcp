# ============================================================================
# NIMCP Perl Bindings - FFI::Platypus (no compilation needed)
# ============================================================================
# WHAT: CMake target for Perl FFI bindings (uses libnimcp.so at runtime)
# WHY:  Provides Perl language bindings via FFI::Platypus
# HOW:  Syntax-checks NIMCP.pm during build; runtime links libnimcp.so
# ============================================================================

find_program(PERL_EXECUTABLE perl)

if(PERL_EXECUTABLE)
    # Verify FFI::Platypus is available
    add_custom_target(nimcp_perl ALL
        COMMAND ${CMAKE_COMMAND} -E env
                "PERL5LIB=$ENV{HOME}/perl5/lib/perl5"
                ${PERL_EXECUTABLE} -MFFI::Platypus -e "1"
        COMMAND ${CMAKE_COMMAND} -E env
                "PERL5LIB=$ENV{HOME}/perl5/lib/perl5"
                "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib"
                ${PERL_EXECUTABLE} -c ${CMAKE_CURRENT_SOURCE_DIR}/NIMCP.pm
        DEPENDS nimcp
        COMMENT "Checking NIMCP Perl bindings (FFI::Platypus)"
    )
else()
    message(STATUS "perl not found — skipping nimcp_perl target")
endif()
