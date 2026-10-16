#=============================================================================
# CMakeLists.txt - Neural Link Protocol Build Configuration
#=============================================================================
# WHAT: Build configuration for NLP module
# WHY:  Enable building the encrypted peer-to-peer brain communication protocol
# HOW:  Compile source files, link dependencies, create library target
#=============================================================================

# Source files
set(NLP_SOURCES
    nimcp_nlp.c
    nimcp_nlp_crypto.c
    nimcp_nlp_session.c
    nimcp_nlp_message.c
    nimcp_nlp_cortical_adapter.c
    nimcp_neural_language.c
    nimcp_nlp_compression.c
    nimcp_nlp_protocol_bridge.c
    nimcp_predictive_protocol.c
)

# Create library target
add_library(nimcp_nlp STATIC ${NLP_SOURCES})

# Include directories
target_include_directories(nimcp_nlp PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link dependencies
target_link_libraries(nimcp_nlp PUBLIC
    nimcp_utils
    nimcp_async
    nimcp_platform
    nimcp_perception
    pthread
    m
)

# OpenSSL for crypto (optional but recommended)
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    target_link_libraries(nimcp_nlp PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(nimcp_nlp PUBLIC NIMCP_ENABLE_OPENSSL)
    message(STATUS "NLP: Using OpenSSL for encryption")
else()
    # Try libsodium as fallback
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SODIUM QUIET libsodium)
        if(SODIUM_FOUND)
            target_include_directories(nimcp_nlp PUBLIC ${SODIUM_INCLUDE_DIRS})
            target_link_libraries(nimcp_nlp PUBLIC ${SODIUM_LIBRARIES})
            target_compile_definitions(nimcp_nlp PUBLIC NIMCP_ENABLE_SODIUM)
            message(STATUS "NLP: Using libsodium for encryption")
        else()
            message(WARNING "NLP: No crypto library found - using software fallback (NOT SECURE)")
            target_compile_definitions(nimcp_nlp PUBLIC NIMCP_CRYPTO_SOFTWARE_FALLBACK)
        endif()
    endif()
endif()

# Compiler options
target_compile_options(nimcp_nlp PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror=return-type
    -Werror=uninitialized
    -fstack-protector-strong
)

# Debug/Release specific options
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(nimcp_nlp PRIVATE -g -O0 -DDEBUG)
else()
    target_compile_options(nimcp_nlp PRIVATE -O3 -DNDEBUG)
endif()

# Export for parent CMakeLists
set(NIMCP_NLP_LIBRARY nimcp_nlp PARENT_SCOPE)
