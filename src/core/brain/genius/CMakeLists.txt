# Genius Profiles Module - CMakeLists.txt
# Configurable genius brain profiles based on cognitive neuroscience research

# Include eidetic subdirectory
add_subdirectory(eidetic)

set(GENIUS_SOURCES
    nimcp_genius_profiles.c
    eidetic/nimcp_eidetic_memory.c
)

set(GENIUS_HEADERS
    ${PROJECT_SOURCE_DIR}/include/core/brain/genius/nimcp_genius_types.h
    ${PROJECT_SOURCE_DIR}/include/core/brain/genius/nimcp_genius_traits.h
    ${PROJECT_SOURCE_DIR}/include/core/brain/genius/nimcp_genius_profiles.h
    ${PROJECT_SOURCE_DIR}/include/core/brain/genius/eidetic/nimcp_eidetic_memory.h
)

# Create static library
add_library(nimcp_genius STATIC ${GENIUS_SOURCES})

target_include_directories(nimcp_genius
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(nimcp_genius
    PUBLIC
        nimcp_bridge_base
        nimcp_utils
        nimcp_bio_async
    PRIVATE
        nimcp_logging
        nimcp_memory
        nimcp_platform
)

# Install headers
install(FILES ${GENIUS_HEADERS}
    DESTINATION include/core/brain/genius
)

# Install library
install(TARGETS nimcp_genius
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
