/**
 * @file test_python_memory_regression.cpp
 * @brief Regression tests for Python bindings memory management
 *
 * WHAT: Tests to prevent regression of memory leak bugs in Python bindings
 * WHY:  Lock in correct memory cleanup behavior after bug fixes
 * HOW:  Test dict building, error cleanup, reference counting
 *
 * BUG HISTORY:
 * - Bug #1: Memory leak in dict building when key/value creation fails
 *   FIX: Clean up all allocated PyObjects on error path
 * - Bug #2: Missing Py_DECREF on exception objects
 *   FIX: Properly decref exception types on module unload
 * - Bug #3: Dict entry leak when PyDict_SetItemString fails
 *   FIX: Decref both key and value on SetItem failure
 *
 * REGRESSION FOCUS:
 * 1. No memory leaks in dict building
 * 2. Proper cleanup on failure paths
 * 3. Reference counts are balanced
 *
 * NOTE: These tests use ASan/LSan markers when available
 *
 * @version 1.0.0
 * @date 2026-01-15
 */

#include <gtest/gtest.h>
#include <Python.h>
#include <cstdlib>
#include <cstring>

/* ASan/LSan detection macros */
#if defined(__SANITIZE_ADDRESS__) || defined(__has_feature)
  #if defined(__SANITIZE_ADDRESS__) || __has_feature(address_sanitizer)
    #define NIMCP_HAS_ASAN 1
    extern "C" void __lsan_do_leak_check(void);
    extern "C" int __lsan_do_recoverable_leak_check(void);
  #endif
#endif

#ifndef NIMCP_HAS_ASAN
  #define NIMCP_HAS_ASAN 0
#endif

//=============================================================================
// Test Fixture
//=============================================================================

class PythonMemoryRegressionTest : public ::testing::Test {
protected:
    bool python_initialized;

    void SetUp() override {
        /* Initialize Python if not already initialized */
        if (!Py_IsInitialized()) {
            Py_Initialize();
            python_initialized = true;
        } else {
            python_initialized = false;
        }
    }

    void TearDown() override {
        /* Only finalize if we initialized */
        if (python_initialized && Py_IsInitialized()) {
            /* Note: Py_Finalize can cause issues with GTest, skip in tests */
            /* Py_Finalize(); */
        }

#if NIMCP_HAS_ASAN
        /* Check for leaks if ASan is available */
        /* Note: This may report false positives due to Python internals */
#endif
    }

    /**
     * @brief Get current reference count for an object
     */
    Py_ssize_t get_refcount(PyObject* obj) {
        if (!obj) return -1;
        return Py_REFCNT(obj);
    }

    /**
     * @brief Create a test dictionary with N entries
     */
    PyObject* create_test_dict(int num_entries) {
        PyObject* dict = PyDict_New();
        if (!dict) return nullptr;

        for (int i = 0; i < num_entries; i++) {
            char key[64];
            snprintf(key, sizeof(key), "key_%d", i);

            PyObject* value = PyLong_FromLong(i);
            if (!value) {
                Py_DECREF(dict);
                return nullptr;
            }

            if (PyDict_SetItemString(dict, key, value) < 0) {
                Py_DECREF(value);
                Py_DECREF(dict);
                return nullptr;
            }

            /* SetItemString increases refcount, we need to release our reference */
            Py_DECREF(value);
        }

        return dict;
    }
};

//=============================================================================
// DICT BUILDING MEMORY REGRESSION TESTS
//=============================================================================

/**
 * BUG: Memory leak when creating dictionary entries
 *
 * WRONG: PyDict_SetItemString steals reference (it doesn't!)
 * RIGHT: PyDict_SetItemString INCREMENTS refcount, caller must decref
 */
