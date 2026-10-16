# ============================================================================
# NIMCP Java Bindings Test Suite
# ============================================================================

find_package(Java COMPONENTS Development Runtime)
include(UseJava)

set(JAVA_SRC_DIR ${CMAKE_SOURCE_DIR}/src/bindings/java)
set(JAVA_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR}/classes)

# Step 1: Compile NIMCP.java (the bindings)
add_custom_command(
    OUTPUT ${JAVA_BIN_DIR}/com/nimcp/NIMCP.class
    COMMAND ${CMAKE_COMMAND} -E make_directory ${JAVA_BIN_DIR}
    COMMAND ${Java_JAVAC_EXECUTABLE} -d ${JAVA_BIN_DIR}
            ${JAVA_SRC_DIR}/NIMCP.java
    DEPENDS ${JAVA_SRC_DIR}/NIMCP.java
    COMMENT "Compiling NIMCP.java"
)

# Step 2: Compile TestNIMCPBindings.java
add_custom_command(
    OUTPUT ${JAVA_BIN_DIR}/TestNIMCPBindings.class
    COMMAND ${Java_JAVAC_EXECUTABLE}
            -cp ${JAVA_BIN_DIR}
            -d ${JAVA_BIN_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/TestNIMCPBindings.java
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/TestNIMCPBindings.java
            ${JAVA_BIN_DIR}/com/nimcp/NIMCP.class
    COMMENT "Compiling TestNIMCPBindings.java"
)

# Custom target to trigger compilation
add_custom_target(java_bindings_test_compile ALL
    DEPENDS ${JAVA_BIN_DIR}/TestNIMCPBindings.class
            nimcp_jni
)

# CTest entry
add_test(
    NAME java_bindings_test
    COMMAND ${Java_JAVA_EXECUTABLE}
            -Djava.library.path=$<TARGET_FILE_DIR:nimcp_jni>:$<TARGET_FILE_DIR:nimcp>
            -cp ${JAVA_BIN_DIR}
            TestNIMCPBindings
)

set_tests_properties(java_bindings_test PROPERTIES
    TIMEOUT 120
    LABELS "bindings;java"
    ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:nimcp_jni>:$<TARGET_FILE_DIR:nimcp>"
)
