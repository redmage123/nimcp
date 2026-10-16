# CMakeLists.txt for eidetic memory integration
# Part of the genius profiles module

set(EIDETIC_SOURCES
    nimcp_eidetic_memory.c
)

# Add sources to parent scope for inclusion in nimcp library
set(GENIUS_EIDETIC_SOURCES ${EIDETIC_SOURCES} PARENT_SCOPE)