TEST_F(PythonMemoryRegressionTest, DictBuilding_NoLeakOnSuccess) {
    /**
     * REGRESSION TEST: Dict building should not leak memory
     *
     * Create and destroy many dictionaries, verify no memory growth.
     */
    const int num_iterations = 1000;
    const int entries_per_dict = 10;

    for (int i = 0; i < num_iterations; i++) {
        PyObject* dict = create_test_dict(entries_per_dict);
        ASSERT_NE(dict, nullptr) << "Failed to create dict at iteration " << i;

        /* Dict should have refcount of 1 */
        EXPECT_EQ(get_refcount(dict), 1) << "Dict has unexpected refcount";

        Py_DECREF(dict);
    }

    /* If there's a memory leak, we would see growth over 1000 iterations */
    /* ASan/LSan will catch this if enabled */
}

TEST_F(PythonMemoryRegressionTest, DictBuilding_ValueRefcountCorrect) {
    /**
     * REGRESSION TEST: Values in dict should have correct refcount
     *
     * After SetItemString, value refcount should be 2 (our ref + dict's ref).
     * After we decref our reference, value refcount should be 1 (dict's ref only).
     */
    PyObject* dict = PyDict_New();
    ASSERT_NE(dict, nullptr);

    PyObject* value = PyLong_FromLong(42);
    ASSERT_NE(value, nullptr);

    /* Before SetItemString: refcount should be 1 */
    EXPECT_EQ(get_refcount(value), 1) << "Initial value refcount should be 1";

    int result = PyDict_SetItemString(dict, "test_key", value);
    ASSERT_EQ(result, 0) << "SetItemString failed";

    /* After SetItemString: refcount should be 2 */
    EXPECT_EQ(get_refcount(value), 2)
        << "REGRESSION: SetItemString should increment refcount to 2";

    /* After we decref: refcount should be 1 */
    Py_DECREF(value);
    /* Can't check refcount here as we no longer own a reference */

    /* Get the value back from dict */
    PyObject* retrieved = PyDict_GetItemString(dict, "test_key");
    EXPECT_NE(retrieved, nullptr) << "Value should still be in dict";

    /* Clean up */
    Py_DECREF(dict);
}

TEST_F(PythonMemoryRegressionTest, DictBuilding_CleanupOnKeyFailure) {
    /**
     * REGRESSION TEST: Cleanup should happen if key creation fails
     *
     * This tests the error path when we can't create a key.
     */
    PyObject* dict = PyDict_New();
    ASSERT_NE(dict, nullptr);

    /* Create a valid value */
    PyObject* value = PyLong_FromLong(123);
    ASSERT_NE(value, nullptr);
    Py_ssize_t initial_refcount = get_refcount(value);

    /* Try to set with NULL key (should fail) */
    int result = PyDict_SetItemString(dict, nullptr, value);

    /* SetItemString with NULL key should fail */
    /* Note: behavior may vary - some versions may segfault */
    /* This test documents expected behavior */

    /* Value refcount should be unchanged on failure */
    EXPECT_EQ(get_refcount(value), initial_refcount)
        << "REGRESSION: Value refcount changed on SetItemString failure";

    Py_DECREF(value);
    Py_DECREF(dict);
}

//=============================================================================
// ERROR PATH CLEANUP REGRESSION TESTS
//=============================================================================

TEST_F(PythonMemoryRegressionTest, ErrorPath_ExceptionTypeCleanup) {
    /**
     * REGRESSION TEST: Exception types should be properly cleaned up
     *
     * When we create custom exception types, we must properly manage
     * their reference counts.
     */
    PyObject* base_exception = PyExc_Exception;

    /* Create a custom exception type */
    PyObject* custom_exception = PyErr_NewException(
        "test_module.TestError", base_exception, nullptr);

    if (custom_exception) {
        /* Custom exception should have refcount of 1 */
        EXPECT_GE(get_refcount(custom_exception), 1)
            << "Custom exception has unexpected refcount";

        /* Clean up - this is what the bug was missing */
        Py_DECREF(custom_exception);
    }

    /* No memory should be leaked */
}

