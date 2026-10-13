# Python module
Python3_add_library(nimcp_python MODULE
    nimcp_module.c
)

set_target_properties(nimcp_python 
    PROPERTIES
        PREFIX ""
        OUTPUT_NAME "nimcp"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/python"
        POSITION_INDEPENDENT_CODE ON
)

target_include_directories(nimcp_python
    PUBLIC  # Changed from PRIVATE to PUBLIC
        ${NIMCP_INCLUDE_DIR}
        ${NIMCP_LIB_DIR}
        ${Python3_INCLUDE_DIRS}
)

target_link_libraries(nimcp_python
    PUBLIC  # Changed from PRIVATE to PUBLIC
        nimcp
        Python3::Python
)

# Export Python module symbols
set_target_properties(nimcp_python PROPERTIES
    ENABLE_EXPORTS ON
)

# Installation rules
install(TARGETS nimcp_python
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/python3/dist-packages
)
