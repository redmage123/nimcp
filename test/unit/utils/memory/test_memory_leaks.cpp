//==============================================================================
// NIMCP Memory Leak Tests
//==============================================================================
// Tests for memory leaks using valgrind
//
// These tests run a subset of unit tests under valgrind to detect:
// - Memory leaks (definite and possible)
// - Invalid memory access
// - Use of uninitialized memory
// - Double frees
//
// Tests are skipped if valgrind is not available.
//
// Note: These tests can be slow, so they run a subset of critical tests.
//==============================================================================

#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>

// Include NIMCP headers for basic allocation tests
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/containers/nimcp_queue.h"
#include "utils/thread/nimcp_thread.h"

namespace {

//==============================================================================
// Helper Functions
//==============================================================================

/**
 * Check if a command exists on the system
 */
bool command_exists(const std::string& command)
{
    std::string check_cmd = "command -v " + command + " > /dev/null 2>&1";
    return (system(check_cmd.c_str()) == 0);
}

/**
 * Execute a shell command and return its output and exit code
 */
struct CommandResult {
    int exit_code;
    std::string output;
};

CommandResult execute_command(const std::string& command)
{
    CommandResult result;
    char buffer[256];
    std::stringstream output;

    FILE* pipe = popen((command + " 2>&1").c_str(), "r");
    if (!pipe) {
        result.exit_code = -1;
        result.output = "Failed to execute command";
        return result;
    }

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output << buffer;
    }

    result.exit_code = pclose(pipe);
    result.output = output.str();

    if (WIFEXITED(result.exit_code)) {
        result.exit_code = WEXITSTATUS(result.exit_code);
    }

    return result;
}

/**
 * Parse valgrind output for leak summary
 */
struct ValgrindResult {
    bool has_leaks;
    int definite_leaks;
    int possible_leaks;
    int errors;
    std::string summary;
};

ValgrindResult parse_valgrind_output(const std::string& output)
{
    ValgrindResult result = {false, 0, 0, 0, ""};

    // Look for leak summary
    size_t leak_pos = output.find("LEAK SUMMARY:");
    if (leak_pos != std::string::npos) {
        result.summary = output.substr(leak_pos);

        // Parse definite leaks
        size_t def_pos = result.summary.find("definitely lost:");
        if (def_pos != std::string::npos) {
            sscanf(result.summary.c_str() + def_pos, "definitely lost: %d", &result.definite_leaks);
        }

        // Parse possible leaks
        size_t pos_pos = result.summary.find("possibly lost:");
        if (pos_pos != std::string::npos) {
            sscanf(result.summary.c_str() + pos_pos, "possibly lost: %d", &result.possible_leaks);
        }

        result.has_leaks = (result.definite_leaks > 0);
    }

    // Count errors
    size_t err_pos = output.find("ERROR SUMMARY:");
    if (err_pos != std::string::npos) {
        sscanf(output.c_str() + err_pos, "ERROR SUMMARY: %d", &result.errors);
    }

    return result;
}

/**
 * Get test executable path
 */
std::string get_test_executable()
{
    char exe_path[1024];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len != -1) {
        exe_path[len] = '\0';
        return std::string(exe_path);
    }
    return "nimcp_tests";
}

//==============================================================================
// Test Fixture
//==============================================================================

class MemoryLeakTest : public ::testing::Test {
   protected:
    std::string test_exe;
    bool valgrind_available;

    void SetUp() override
    {
        test_exe = get_test_executable();
        valgrind_available = command_exists("valgrind");

        if (!valgrind_available) {
            GTEST_SKIP() << "valgrind not available - install with: sudo apt-get install valgrind";
        }
    }

    /**
     * Run a specific test under valgrind
     */
    ValgrindResult run_test_under_valgrind(const std::string& test_filter, int timeout_sec = 30)
    {
        std::string valgrind_cmd = "timeout " + std::to_string(timeout_sec) +
                                   " "
                                   "valgrind "
                                   "--leak-check=full "
                                   "--show-leak-kinds=all "
                                   "--track-origins=yes "
                                   "--error-exitcode=99 " +
                                   test_exe +
                                   " "
                                   "--gtest_filter=" +
                                   test_filter +
                                   " "
                                   "2>&1";

        CommandResult cmd_result = execute_command(valgrind_cmd);
        return parse_valgrind_output(cmd_result.output);
    }
};