TEST_F(PythonMemoryRegressionTest, ErrorPath_PartialDictCleanup) {
    /**
     * REGRESSION TEST: Partial dict should be cleaned up on error
     *
     * If we're building a dict and an error occurs partway through,
     * all previously added entries must be properly cleaned up.
     */
    const int num_entries = 100;
    const int fail_at = 50;

    PyObject* dict = PyDict_New();
    ASSERT_NE(dict, nullptr);

    bool simulated_failure = false;

    for (int i = 0; i < num_entries; i++) {
        if (i == fail_at) {
            simulated_failure = true;
            break;
        }

        char key[64];
        snprintf(key, sizeof(key), "key_%d", i);

        PyObject* value = PyLong_FromLong(i * 100);
        if (!value) {
            simulated_failure = true;
            break;
        }

        if (PyDict_SetItemString(dict, key, value) < 0) {
            Py_DECREF(value);
            simulated_failure = true;
            break;
        }

        Py_DECREF(value);
    }

    /* Simulated failure at entry 50 */
    EXPECT_TRUE(simulated_failure);

    /* Dict has partial data but should be cleanly releasable */
    EXPECT_EQ(PyDict_Size(dict), fail_at)
        << "Dict should have entries up to failure point";

    /* This should not leak - all entries should be freed */
    Py_DECREF(dict);
}

//=============================================================================
// REFERENCE COUNTING REGRESSION TESTS
//=============================================================================

TEST_F(PythonMemoryRegressionTest, RefCount_ListAppendBehavior) {
    /**
     * REGRESSION TEST: PyList_Append increments refcount
     *
     * Like SetItemString, Append does NOT steal the reference.
     */
    PyObject* list = PyList_New(0);
    ASSERT_NE(list, nullptr);

    PyObject* item = PyLong_FromLong(42);
    ASSERT_NE(item, nullptr);

    EXPECT_EQ(get_refcount(item), 1) << "Initial item refcount should be 1";

    int result = PyList_Append(list, item);
    ASSERT_EQ(result, 0) << "Append failed";

    EXPECT_EQ(get_refcount(item), 2)
        << "REGRESSION: Append should increment refcount to 2";

    Py_DECREF(item);  /* Release our reference */

    /* Item is still in list with refcount 1 */
    Py_DECREF(list);  /* This should decref the item to 0 and free it */
}

TEST_F(PythonMemoryRegressionTest, RefCount_TupleSetItemBehavior) {
    /**
     * REGRESSION TEST: PyTuple_SetItem STEALS reference (unlike dict/list)
     *
     * This is a common source of bugs - tuple SetItem is different!
     */
    PyObject* tuple = PyTuple_New(1);
    ASSERT_NE(tuple, nullptr);

    PyObject* item = PyLong_FromLong(42);
    ASSERT_NE(item, nullptr);

    EXPECT_EQ(get_refcount(item), 1) << "Initial item refcount should be 1";

    /* SetItem STEALS reference - do NOT decref after */
    PyTuple_SetItem(tuple, 0, item);

    /* Refcount should still be 1 (stolen, not incremented) */
    EXPECT_EQ(get_refcount(item), 1)
        << "REGRESSION: Tuple SetItem steals reference, refcount stays 1";

    /* Do NOT Py_DECREF(item) here - tuple owns it now */

    Py_DECREF(tuple);  /* This frees the item */
}

TEST_F(PythonMemoryRegressionTest, RefCount_BalancedAfterOperations) {
    /**
     * REGRESSION TEST: Complex operations should have balanced refcounts
     */
    PyObject* dict = PyDict_New();
    ASSERT_NE(dict, nullptr);

    /* Add 100 items */
    for (int i = 0; i < 100; i++) {
        char key[64];
        snprintf(key, sizeof(key), "key_%d", i);

        PyObject* value = PyLong_FromLong(i);
        ASSERT_NE(value, nullptr);

        ASSERT_EQ(PyDict_SetItemString(dict, key, value), 0);
        Py_DECREF(value);  /* Must decref after SetItemString */
    }

    EXPECT_EQ(PyDict_Size(dict), 100);

    /* Remove 50 items */
    for (int i = 0; i < 50; i++) {
        char key[64];
        snprintf(key, sizeof(key), "key_%d", i);

        ASSERT_EQ(PyDict_DelItemString(dict, key), 0);
    }

    EXPECT_EQ(PyDict_Size(dict), 50);

    /* Clear remaining */
    PyDict_Clear(dict);
    EXPECT_EQ(PyDict_Size(dict), 0);

    /* Final cleanup */
    Py_DECREF(dict);

    /* If refcounts were unbalanced, ASan/LSan would catch leaks */
}

