# ============================================================================
# NIMCP Java Bindings - JNI Bridge Library
# ============================================================================
# WHAT: CMake target for Java/JNI bridge to NIMCP C API
# WHY:  Provides Java language bindings via JNI
# HOW:  Shared library (libnimcp_jni.so) that links against the C library
# ============================================================================

# Find JNI headers manually (headless JDK lacks AWT, so find_package(JNI) fails)
find_package(Java COMPONENTS Development)

# Locate jni.h
find_path(JNI_INCLUDE_DIR jni.h
    HINTS ENV JAVA_HOME
    PATHS /usr/lib/jvm/java-21-openjdk-amd64
          /usr/lib/jvm/default-java
    PATH_SUFFIXES include
)

# Locate jni_md.h (platform-specific)
find_path(JNI_INCLUDE_DIR_MD jni_md.h
    HINTS ${JNI_INCLUDE_DIR}
    PATH_SUFFIXES linux
)

if(NOT JNI_INCLUDE_DIR OR NOT JNI_INCLUDE_DIR_MD)
    message(FATAL_ERROR "Could not find JNI headers (jni.h / jni_md.h)")
endif()

message(STATUS "JNI include: ${JNI_INCLUDE_DIR}")
message(STATUS "JNI platform include: ${JNI_INCLUDE_DIR_MD}")

add_library(nimcp_jni SHARED nimcp_jni.c)

target_include_directories(nimcp_jni PRIVATE
    ${JNI_INCLUDE_DIR}
    ${JNI_INCLUDE_DIR_MD}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(nimcp_jni PRIVATE nimcp)

set_target_properties(nimcp_jni PROPERTIES
    OUTPUT_NAME "nimcp_jni"
    PREFIX "lib"
)