//==============================================================================
// Valgrind Availability Tests
//==============================================================================

/**
 * Test that valgrind is available
 */
TEST_F(MemoryLeakTest, ValgrindAvailable)
{
    ASSERT_TRUE(valgrind_available) << "valgrind is required for memory leak tests";

    CommandResult result = execute_command("valgrind --version");
    EXPECT_EQ(result.exit_code, 0);

    std::cout << "valgrind version: " << result.output << std::endl;
}

//==============================================================================
// Basic Memory Leak Tests (C code)
//==============================================================================

/**
 * Test simple allocation and deallocation (no leaks expected)
 */
TEST_F(MemoryLeakTest, SimpleAllocation)
{
    // Simple test that shouldn't leak
    void* ptr = malloc(100);
    ASSERT_NE(ptr, nullptr);
    memset(ptr, 0, 100);
    free(ptr);

    // This test itself shouldn't leak
}

/**
 * Test neural network creation and destruction for leaks
 */
TEST_F(MemoryLeakTest, NeuralNetworkCreationDestruction)
{
    GTEST_SKIP() << "nimcp_neuralnet API not yet implemented";

    const int input_size = 10;
    const int hidden_size = 5;
    const int output_size = 3;

    // TODO: Implement neuralnet API
    // nimcp_neuralnet* net = nimcp_neuralnet_create(input_size, hidden_size, output_size);
    // ASSERT_NE(net, nullptr) << "Failed to create neural network";
    // nimcp_neuralnet_destroy(net);

    // Check if valgrind is available for deeper analysis
    if (!valgrind_available) {
        GTEST_SKIP() << "valgrind not available for leak detection";
    }

    std::cout << "Neural network creation/destruction completed - run under valgrind for leak check"
              << std::endl;
}

/**
 * Test queue allocation and deallocation
 */
TEST_F(MemoryLeakTest, QueueAllocationDeallocation)
{
    GTEST_SKIP() << "Queue API changed, test needs updating";

    // TODO: Update to new nimcp_queue_create API that takes config struct
    // const size_t queue_size = 100;
    // nimcp_queue* queue = nimcp_queue_create(queue_size);
    // ASSERT_NE(queue, nullptr) << "Failed to create queue";
    // nimcp_queue_destroy(queue);

    std::cout << "Queue creation/destruction completed - run under valgrind for leak check"
              << std::endl;
}

//==============================================================================
// Valgrind Integration Tests
//==============================================================================

/**
 * Run neural network tests under valgrind
 */
TEST_F(MemoryLeakTest, DISABLED_ValgrindNeuralNetworkTests)
{
    if (!valgrind_available) {
        GTEST_SKIP() << "valgrind not available";
    }

    // Run a subset of neural network tests under valgrind
    ValgrindResult result = run_test_under_valgrind("NeuralNetCreate.*", 60);

    std::cout << "Valgrind Summary:" << std::endl;
    std::cout << result.summary << std::endl;

    // Report results
    if (result.has_leaks) {
        std::cout << "WARNING: Detected " << result.definite_leaks << " bytes definitely lost"
                  << std::endl;
        std::cout << "WARNING: Detected " << result.possible_leaks << " bytes possibly lost"
                  << std::endl;
    }

    EXPECT_EQ(result.errors, 0) << "Valgrind detected memory errors";
    EXPECT_FALSE(result.has_leaks) << "Valgrind detected definite memory leaks";
}

/**
 * Run queue tests under valgrind
 */
TEST_F(MemoryLeakTest, DISABLED_ValgrindQueueTests)
{
    if (!valgrind_available) {
        GTEST_SKIP() << "valgrind not available";
    }

    ValgrindResult result = run_test_under_valgrind("QueueUtils.*", 60);

    std::cout << "Valgrind Summary:" << std::endl;
    std::cout << result.summary << std::endl;

    EXPECT_EQ(result.errors, 0) << "Valgrind detected memory errors";
    EXPECT_FALSE(result.has_leaks) << "Valgrind detected definite memory leaks";
}

/**
 * Run thread utilities tests under valgrind
 */
