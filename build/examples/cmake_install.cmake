# Install script for directory: /home/bbrelin/nimcp/examples

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}/examples/event_demo" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/examples/event_demo")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}/examples/event_demo"
         RPATH "")
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/examples/event_demo")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/examples" TYPE EXECUTABLE FILES "/home/bbrelin/nimcp/build/examples/event_demo")
  if(EXISTS "$ENV{DESTDIR}/examples/event_demo" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/examples/event_demo")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}/examples/event_demo"
         OLD_RPATH "/home/bbrelin/nimcp/build/lib:"
         NEW_RPATH "")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}/examples/event_demo")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}/examples/brain_demo" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/examples/brain_demo")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}/examples/brain_demo"
         RPATH "")
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/examples/brain_demo")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/examples" TYPE EXECUTABLE FILES "/home/bbrelin/nimcp/build/examples/brain_demo")
  if(EXISTS "$ENV{DESTDIR}/examples/brain_demo" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/examples/brain_demo")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}/examples/brain_demo"
         OLD_RPATH "/home/bbrelin/nimcp/build/lib:"
         NEW_RPATH "")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}/examples/brain_demo")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}/examples/ethics_demo" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/examples/ethics_demo")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}/examples/ethics_demo"
         RPATH "")
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/examples/ethics_demo")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/examples" TYPE EXECUTABLE FILES "/home/bbrelin/nimcp/build/examples/ethics_demo")
  if(EXISTS "$ENV{DESTDIR}/examples/ethics_demo" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/examples/ethics_demo")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}/examples/ethics_demo"
         OLD_RPATH "/home/bbrelin/nimcp/build/lib:"
         NEW_RPATH "")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}/examples/ethics_demo")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}/examples/infant_demo" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/examples/infant_demo")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}/examples/infant_demo"
         RPATH "")
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/examples/infant_demo")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/examples" TYPE EXECUTABLE FILES "/home/bbrelin/nimcp/build/examples/infant_demo")
  if(EXISTS "$ENV{DESTDIR}/examples/infant_demo" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/examples/infant_demo")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}/examples/infant_demo"
         OLD_RPATH "/home/bbrelin/nimcp/build/lib:"
         NEW_RPATH "")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}/examples/infant_demo")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}/examples/integrated_demo" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/examples/integrated_demo")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}/examples/integrated_demo"
         RPATH "")
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/examples/integrated_demo")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/examples" TYPE EXECUTABLE FILES "/home/bbrelin/nimcp/build/examples/integrated_demo")
  if(EXISTS "$ENV{DESTDIR}/examples/integrated_demo" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/examples/integrated_demo")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}/examples/integrated_demo"
         OLD_RPATH "/home/bbrelin/nimcp/build/lib:"
         NEW_RPATH "")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}/examples/integrated_demo")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}/examples/brain_probe_demo" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/examples/brain_probe_demo")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}/examples/brain_probe_demo"
         RPATH "")
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/examples/brain_probe_demo")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/examples" TYPE EXECUTABLE FILES "/home/bbrelin/nimcp/build/examples/brain_probe_demo")
  if(EXISTS "$ENV{DESTDIR}/examples/brain_probe_demo" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/examples/brain_probe_demo")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}/examples/brain_probe_demo"
         OLD_RPATH "/home/bbrelin/nimcp/build/lib:"
         NEW_RPATH "")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}/examples/brain_probe_demo")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}/examples/izhikevich_demo" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/examples/izhikevich_demo")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}/examples/izhikevich_demo"
         RPATH "")
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/examples/izhikevich_demo")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/examples" TYPE EXECUTABLE FILES "/home/bbrelin/nimcp/build/examples/izhikevich_demo")
  if(EXISTS "$ENV{DESTDIR}/examples/izhikevich_demo" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/examples/izhikevich_demo")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}/examples/izhikevich_demo"
         OLD_RPATH "/home/bbrelin/nimcp/build/lib:"
         NEW_RPATH "")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}/examples/izhikevich_demo")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}/examples/nlp_integration_test" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/examples/nlp_integration_test")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}/examples/nlp_integration_test"
         RPATH "")
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/examples/nlp_integration_test")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/examples" TYPE EXECUTABLE FILES "/home/bbrelin/nimcp/build/examples/nlp_integration_test")
  if(EXISTS "$ENV{DESTDIR}/examples/nlp_integration_test" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/examples/nlp_integration_test")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}/examples/nlp_integration_test"
         OLD_RPATH "/home/bbrelin/nimcp/build/lib:"
         NEW_RPATH "")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}/examples/nlp_integration_test")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}/examples/fractal_network_demo" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/examples/fractal_network_demo")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}/examples/fractal_network_demo"
         RPATH "")
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/examples/fractal_network_demo")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/examples" TYPE EXECUTABLE FILES "/home/bbrelin/nimcp/build/examples/fractal_network_demo")
  if(EXISTS "$ENV{DESTDIR}/examples/fractal_network_demo" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/examples/fractal_network_demo")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}/examples/fractal_network_demo"
         OLD_RPATH "/home/bbrelin/nimcp/build/lib:"
         NEW_RPATH "")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}/examples/fractal_network_demo")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}/examples/nlp_integration_demo" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/examples/nlp_integration_demo")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}/examples/nlp_integration_demo"
         RPATH "")
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/examples/nlp_integration_demo")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/examples" TYPE EXECUTABLE FILES "/home/bbrelin/nimcp/build/examples/nlp_integration_demo")
  if(EXISTS "$ENV{DESTDIR}/examples/nlp_integration_demo" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/examples/nlp_integration_demo")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}/examples/nlp_integration_demo"
         OLD_RPATH "/home/bbrelin/nimcp/build/lib:"
         NEW_RPATH "")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}/examples/nlp_integration_demo")
    endif()
  endif()
endif()

