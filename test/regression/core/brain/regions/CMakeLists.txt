# Brain Regions Regression Tests

# Add region subdirectories
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/broca")
    add_subdirectory(broca)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cerebellum")
    add_subdirectory(cerebellum)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/hypothalamus")
    add_subdirectory(hypothalamus)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/hippocampus")
    add_subdirectory(hippocampus)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/wernicke")
    add_subdirectory(wernicke)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/locus_coeruleus")
    add_subdirectory(locus_coeruleus)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/claustrum")
    add_subdirectory(claustrum)
endif()