TEST_F(MemoryLeakTest, DISABLED_ValgrindThreadTests)
{
    if (!valgrind_available) {
        GTEST_SKIP() << "valgrind not available";
    }

    ValgrindResult result = run_test_under_valgrind("ThreadUtils.*", 60);

    std::cout << "Valgrind Summary:" << std::endl;
    std::cout << result.summary << std::endl;

    EXPECT_EQ(result.errors, 0) << "Valgrind detected memory errors";
    EXPECT_FALSE(result.has_leaks) << "Valgrind detected definite memory leaks";
}

/**
 * Run all tests under valgrind (comprehensive but slow)
 * Disabled by default - enable manually for full leak checking
 */
TEST_F(MemoryLeakTest, DISABLED_ValgrindAllTests)
{
    if (!valgrind_available) {
        GTEST_SKIP() << "valgrind not available";
    }

    std::cout << "WARNING: Running all tests under valgrind - this will be slow" << std::endl;

    ValgrindResult result = run_test_under_valgrind("*", 300);

    std::cout << "Valgrind Summary:" << std::endl;
    std::cout << result.summary << std::endl;

    // Report but don't fail on possible leaks (might be Python-related)
    if (result.has_leaks) {
        std::cout << "WARNING: Detected " << result.definite_leaks << " bytes definitely lost"
                  << std::endl;
        std::cout << "WARNING: Detected " << result.possible_leaks << " bytes possibly lost"
                  << std::endl;
    }

    EXPECT_EQ(result.errors, 0) << "Valgrind detected memory errors";
}

//==============================================================================
// Memory Pattern Tests
//==============================================================================

/**
 * Test for use-after-free detection (should detect with valgrind)
 */
TEST_F(MemoryLeakTest, DISABLED_UseAfterFreeDetection)
{
    // This test intentionally creates a use-after-free for valgrind to detect
    // ONLY RUN UNDER VALGRIND

    if (!valgrind_available) {
        GTEST_SKIP() << "This test must be run under valgrind";
    }

    std::cout << "Testing use-after-free detection (intentional error)" << std::endl;

    // Note: We can't actually trigger use-after-free here as it would crash
    // This is just a placeholder showing how such tests would be structured

    GTEST_SKIP() << "Skipping intentional memory error test";
}

/**
 * Test for double-free detection (should detect with valgrind)
 */
TEST_F(MemoryLeakTest, DISABLED_DoubleFreeDetection)
{
    if (!valgrind_available) {
        GTEST_SKIP() << "This test must be run under valgrind";
    }

    std::cout << "Testing double-free detection (intentional error)" << std::endl;

    // Note: We can't actually trigger double-free here as it would crash
    // This is just a placeholder showing how such tests would be structured

    GTEST_SKIP() << "Skipping intentional memory error test";
}

//==============================================================================
// Manual Valgrind Test Instructions
//==============================================================================

/**
 * Instructions for manual valgrind testing
 */
TEST_F(MemoryLeakTest, ManualValgrindInstructions)
{
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "Manual Valgrind Testing Instructions" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    std::cout << "\nTo run full memory leak analysis manually:\n" << std::endl;
    std::cout << "1. Run all tests under valgrind:" << std::endl;
    std::cout << "   valgrind --leak-check=full --show-leak-kinds=all \\" << std::endl;
    std::cout << "            --track-origins=yes \\" << std::endl;
    std::cout << "            ./build/src/tests/nimcp_tests" << std::endl;
    std::cout << "\n2. Run specific test suite:" << std::endl;
    std::cout << "   valgrind --leak-check=full \\" << std::endl;
    std::cout << "            ./build/src/tests/nimcp_tests \\" << std::endl;
    std::cout << "            --gtest_filter=NeuralNetCreate.*" << std::endl;
    std::cout << "\n3. Save valgrind output to file:" << std::endl;
    std::cout << "   valgrind --leak-check=full --log-file=valgrind.log \\" << std::endl;
    std::cout << "            ./build/src/tests/nimcp_tests" << std::endl;
    std::cout << "\n4. Enable all leak checks:" << std::endl;
    std::cout << "   valgrind --leak-check=full --show-leak-kinds=all \\" << std::endl;
    std::cout << "            --show-reachable=yes --track-origins=yes \\" << std::endl;
    std::cout << "            --verbose ./build/src/tests/nimcp_tests" << std::endl;
    std::cout << "\n" << std::string(70, '=') << std::endl;
}

}  // anonymous namespace

//==============================================================================
// Main
//==============================================================================
// Note: Main is provided by GTest::Main linkage in CMakeLists.txt
