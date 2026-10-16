# Brain Regions Integration Tests
# Tests for various brain region integrations

# Add region subdirectories
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/broca")
    add_subdirectory(broca)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/hypothalamus")
    add_subdirectory(hypothalamus)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/locus_coeruleus")
    add_subdirectory(locus_coeruleus)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/hippocampus")
    add_subdirectory(hippocampus)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sensory")
    add_subdirectory(sensory)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/motor")
    add_subdirectory(motor)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/claustrum")
    add_subdirectory(claustrum)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ofc")
    add_subdirectory(ofc)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/pag")
    add_subdirectory(pag)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/red_nucleus")
    add_subdirectory(red_nucleus)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/reticular")
    add_subdirectory(reticular)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/retrosplenial")
    add_subdirectory(retrosplenial)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/entorhinal/CMakeLists.txt")
    add_subdirectory(entorhinal)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/perirhinal/CMakeLists.txt")
    add_subdirectory(perirhinal)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/parahippocampal/CMakeLists.txt")
    add_subdirectory(parahippocampal)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/mammillary/CMakeLists.txt")
    add_subdirectory(mammillary)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vta/CMakeLists.txt")
    add_subdirectory(vta)
endif()