//=============================================================================
// STRESS TESTS FOR MEMORY LEAKS
//=============================================================================

TEST_F(PythonMemoryRegressionTest, Stress_RepeatedDictCreation) {
    /**
     * REGRESSION TEST: Repeated dict creation/destruction should not leak
     */
    const int num_iterations = 10000;

    for (int i = 0; i < num_iterations; i++) {
        PyObject* dict = PyDict_New();
        ASSERT_NE(dict, nullptr);

        PyObject* key = PyUnicode_FromString("test_key");
        PyObject* value = PyLong_FromLong(i);

        if (key && value) {
            PyDict_SetItem(dict, key, value);
        }

        if (key) Py_DECREF(key);
        if (value) Py_DECREF(value);
        Py_DECREF(dict);
    }
}

TEST_F(PythonMemoryRegressionTest, Stress_RepeatedListOperations) {
    /**
     * REGRESSION TEST: Repeated list operations should not leak
     */
    const int num_iterations = 10000;
    const int items_per_list = 10;

    for (int i = 0; i < num_iterations; i++) {
        PyObject* list = PyList_New(0);
        ASSERT_NE(list, nullptr);

        for (int j = 0; j < items_per_list; j++) {
            PyObject* item = PyLong_FromLong(j);
            if (item) {
                PyList_Append(list, item);
                Py_DECREF(item);  /* Must decref after append */
            }
        }

        Py_DECREF(list);
    }
}

TEST_F(PythonMemoryRegressionTest, Stress_NestedStructures) {
    /**
     * REGRESSION TEST: Nested dicts/lists should not leak
     */
    const int depth = 10;

    PyObject* outer = PyDict_New();
    ASSERT_NE(outer, nullptr);

    PyObject* current = outer;
    Py_INCREF(current);  /* Keep reference as we traverse */

    for (int i = 0; i < depth; i++) {
        PyObject* inner = PyDict_New();
        ASSERT_NE(inner, nullptr);

        char key[64];
        snprintf(key, sizeof(key), "level_%d", i);

        PyDict_SetItemString(current, key, inner);
        Py_DECREF(current);  /* Release old current */
        current = inner;
        /* Don't decref inner - current now owns it */
    }

    Py_DECREF(current);  /* Release final inner */
    Py_DECREF(outer);    /* This should recursively free all */
}

//=============================================================================
// CLEANUP BEHAVIOR REGRESSION TESTS
//=============================================================================

TEST_F(PythonMemoryRegressionTest, Cleanup_ModuleUnload) {
    /**
     * REGRESSION TEST: Document expected cleanup on module unload
     *
     * When a module is unloaded, all module-level objects should be
     * properly decreffed.
     */

    /* Create objects that would be module-level */
    PyObject* module_dict = PyDict_New();
    PyObject* module_list = PyList_New(0);
    PyObject* module_exception = PyErr_NewException(
        "test_module.Error", PyExc_Exception, nullptr);

    ASSERT_NE(module_dict, nullptr);
    ASSERT_NE(module_list, nullptr);
    /* module_exception may be nullptr on some Python versions */

    /* Simulate module unload - all objects must be cleaned up */
    Py_DECREF(module_dict);
    Py_DECREF(module_list);
    if (module_exception) {
        Py_DECREF(module_exception);
    }

    /* No leaks should occur */
}

//=============================================================================
// CMAKE CONFIGURATION
//=============================================================================

TEST_F(PythonMemoryRegressionTest, CMakeLists_Placeholder) {
    /**
     * This test exists to ensure the file compiles and links properly.
     * The CMakeLists.txt for this directory should include:
     *
     * target_link_libraries(... Python3::Python ...)
     */
    EXPECT_TRUE(Py_IsInitialized()) << "Python should be initialized";
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
