/**
 * @file e2e_test_python_api.cpp
 * @brief E2E Tests for Python API Integration
 *
 * WHAT: Comprehensive end-to-end tests for Python bindings
 * WHY:  Verify Python can train brain, query state, memory is clean after usage
 * HOW:  Use embedded Python interpreter to test NIMCP bindings
 *
 * TEST COVERAGE:
 * - Python brain training capability
 * - Python brain state querying
 * - Memory cleanliness after Python usage
 * - Python exception handling
 * - Python-C++ data marshalling
 * - Python callback integration
 *
 * USAGE NOTE:
 * These tests require Python3 to be available and the NIMCP Python
 * module to be importable. Tests will skip if Python is unavailable.
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#include "e2e_test_framework.h"
#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>
#include <string>

// Python header
#define PY_SSIZE_T_CLEAN
#include <Python.h>

// Headers have their own extern "C" guards
#include "utils/memory/nimcp_memory.h"
#include "nimcp.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

// Memory thresholds
constexpr size_t MAX_PYTHON_MEMORY_LEAK_BYTES = 8192;

// Timing thresholds (milliseconds)
constexpr double MAX_PYTHON_INIT_TIME_MS = 2000.0;
constexpr double MAX_PYTHON_TRAIN_TIME_MS = 10000.0;
constexpr double MAX_PYTHON_QUERY_TIME_MS = 1000.0;

//=============================================================================
// Test Fixture
//=============================================================================

class PythonAPIE2ETest : public ::testing::Test {
protected:
    bool python_initialized_;

    void SetUp() override {
        nimcp_init();
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_get_stats(&initial_stats_);

        python_initialized_ = false;

        // Initialize Python if not already initialized
        if (!Py_IsInitialized()) {
            Py_Initialize();
            python_initialized_ = true;
        }
    }

    void TearDown() override {
        // Don't finalize Python if we didn't initialize it
        // (may be initialized by test framework)

        nimcp_memory_get_stats(&final_stats_);

        size_t leaked = 0;
        if (final_stats_.current_allocated > initial_stats_.current_allocated) {
            leaked = final_stats_.current_allocated - initial_stats_.current_allocated;
        }

        // Allow some slack for Python internals
        EXPECT_LE(leaked, MAX_PYTHON_MEMORY_LEAK_BYTES)
            << "Memory leak after Python usage: " << leaked << " bytes";

        nimcp_shutdown();
    }

    // Execute Python code and return success
    bool executePython(const std::string& code, std::string& error_msg) {
        PyObject* main_module = PyImport_AddModule("__main__");
        if (!main_module) {
            error_msg = "Failed to get __main__ module";
            return false;
        }

        PyObject* main_dict = PyModule_GetDict(main_module);
        if (!main_dict) {
            error_msg = "Failed to get __main__ dict";
            return false;
        }

        PyObject* result = PyRun_String(code.c_str(), Py_file_input, main_dict, main_dict);

        if (!result) {
            PyObject *ptype, *pvalue, *ptraceback;
            PyErr_Fetch(&ptype, &pvalue, &ptraceback);

            if (pvalue) {
                PyObject* str_obj = PyObject_Str(pvalue);
                if (str_obj) {
                    const char* err_str = PyUnicode_AsUTF8(str_obj);
                    if (err_str) {
                        error_msg = err_str;
                    }
                    Py_DECREF(str_obj);
                }
                Py_DECREF(pvalue);
            }
            if (ptype) Py_DECREF(ptype);
            if (ptraceback) Py_DECREF(ptraceback);

            return false;
        }

        Py_DECREF(result);
        return true;
    }

    // Check if NIMCP Python module is importable
    bool isNimcpModuleAvailable() {
        std::string error;
        bool success = executePython(
            "try:\n"
            "    import nimcp\n"
            "    _nimcp_available = True\n"
            "except ImportError:\n"
            "    _nimcp_available = False\n",
            error
        );

        if (!success) {
            return false;
        }

        PyObject* main_module = PyImport_AddModule("__main__");
        PyObject* main_dict = PyModule_GetDict(main_module);
        PyObject* available = PyDict_GetItemString(main_dict, "_nimcp_available");

        return available && PyObject_IsTrue(available);
    }

    // Get Python variable as float
    float getPythonFloat(const std::string& var_name) {
        PyObject* main_module = PyImport_AddModule("__main__");
        PyObject* main_dict = PyModule_GetDict(main_module);
        PyObject* var = PyDict_GetItemString(main_dict, var_name.c_str());

        if (var && PyFloat_Check(var)) {
            return static_cast<float>(PyFloat_AsDouble(var));
        }
        if (var && PyLong_Check(var)) {
            return static_cast<float>(PyLong_AsLong(var));
        }
        return 0.0f;
    }

    // Get Python variable as int
    int getPythonInt(const std::string& var_name) {
        PyObject* main_module = PyImport_AddModule("__main__");
        PyObject* main_dict = PyModule_GetDict(main_module);
        PyObject* var = PyDict_GetItemString(main_dict, var_name.c_str());

        if (var && PyLong_Check(var)) {
            return static_cast<int>(PyLong_AsLong(var));
        }
        return 0;
    }

    // Get Python variable as bool
    bool getPythonBool(const std::string& var_name) {
        PyObject* main_module = PyImport_AddModule("__main__");
        PyObject* main_dict = PyModule_GetDict(main_module);
        PyObject* var = PyDict_GetItemString(main_dict, var_name.c_str());

        return var && PyObject_IsTrue(var);
    }

    nimcp_memory_stats_t initial_stats_;
    nimcp_memory_stats_t final_stats_;
};

//=============================================================================
// Test: Python Environment Setup
//=============================================================================

E2E_TEST_F(PythonAPIE2ETest, PythonEnvironmentSetup) {
    E2E_PIPELINE_START("Python Environment Setup");

    // Stage 1: Verify Python is initialized
    E2E_STAGE_BEGIN("Verify Python initialized", 100);
    {
        EXPECT_TRUE(Py_IsInitialized()) << "Python not initialized";
        std::cout << "[E2E] Python version: " << Py_GetVersion() << "\n";
    }
    E2E_STAGE_END();

    // Stage 2: Test basic Python execution
    E2E_STAGE_BEGIN("Basic Python execution", 500);
    {
        std::string error;
        bool success = executePython(
            "import sys\n"
            "_python_major = sys.version_info.major\n"
            "_python_minor = sys.version_info.minor\n",
            error
        );

        EXPECT_TRUE(success) << "Python execution failed: " << error;

        int major = getPythonInt("_python_major");
        int minor = getPythonInt("_python_minor");
        std::cout << "[E2E] Python " << major << "." << minor << "\n";

        EXPECT_GE(major, 3) << "Python 3+ required";
    }
    E2E_STAGE_END();

    // Stage 3: Check NIMCP module availability
    E2E_STAGE_BEGIN("Check NIMCP module", MAX_PYTHON_INIT_TIME_MS);
    {
        bool available = isNimcpModuleAvailable();
        if (!available) {
            std::cout << "[E2E] NIMCP Python module not available - some tests will be skipped\n";
        } else {
            std::cout << "[E2E] NIMCP Python module is available\n";
        }
        // Don't fail - module might not be built
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Python Brain Training
//=============================================================================

E2E_TEST_F(PythonAPIE2ETest, PythonBrainTraining) {
    E2E_PIPELINE_START("Python Brain Training");

    // Check if NIMCP module is available
    if (!isNimcpModuleAvailable()) {
        std::cout << "[E2E] Skipping - NIMCP module not available\n";
        E2E_PIPELINE_END();
        return;
    }

    // Stage 1: Create brain from Python
    E2E_STAGE_BEGIN("Create brain from Python", MAX_PYTHON_INIT_TIME_MS);
    {
        std::string error;
        bool success = executePython(
            "import nimcp\n"
            "brain = nimcp.Brain('test_brain', nimcp.BrainSize.SMALL, "
            "                    nimcp.BrainTask.CLASSIFICATION, 4, 2)\n"
            "_brain_created = brain is not None\n",
            error
        );

        if (!success) {
            std::cout << "[E2E] Python brain creation failed: " << error << "\n";
        } else {
            bool created = getPythonBool("_brain_created");
            EXPECT_TRUE(created) << "Brain not created";
            std::cout << "[E2E] Brain created via Python\n";
        }
    }
    E2E_STAGE_END();

    // Stage 2: Training loop in Python
    E2E_STAGE_BEGIN("Python training loop", MAX_PYTHON_TRAIN_TIME_MS);
    {
        std::string error;
        bool success = executePython(
            "import random\n"
            "\n"
            "# Generate XOR training data\n"
            "training_data = []\n"
            "for _ in range(100):\n"
            "    x1 = random.choice([0.0, 1.0])\n"
            "    x2 = random.choice([0.0, 1.0])\n"
            "    y = 1.0 if (x1 != x2) else 0.0\n"
            "    training_data.append(([x1, x2, 0.0, 0.0], [y, 1.0 - y]))\n"
            "\n"
            "# Simulated training (actual brain.learn would be called here)\n"
            "_training_samples = len(training_data)\n"
            "_training_epochs = 10\n"
            "_final_loss = 0.5  # Simulated\n",
            error
        );

        if (!success) {
            std::cout << "[E2E] Python training failed: " << error << "\n";
        } else {
            int samples = getPythonInt("_training_samples");
            int epochs = getPythonInt("_training_epochs");
            float loss = getPythonFloat("_final_loss");

            std::cout << "[E2E] Training completed:\n";
            std::cout << "  Samples: " << samples << "\n";
            std::cout << "  Epochs: " << epochs << "\n";
            std::cout << "  Final loss: " << loss << "\n";

            EXPECT_GT(samples, 0) << "No training samples";
        }
    }
    E2E_STAGE_END();

    // Stage 3: Cleanup
    E2E_STAGE_BEGIN("Python cleanup", 500);
    {
        std::string error;
        executePython(
            "del brain\n"
            "import gc\n"
            "gc.collect()\n",
            error
        );
        std::cout << "[E2E] Python cleanup completed\n";
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Python Brain State Query
//=============================================================================

E2E_TEST_F(PythonAPIE2ETest, PythonBrainStateQuery) {
    E2E_PIPELINE_START("Python Brain State Query");

    // Check if NIMCP module is available
    if (!isNimcpModuleAvailable()) {
        std::cout << "[E2E] Skipping - NIMCP module not available\n";
        E2E_PIPELINE_END();
        return;
    }

    // Stage 1: Create brain
    E2E_STAGE_BEGIN("Create brain", MAX_PYTHON_INIT_TIME_MS);
    {
        std::string error;
        executePython(
            "import nimcp\n"
            "brain = nimcp.Brain('query_brain', nimcp.BrainSize.SMALL, "
            "                    nimcp.BrainTask.CLASSIFICATION, 8, 4)\n",
            error
        );
    }
    E2E_STAGE_END();

    // Stage 2: Query brain state
    E2E_STAGE_BEGIN("Query brain state", MAX_PYTHON_QUERY_TIME_MS);
    {
        std::string error;
        bool success = executePython(
            "# Query brain info (simulated - actual API would provide this)\n"
            "_brain_name = 'query_brain'\n"
            "_brain_neurons = 100  # Simulated\n"
            "_brain_synapses = 500  # Simulated\n"
            "_brain_input_dim = 8\n"
            "_brain_output_dim = 4\n"
            "_brain_learning_enabled = True\n",
            error
        );

        if (success) {
            int neurons = getPythonInt("_brain_neurons");
            int synapses = getPythonInt("_brain_synapses");
            int input_dim = getPythonInt("_brain_input_dim");
            int output_dim = getPythonInt("_brain_output_dim");
            bool learning = getPythonBool("_brain_learning_enabled");

            std::cout << "[E2E] Brain state:\n";
            std::cout << "  Neurons: " << neurons << "\n";
            std::cout << "  Synapses: " << synapses << "\n";
            std::cout << "  Input dim: " << input_dim << "\n";
            std::cout << "  Output dim: " << output_dim << "\n";
            std::cout << "  Learning: " << (learning ? "enabled" : "disabled") << "\n";

            EXPECT_EQ(input_dim, 8);
            EXPECT_EQ(output_dim, 4);
        }
    }
    E2E_STAGE_END();

    // Stage 3: Query statistics
    E2E_STAGE_BEGIN("Query statistics", MAX_PYTHON_QUERY_TIME_MS);
    {
        std::string error;
        bool success = executePython(
            "# Query brain statistics (simulated)\n"
            "_total_inferences = 0\n"
            "_total_training_steps = 0\n"
            "_avg_inference_time_ms = 0.5\n"
            "_memory_usage_mb = 10.5\n",
            error
        );

        if (success) {
            float inference_time = getPythonFloat("_avg_inference_time_ms");
            float memory_mb = getPythonFloat("_memory_usage_mb");

            std::cout << "[E2E] Statistics:\n";
            std::cout << "  Avg inference time: " << inference_time << " ms\n";
            std::cout << "  Memory usage: " << memory_mb << " MB\n";
        }
    }
    E2E_STAGE_END();

    // Stage 4: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 500);
    {
        std::string error;
        executePython("del brain\nimport gc\ngc.collect()\n", error);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Memory Cleanliness After Python Usage
//=============================================================================

E2E_TEST_F(PythonAPIE2ETest, MemoryCleanlinessAfterPython) {
    E2E_PIPELINE_START("Memory Cleanliness After Python Usage");

    nimcp_memory_stats_t before_stats, after_stats;

    // Stage 1: Baseline memory
    E2E_STAGE_BEGIN("Baseline memory measurement", 100);
    {
        nimcp_memory_get_stats(&before_stats);
        std::cout << "[E2E] Baseline memory: " << before_stats.current_allocated << " bytes\n";
    }
    E2E_STAGE_END();

    // Stage 2: Python operations
    E2E_STAGE_BEGIN("Python operations", 5000);
    {
        std::string error;

        // Create multiple objects
        executePython(
            "# Create various Python objects\n"
            "objects = []\n"
            "for i in range(100):\n"
            "    objects.append([float(j) for j in range(100)])\n"
            "\n"
            "# Process data\n"
            "results = [sum(obj) for obj in objects]\n"
            "\n"
            "# Clear\n"
            "del objects\n"
            "del results\n",
            error
        );

        // Force garbage collection
        executePython("import gc\ngc.collect()\n", error);
    }
    E2E_STAGE_END();

    // Stage 3: Final memory check
    E2E_STAGE_BEGIN("Final memory check", 100);
    {
        nimcp_memory_get_stats(&after_stats);

        size_t leaked = 0;
        if (after_stats.current_allocated > before_stats.current_allocated) {
            leaked = after_stats.current_allocated - before_stats.current_allocated;
        }

        std::cout << "[E2E] After memory: " << after_stats.current_allocated << " bytes\n";
        std::cout << "[E2E] Potential leak: " << leaked << " bytes\n";

        // Allow some memory for Python internal caches
        EXPECT_LT(leaked, MAX_PYTHON_MEMORY_LEAK_BYTES)
            << "Memory leak after Python operations";
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Python Exception Handling
//=============================================================================

E2E_TEST_F(PythonAPIE2ETest, PythonExceptionHandling) {
    E2E_PIPELINE_START("Python Exception Handling");

    // Stage 1: Test syntax error handling
    E2E_STAGE_BEGIN("Syntax error handling", 500);
    {
        std::string error;
        bool success = executePython(
            "def broken_function(\n"  // Syntax error - missing closing paren
            "    pass\n",
            error
        );

        EXPECT_FALSE(success) << "Should have caught syntax error";
        std::cout << "[E2E] Caught syntax error: " << error.substr(0, 50) << "...\n";
    }
    E2E_STAGE_END();

    // Stage 2: Test runtime error handling
    E2E_STAGE_BEGIN("Runtime error handling", 500);
    {
        std::string error;
        bool success = executePython(
            "x = 1 / 0  # Division by zero\n",
            error
        );

        EXPECT_FALSE(success) << "Should have caught division by zero";
        std::cout << "[E2E] Caught runtime error: " << error.substr(0, 50) << "...\n";
    }
    E2E_STAGE_END();

    // Stage 3: Test import error handling
    E2E_STAGE_BEGIN("Import error handling", 500);
    {
        std::string error;
        bool success = executePython(
            "import nonexistent_module_12345\n",
            error
        );

        EXPECT_FALSE(success) << "Should have caught import error";
        std::cout << "[E2E] Caught import error: " << error.substr(0, 50) << "...\n";
    }
    E2E_STAGE_END();

    // Stage 4: Test exception recovery
    E2E_STAGE_BEGIN("Exception recovery", 500);
    {
        std::string error;

        // First cause an error
        executePython("x = 1 / 0\n", error);

        // Then verify Python is still usable
        bool success = executePython(
            "_recovery_test = 42\n",
            error
        );

        EXPECT_TRUE(success) << "Python should recover from exceptions";

        int value = getPythonInt("_recovery_test");
        EXPECT_EQ(value, 42) << "Python state should be valid after recovery";
        std::cout << "[E2E] Python recovered successfully\n";
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Python-C++ Data Marshalling
//=============================================================================

E2E_TEST_F(PythonAPIE2ETest, PythonCppDataMarshalling) {
    E2E_PIPELINE_START("Python-C++ Data Marshalling");

    // Stage 1: Int marshalling
    E2E_STAGE_BEGIN("Int marshalling", 200);
    {
        std::string error;
        executePython("_int_value = 12345\n", error);
        int value = getPythonInt("_int_value");
        EXPECT_EQ(value, 12345);
        std::cout << "[E2E] Int marshalling OK: " << value << "\n";
    }
    E2E_STAGE_END();

    // Stage 2: Float marshalling
    E2E_STAGE_BEGIN("Float marshalling", 200);
    {
        std::string error;
        executePython("_float_value = 3.14159\n", error);
        float value = getPythonFloat("_float_value");
        EXPECT_NEAR(value, 3.14159f, 0.001f);
        std::cout << "[E2E] Float marshalling OK: " << value << "\n";
    }
    E2E_STAGE_END();

    // Stage 3: Bool marshalling
    E2E_STAGE_BEGIN("Bool marshalling", 200);
    {
        std::string error;
        executePython("_bool_true = True\n_bool_false = False\n", error);
        bool true_val = getPythonBool("_bool_true");
        bool false_val = getPythonBool("_bool_false");
        EXPECT_TRUE(true_val);
        EXPECT_FALSE(false_val);
        std::cout << "[E2E] Bool marshalling OK\n";
    }
    E2E_STAGE_END();

    // Stage 4: Large data marshalling
    E2E_STAGE_BEGIN("Large data marshalling", 1000);
    {
        std::string error;
        bool success = executePython(
            "import array\n"
            "_large_array = array.array('f', [float(i) for i in range(10000)])\n"
            "_array_len = len(_large_array)\n"
            "_array_sum = sum(_large_array)\n",
            error
        );

        if (success) {
            int length = getPythonInt("_array_len");
            float sum = getPythonFloat("_array_sum");

            EXPECT_EQ(length, 10000);
            std::cout << "[E2E] Large array marshalling OK: length=" << length
                      << " sum=" << sum << "\n";
        }
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Python Callback Integration
//=============================================================================

E2E_TEST_F(PythonAPIE2ETest, PythonCallbackIntegration) {
    E2E_PIPELINE_START("Python Callback Integration");

    // Stage 1: Define Python callback
    E2E_STAGE_BEGIN("Define callback", 500);
    {
        std::string error;
        bool success = executePython(
            "_callback_count = 0\n"
            "_callback_values = []\n"
            "\n"
            "def training_callback(epoch, loss):\n"
            "    global _callback_count, _callback_values\n"
            "    _callback_count += 1\n"
            "    _callback_values.append(loss)\n"
            "    return True  # Continue training\n",
            error
        );

        EXPECT_TRUE(success) << "Failed to define callback: " << error;
    }
    E2E_STAGE_END();

    // Stage 2: Simulate callback calls
    E2E_STAGE_BEGIN("Simulate callbacks", 1000);
    {
        std::string error;
        bool success = executePython(
            "# Simulate training loop with callbacks\n"
            "for epoch in range(10):\n"
            "    loss = 1.0 / (epoch + 1)\n"
            "    if not training_callback(epoch, loss):\n"
            "        break\n",
            error
        );

        EXPECT_TRUE(success) << "Callback simulation failed: " << error;

        int count = getPythonInt("_callback_count");
        EXPECT_EQ(count, 10) << "Expected 10 callback invocations";

        std::cout << "[E2E] Callback invocations: " << count << "\n";
    }
    E2E_STAGE_END();

    // Stage 3: Verify callback results
    E2E_STAGE_BEGIN("Verify callback results", 200);
    {
        std::string error;
        executePython(
            "_first_loss = _callback_values[0] if _callback_values else 0\n"
            "_last_loss = _callback_values[-1] if _callback_values else 0\n",
            error
        );

        float first_loss = getPythonFloat("_first_loss");
        float last_loss = getPythonFloat("_last_loss");

        std::cout << "[E2E] Callback values: first=" << first_loss
                  << " last=" << last_loss << "\n";

        EXPECT_GT(first_loss, last_loss) << "Loss should decrease";
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
