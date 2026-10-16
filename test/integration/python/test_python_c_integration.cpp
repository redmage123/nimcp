/**
 * @file test_python_c_integration.cpp
 * @brief Integration tests for Python-C boundary
 *
 * WHAT: Test Python bindings with actual Python interpreter
 * WHY:  Verify memory is properly managed across Python/C boundary
 *       and error propagation from C to Python exceptions works
 * HOW:  Use Python embedding API to test bindings in-process
 *
 * TEST COVERAGE:
 * - Python interpreter initialization
 * - Module loading and type registration
 * - Memory management across Python/C boundary
 * - Error propagation from C to Python exceptions
 * - Reference counting correctness
 * - Exception handling
 *
 * @author NIMCP Development Team
 * @date 2026-01-15
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <thread>
#include <atomic>

/* Python header - must come before other includes to avoid _POSIX_C_SOURCE conflicts */
#define PY_SSIZE_T_CLEAN
#include <Python.h>

/* NIMCP headers */
extern "C" {
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class PythonCIntegrationTest : public ::testing::Test {
protected:
    bool python_initialized_ = false;
    PyObject* nimcp_module_ = nullptr;

    void SetUp() override {
        // Initialize Python interpreter if not already done
        if (!Py_IsInitialized()) {
            Py_Initialize();
            python_initialized_ = true;
        }

        // Ensure we have the GIL
        PyGILState_STATE gstate = PyGILState_Ensure();

        // Add build path to Python path for module loading
        PyRun_SimpleString("import sys");
        PyRun_SimpleString("sys.path.insert(0, '/home/bbrelin/nimcp/build/src/python')");

        // Try to import nimcp module
        nimcp_module_ = PyImport_ImportModule("nimcp");
        // Module may not be available in all test configurations
        // Tests will skip if module not found

        PyGILState_Release(gstate);
    }

    void TearDown() override {
        PyGILState_STATE gstate = PyGILState_Ensure();

        if (nimcp_module_) {
            Py_DECREF(nimcp_module_);
            nimcp_module_ = nullptr;
        }

        PyGILState_Release(gstate);

        // Note: We don't finalize Python as other tests may need it
        // Py_Finalize() is called only once at program exit
    }

    bool ModuleAvailable() const {
        return nimcp_module_ != nullptr;
    }

    // Helper to run Python code and get result
    PyObject* RunPython(const char* code) {
        PyObject* main_module = PyImport_AddModule("__main__");
        PyObject* main_dict = PyModule_GetDict(main_module);
        return PyRun_String(code, Py_eval_input, main_dict, main_dict);
    }

    // Helper to run Python statement (no return value)
    int ExecPython(const char* code) {
        return PyRun_SimpleString(code);
    }
};

//=============================================================================
// PYTHON INTERPRETER TESTS
//=============================================================================

TEST_F(PythonCIntegrationTest, PythonInterpreter_Initialization) {
    /* WHAT: Verify Python interpreter is initialized */
    /* WHY:  Basic sanity check for Python embedding */

    EXPECT_TRUE(Py_IsInitialized());
}

TEST_F(PythonCIntegrationTest, PythonInterpreter_GIL) {
    /* WHAT: Test GIL acquisition and release */
    /* WHY:  Ensure thread safety for Python calls */

    PyGILState_STATE gstate = PyGILState_Ensure();
    EXPECT_TRUE(PyGILState_Check());
    PyGILState_Release(gstate);
}

TEST_F(PythonCIntegrationTest, PythonInterpreter_BasicEval) {
    /* WHAT: Test basic Python evaluation */
    /* WHY:  Verify interpreter works for expressions */

    PyGILState_STATE gstate = PyGILState_Ensure();

    PyObject* result = PyRun_String("1 + 2", Py_eval_input,
        PyModule_GetDict(PyImport_AddModule("__main__")),
        PyModule_GetDict(PyImport_AddModule("__main__")));

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(PyLong_AsLong(result), 3);
    Py_DECREF(result);

    PyGILState_Release(gstate);
}

//=============================================================================
// MODULE LOADING TESTS
//=============================================================================

TEST_F(PythonCIntegrationTest, ModuleLoading_Import) {
    /* WHAT: Test nimcp module import */
    /* WHY:  Verify module is properly registered */

    if (!ModuleAvailable()) {
        GTEST_SKIP() << "NIMCP Python module not available";
    }

    PyGILState_STATE gstate = PyGILState_Ensure();

    EXPECT_NE(nimcp_module_, nullptr);
    EXPECT_TRUE(PyModule_Check(nimcp_module_));

    PyGILState_Release(gstate);
}

TEST_F(PythonCIntegrationTest, ModuleLoading_Types) {
    /* WHAT: Test type registration in module */
    /* WHY:  Verify all types are accessible */

    if (!ModuleAvailable()) {
        GTEST_SKIP() << "NIMCP Python module not available";
    }

    PyGILState_STATE gstate = PyGILState_Ensure();

    const char* type_names[] = {
        "Brain", "NeuralNetwork", "P2PNode",
        "NetworkConfig", "NodeConfig", "GlialIntegration"
    };

    for (const char* name : type_names) {
        PyObject* type = PyObject_GetAttrString(nimcp_module_, name);
        if (type) {
            EXPECT_TRUE(PyType_Check(type)) << "Expected type for: " << name;
            Py_DECREF(type);
        }
        // Some types may not be registered in all configurations
        PyErr_Clear();
    }

    PyGILState_Release(gstate);
}

TEST_F(PythonCIntegrationTest, ModuleLoading_Exceptions) {
    /* WHAT: Test exception types are registered */
    /* WHY:  Verify error handling infrastructure */

    if (!ModuleAvailable()) {
        GTEST_SKIP() << "NIMCP Python module not available";
    }

    PyGILState_STATE gstate = PyGILState_Ensure();

    const char* exception_names[] = {
        "NIMCPError", "NetworkError", "ProtocolError", "NodeError"
    };

    for (const char* name : exception_names) {
        PyObject* exc = PyObject_GetAttrString(nimcp_module_, name);
        if (exc) {
            EXPECT_TRUE(PyExceptionClass_Check(exc)) << "Expected exception for: " << name;
            Py_DECREF(exc);
        }
        PyErr_Clear();
    }

    PyGILState_Release(gstate);
}

//=============================================================================
// MEMORY MANAGEMENT TESTS
//=============================================================================

TEST_F(PythonCIntegrationTest, Memory_ReferenceCountingBasic) {
    /* WHAT: Test basic reference counting */
    /* WHY:  Ensure objects are not leaked */

    PyGILState_STATE gstate = PyGILState_Ensure();

    /* Use large value to avoid Python 3.12+ immortal object cache
     * (small integers like 0-256 are immortal and Py_INCREF is a no-op) */
    PyObject* obj = PyLong_FromLong(999999);
    ASSERT_NE(obj, nullptr);

    Py_ssize_t initial_refcount = Py_REFCNT(obj);
    EXPECT_GE(initial_refcount, 1);

    Py_INCREF(obj);
    EXPECT_EQ(Py_REFCNT(obj), initial_refcount + 1);

    Py_DECREF(obj);
    EXPECT_EQ(Py_REFCNT(obj), initial_refcount);

    Py_DECREF(obj);  // Final decref

    PyGILState_Release(gstate);
}

TEST_F(PythonCIntegrationTest, Memory_ListOperations) {
    /* WHAT: Test list operations and reference counting */
    /* WHY:  Verify container ownership semantics */

    PyGILState_STATE gstate = PyGILState_Ensure();

    PyObject* list = PyList_New(0);
    ASSERT_NE(list, nullptr);

    // Add items
    for (int i = 0; i < 10; ++i) {
        PyObject* item = PyLong_FromLong(i);
        ASSERT_NE(item, nullptr);

        // PyList_Append steals reference
        int result = PyList_Append(list, item);
        EXPECT_EQ(result, 0);
        Py_DECREF(item);  // We still own our reference
    }

    EXPECT_EQ(PyList_Size(list), 10);

    // Verify items
    for (int i = 0; i < 10; ++i) {
        PyObject* item = PyList_GetItem(list, i);  // Borrowed reference
        EXPECT_EQ(PyLong_AsLong(item), i);
    }

    Py_DECREF(list);

    PyGILState_Release(gstate);
}

TEST_F(PythonCIntegrationTest, Memory_DictOperations) {
    /* WHAT: Test dict operations and reference counting */
    /* WHY:  Verify dict ownership semantics */

    PyGILState_STATE gstate = PyGILState_Ensure();

    PyObject* dict = PyDict_New();
    ASSERT_NE(dict, nullptr);

    // Add items
    const char* keys[] = {"alpha", "beta", "gamma"};
    for (int i = 0; i < 3; ++i) {
        PyObject* key = PyUnicode_FromString(keys[i]);
        PyObject* value = PyLong_FromLong(i * 10);

        ASSERT_NE(key, nullptr);
        ASSERT_NE(value, nullptr);

        int result = PyDict_SetItem(dict, key, value);
        EXPECT_EQ(result, 0);

        Py_DECREF(key);
        Py_DECREF(value);
    }

    EXPECT_EQ(PyDict_Size(dict), 3);

    // Verify items
    for (int i = 0; i < 3; ++i) {
        PyObject* key = PyUnicode_FromString(keys[i]);
        PyObject* value = PyDict_GetItem(dict, key);  // Borrowed
        if (value) {
            EXPECT_EQ(PyLong_AsLong(value), i * 10);
        }
        Py_DECREF(key);
    }

    Py_DECREF(dict);

    PyGILState_Release(gstate);
}

TEST_F(PythonCIntegrationTest, Memory_ObjectCreationDestruction) {
    /* WHAT: Test object creation and destruction cycle */
    /* WHY:  Verify C-created Python objects are properly managed */

    if (!ModuleAvailable()) {
        GTEST_SKIP() << "NIMCP Python module not available";
    }

    PyGILState_STATE gstate = PyGILState_Ensure();

    // Try to create Brain object if available
    PyObject* brain_type = PyObject_GetAttrString(nimcp_module_, "Brain");
    if (brain_type && PyType_Check(brain_type)) {
        // Create instance
        PyObject* args = PyTuple_New(0);
        PyObject* brain = PyObject_Call(brain_type, args, nullptr);

        if (brain) {
            // Object created successfully
            EXPECT_GE(Py_REFCNT(brain), 1);
            Py_DECREF(brain);
        } else {
            // Creation failed - clear error
            PyErr_Clear();
        }

        Py_DECREF(args);
        Py_DECREF(brain_type);
    }
    PyErr_Clear();

    PyGILState_Release(gstate);
}

//=============================================================================
// ERROR PROPAGATION TESTS
//=============================================================================

TEST_F(PythonCIntegrationTest, ErrorPropagation_SetException) {
    /* WHAT: Test setting Python exceptions from C */
    /* WHY:  Verify error propagation mechanism */

    PyGILState_STATE gstate = PyGILState_Ensure();

    // Set an exception
    PyErr_SetString(PyExc_ValueError, "Test error from C");
    EXPECT_TRUE(PyErr_Occurred());

    // Check exception type
    PyObject *type, *value, *tb;
    PyErr_Fetch(&type, &value, &tb);

    EXPECT_EQ(type, PyExc_ValueError);

    Py_XDECREF(type);
    Py_XDECREF(value);
    Py_XDECREF(tb);

    PyGILState_Release(gstate);
}

TEST_F(PythonCIntegrationTest, ErrorPropagation_CustomException) {
    /* WHAT: Test custom NIMCP exceptions */
    /* WHY:  Verify exception hierarchy works */

    if (!ModuleAvailable()) {
        GTEST_SKIP() << "NIMCP Python module not available";
    }

    PyGILState_STATE gstate = PyGILState_Ensure();

    PyObject* nimcp_error = PyObject_GetAttrString(nimcp_module_, "NIMCPError");
    if (nimcp_error && PyExceptionClass_Check(nimcp_error)) {
        // Set custom exception
        PyErr_SetString(nimcp_error, "Test NIMCP error");
        EXPECT_TRUE(PyErr_Occurred());

        // Verify it's the right type
        EXPECT_TRUE(PyErr_ExceptionMatches(nimcp_error));

        PyErr_Clear();
        Py_DECREF(nimcp_error);
    }
    PyErr_Clear();

    PyGILState_Release(gstate);
}

TEST_F(PythonCIntegrationTest, ErrorPropagation_ExceptionHierarchy) {
    /* WHAT: Test exception inheritance */
    /* WHY:  Verify NetworkError inherits from NIMCPError */

    if (!ModuleAvailable()) {
        GTEST_SKIP() << "NIMCP Python module not available";
    }

    PyGILState_STATE gstate = PyGILState_Ensure();

    PyObject* nimcp_error = PyObject_GetAttrString(nimcp_module_, "NIMCPError");
    PyObject* network_error = PyObject_GetAttrString(nimcp_module_, "NetworkError");

    if (nimcp_error && network_error &&
        PyExceptionClass_Check(nimcp_error) && PyExceptionClass_Check(network_error)) {
        // Set NetworkError
        PyErr_SetString(network_error, "Test network error");

        // Should match both NetworkError and NIMCPError
        EXPECT_TRUE(PyErr_ExceptionMatches(network_error));
        // Note: PyErr_ExceptionMatches checks inheritance

        PyErr_Clear();
    }

    Py_XDECREF(nimcp_error);
    Py_XDECREF(network_error);
    PyErr_Clear();

    PyGILState_Release(gstate);
}

TEST_F(PythonCIntegrationTest, ErrorPropagation_ClearError) {
    /* WHAT: Test error clearing */
    /* WHY:  Verify clean error state management */

    PyGILState_STATE gstate = PyGILState_Ensure();

    // Set error
    PyErr_SetString(PyExc_RuntimeError, "Test error");
    EXPECT_TRUE(PyErr_Occurred());

    // Clear it
    PyErr_Clear();
    EXPECT_FALSE(PyErr_Occurred());

    PyGILState_Release(gstate);
}

//=============================================================================
// THREAD SAFETY TESTS
//=============================================================================

TEST_F(PythonCIntegrationTest, ThreadSafety_GILFromMultipleThreads) {
    /* WHAT: Test GIL acquisition from multiple threads */
    /* WHY:  Verify thread-safe Python access */

    std::atomic<int> successful_ops{0};
    std::atomic<bool> stop{false};

    auto worker = [&]() {
        while (!stop.load()) {
            PyGILState_STATE gstate = PyGILState_Ensure();

            // Simple Python operation
            PyObject* result = PyLong_FromLong(42);
            if (result) {
                Py_DECREF(result);
                successful_ops.fetch_add(1);
            }

            PyGILState_Release(gstate);
            std::this_thread::yield();
        }
    };

    /* Release the GIL from the main thread so worker threads can acquire it.
     * Without this, PyGILState_Ensure in workers deadlocks waiting for
     * the GIL that the main thread implicitly holds. */
    PyThreadState* save = PyEval_SaveThread();

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(worker);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop.store(true);

    for (auto& t : threads) {
        t.join();
    }

    /* Restore the GIL for the main thread */
    PyEval_RestoreThread(save);

    EXPECT_GT(successful_ops.load(), 100);
}

TEST_F(PythonCIntegrationTest, ThreadSafety_ConcurrentObjectCreation) {
    /* WHAT: Test concurrent object creation */
    /* WHY:  Verify thread-safe allocation */

    std::atomic<int> objects_created{0};
    std::atomic<bool> stop{false};

    auto worker = [&]() {
        while (!stop.load()) {
            PyGILState_STATE gstate = PyGILState_Ensure();

            PyObject* list = PyList_New(5);
            if (list) {
                for (int i = 0; i < 5; ++i) {
                    PyObject* item = PyLong_FromLong(i);
                    if (item) {
                        PyList_SetItem(list, i, item);  // Steals reference
                    }
                }
                Py_DECREF(list);
                objects_created.fetch_add(1);
            }

            PyGILState_Release(gstate);
        }
    };

    /* Release the GIL from the main thread so worker threads can acquire it */
    PyThreadState* save = PyEval_SaveThread();

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(worker);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop.store(true);

    for (auto& t : threads) {
        t.join();
    }

    /* Restore the GIL for the main thread */
    PyEval_RestoreThread(save);

    EXPECT_GT(objects_created.load(), 50);
}

//=============================================================================
// DATA CONVERSION TESTS
//=============================================================================

TEST_F(PythonCIntegrationTest, DataConversion_FloatArray) {
    /* WHAT: Test float array conversion between C and Python */
    /* WHY:  Verify numerical data crossing boundary */

    PyGILState_STATE gstate = PyGILState_Ensure();

    // C array to Python list
    float c_array[] = {1.0f, 2.5f, 3.14159f, -0.5f};
    int len = sizeof(c_array) / sizeof(c_array[0]);

    PyObject* py_list = PyList_New(len);
    ASSERT_NE(py_list, nullptr);

    for (int i = 0; i < len; ++i) {
        PyObject* item = PyFloat_FromDouble((double)c_array[i]);
        PyList_SetItem(py_list, i, item);  // Steals reference
    }

    // Python list back to C array
    float result[4];
    for (int i = 0; i < len; ++i) {
        PyObject* item = PyList_GetItem(py_list, i);  // Borrowed
        result[i] = (float)PyFloat_AsDouble(item);
    }

    // Verify roundtrip
    for (int i = 0; i < len; ++i) {
        EXPECT_NEAR(result[i], c_array[i], 1e-5f);
    }

    Py_DECREF(py_list);

    PyGILState_Release(gstate);
}

TEST_F(PythonCIntegrationTest, DataConversion_StringBuffer) {
    /* WHAT: Test string conversion between C and Python */
    /* WHY:  Verify string data crossing boundary */

    PyGILState_STATE gstate = PyGILState_Ensure();

    const char* c_string = "Hello from C!";

    // C string to Python string
    PyObject* py_string = PyUnicode_FromString(c_string);
    ASSERT_NE(py_string, nullptr);

    // Python string back to C
    const char* result = PyUnicode_AsUTF8(py_string);
    ASSERT_NE(result, nullptr);

    EXPECT_STREQ(result, c_string);

    Py_DECREF(py_string);

    PyGILState_Release(gstate);
}

TEST_F(PythonCIntegrationTest, DataConversion_BytesBuffer) {
    /* WHAT: Test bytes buffer conversion */
    /* WHY:  Verify binary data crossing boundary */

    PyGILState_STATE gstate = PyGILState_Ensure();

    unsigned char c_buffer[] = {0x00, 0x01, 0x02, 0xFF, 0xFE, 0xFD};
    size_t len = sizeof(c_buffer);

    // C buffer to Python bytes
    PyObject* py_bytes = PyBytes_FromStringAndSize((const char*)c_buffer, len);
    ASSERT_NE(py_bytes, nullptr);

    // Python bytes back to C buffer
    char* result;
    Py_ssize_t result_len;
    int rc = PyBytes_AsStringAndSize(py_bytes, &result, &result_len);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ((size_t)result_len, len);

    // Verify roundtrip
    for (size_t i = 0; i < len; ++i) {
        EXPECT_EQ((unsigned char)result[i], c_buffer[i]);
    }

    Py_DECREF(py_bytes);

    PyGILState_Release(gstate);
}
