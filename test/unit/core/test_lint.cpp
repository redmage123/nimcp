//==============================================================================
// NIMCP Lint Tests
//==============================================================================
// Tests for code quality, linting, and static analysis
//
// These tests run various linting and quality tools and verify their results.
// Tests are skipped gracefully if required tools are not available.
//
// Tests include:
// - Lint script execution
// - Code formatting compliance
// - File size limits
// - Complexity metrics
// - Common code issues detection
//==============================================================================

#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

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
    char buffer[128];
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

    // Extract actual exit code
    if (WIFEXITED(result.exit_code)) {
        result.exit_code = WEXITSTATUS(result.exit_code);
    }

    return result;
}

/**
 * Get project root directory
 */
std::string get_project_root()
{
    // Try to find project root by looking for CMakeLists.txt
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == nullptr) {
        return "";
    }

    std::string path = cwd;
    while (!path.empty() && path != "/") {
        struct stat buffer;
        std::string cmake_file = path + "/CMakeLists.txt";
        if (stat(cmake_file.c_str(), &buffer) == 0) {
            return path;
        }
        size_t pos = path.find_last_of('/');
        if (pos == std::string::npos)
            break;
        path = path.substr(0, pos);
    }

    return cwd;  // Fallback to current directory
}

/**
 * Count lines in a file
 */
int count_file_lines(const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return -1;
    }

    int lines = 0;
    std::string line;
    while (std::getline(file, line)) {
        ++lines;
    }

    return lines;
}

//==============================================================================
// Test Fixture
//==============================================================================

class LintTest : public ::testing::Test {
   protected:
    std::string project_root;

    void SetUp() override
    {
        project_root = get_project_root();
        ASSERT_FALSE(project_root.empty()) << "Failed to find project root";
    }
};

//==============================================================================
// Lint Script Tests
//==============================================================================

/**
 * Test that the lint script exists and is executable
 */
TEST_F(LintTest, LintScriptExists)
{
    std::string script_path = project_root + "/scripts/lint.sh";

    struct stat buffer;
    ASSERT_EQ(stat(script_path.c_str(), &buffer), 0) << "Lint script not found at: " << script_path;

    // Check if executable
    EXPECT_TRUE(buffer.st_mode & S_IXUSR) << "Lint script is not executable";
}

/**
 * Test lint script execution
 * Note: This test may fail if code has linting issues, which is expected
 */
TEST_F(LintTest, LintScriptExecutes)
{
    if (!command_exists("bash")) {
        GTEST_SKIP() << "bash not available";
    }

    std::string script_path = project_root + "/scripts/lint.sh";
    struct stat buffer;
    if (stat(script_path.c_str(), &buffer) != 0) {
        GTEST_SKIP() << "Lint script not found";
    }

    CommandResult result = execute_command("cd " + project_root + " && " + script_path);

    // Script should execute (exit code 0 or 1, not crash with -1 or 127)
    EXPECT_NE(result.exit_code, -1) << "Failed to execute lint script";
    EXPECT_NE(result.exit_code, 127) << "Lint script not found or not executable";

    // Output should contain some information
    EXPECT_FALSE(result.output.empty()) << "Lint script produced no output";

    std::cout << "Lint script output:\n" << result.output << std::endl;

    // Note: We don't ASSERT exit code 0 because the code may have linting issues
    if (result.exit_code != 0) {
        std::cout << "WARNING: Lint script found issues (exit code: " << result.exit_code << ")"
                  << std::endl;
    }
}

//==============================================================================
// Code Formatting Tests
//==============================================================================

/**
 * Test that clang-format is available (optional)
 */
TEST_F(LintTest, ClangFormatAvailable)
{
    if (!command_exists("clang-format")) {
        GTEST_SKIP() << "clang-format not installed";
    }

    CommandResult result = execute_command("clang-format --version");
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_FALSE(result.output.empty());

    std::cout << "clang-format version: " << result.output << std::endl;
}

/**
 * Test a sample file for formatting compliance (if clang-format available)
 */
TEST_F(LintTest, SampleFileFormatting)
{
    if (!command_exists("clang-format")) {
        GTEST_SKIP() << "clang-format not installed";
    }

    // Find a sample C file to test
    std::string find_cmd = "find " + project_root + "/src/lib -name '*.c' | head -1";
    CommandResult find_result = execute_command(find_cmd);

    if (find_result.exit_code != 0 || find_result.output.empty()) {
        GTEST_SKIP() << "No C files found to test";
    }

    std::string sample_file = find_result.output;
    // Remove trailing newline
    sample_file.erase(sample_file.find_last_not_of("\n\r") + 1);

    // Check formatting
    std::string format_cmd = "clang-format --dry-run --Werror " + sample_file + " 2>&1";
    CommandResult format_result = execute_command(format_cmd);

    // Note: We don't assert success, just log the result
    if (format_result.exit_code != 0) {
        std::cout << "WARNING: Formatting issues in " << sample_file << std::endl;
        std::cout << format_result.output << std::endl;
    } else {
        std::cout << "File properly formatted: " << sample_file << std::endl;
    }
}

//==============================================================================
// File Size Tests
//==============================================================================

/**
 * Test that no source files exceed reasonable size limits
 */
TEST_F(LintTest, FileSizeLimits)
{
    const int MAX_LINES = 2000;
    std::vector<std::string> oversized_files;

    std::string find_cmd = "find " + project_root + "/src/lib -name '*.c' 2>/dev/null";
    CommandResult find_result = execute_command(find_cmd);

    if (find_result.exit_code != 0) {
        GTEST_SKIP() << "Failed to find source files";
    }

    std::istringstream iss(find_result.output);
    std::string filepath;

    while (std::getline(iss, filepath)) {
        if (filepath.empty())
            continue;

        int lines = count_file_lines(filepath);
        if (lines > MAX_LINES) {
            oversized_files.push_back(filepath + " (" + std::to_string(lines) + " lines)");
        }
    }

    if (!oversized_files.empty()) {
        std::cout << "WARNING: Files exceeding " << MAX_LINES << " lines:" << std::endl;
        for (const auto& file : oversized_files) {
            std::cout << "  - " << file << std::endl;
        }
    }

    // Note: We don't fail the test, just warn
    EXPECT_TRUE(oversized_files.empty())
        << "Found " << oversized_files.size() << " oversized files";
}

//==============================================================================
// Static Analysis Tests
//==============================================================================

/**
 * Test that cppcheck is available and can run (optional)
 */
TEST_F(LintTest, CppCheckAvailable)
{
    if (!command_exists("cppcheck")) {
        GTEST_SKIP() << "cppcheck not installed";
    }

    CommandResult result = execute_command("cppcheck --version");
    EXPECT_EQ(result.exit_code, 0);

    std::cout << "cppcheck version: " << result.output << std::endl;
}

/**
 * Run basic cppcheck on a sample file
 */
TEST_F(LintTest, CppCheckSample)
{
    if (!command_exists("cppcheck")) {
        GTEST_SKIP() << "cppcheck not installed";
    }

    // Find a sample file
    std::string find_cmd = "find " + project_root + "/src/lib -name '*.c' | head -1";
    CommandResult find_result = execute_command(find_cmd);

    if (find_result.exit_code != 0 || find_result.output.empty()) {
        GTEST_SKIP() << "No C files found to test";
    }

    std::string sample_file = find_result.output;
    sample_file.erase(sample_file.find_last_not_of("\n\r") + 1);

    std::string check_cmd = "cppcheck --enable=warning --quiet "
                            "--suppress=missingIncludeSystem "
                            "-I" +
                            project_root + "/src/include " + sample_file + " 2>&1";

    CommandResult check_result = execute_command(check_cmd);

    // Log results but don't fail test
    if (!check_result.output.empty()) {
        std::cout << "cppcheck output for " << sample_file << ":" << std::endl;
        std::cout << check_result.output << std::endl;
    } else {
        std::cout << "cppcheck: No issues found in " << sample_file << std::endl;
    }
}

//==============================================================================
// Code Quality Checks
//==============================================================================

/**
 * Check for FIXME/XXX comments that need attention
 */
TEST_F(LintTest, CheckForFIXME)
{
    std::string grep_cmd = "grep -r 'FIXME\\|XXX' " + project_root + "/src/ 2>/dev/null || true";
    CommandResult result = execute_command(grep_cmd);

    if (!result.output.empty()) {
        std::cout << "WARNING: Found FIXME/XXX comments:" << std::endl;
        std::cout << result.output << std::endl;
    } else {
        std::cout << "No FIXME/XXX comments found" << std::endl;
    }

    // Don't fail test, just informational
}

/**
 * Count TODO comments (informational)
 */
TEST_F(LintTest, CheckForTODO)
{
    std::string grep_cmd = "grep -rc 'TODO' " + project_root + "/src/ 2>/dev/null || echo '0'";
    CommandResult result = execute_command(grep_cmd);

    std::cout << "TODO comment statistics:" << std::endl;
    std::cout << result.output << std::endl;

    // Don't fail test, just informational
}

//==============================================================================
// Complexity Metrics
//==============================================================================

/**
 * Test code complexity using lizard if available
 */
TEST_F(LintTest, CodeComplexity)
{
    if (!command_exists("lizard")) {
        GTEST_SKIP() << "lizard not installed (optional)";
    }

    std::string lizard_cmd = "lizard -l cpp " + project_root + "/src/lib/ 2>/dev/null";
    CommandResult result = execute_command(lizard_cmd);

    if (result.exit_code == 0) {
        std::cout << "Code complexity analysis:" << std::endl;
        std::cout << result.output << std::endl;
    }

    // Don't fail test, just informational
}

//==============================================================================
// Memory Safety Checks
//==============================================================================

/**
 * Check for common memory issues in test code
 */
TEST_F(LintTest, MemorySafetyPatterns)
{
    // Look for common patterns that might indicate memory issues
    std::vector<std::string> patterns = {
        "malloc.*without.*free",
        "strcpy",   // Prefer strncpy
        "sprintf",  // Prefer snprintf
        "gets"      // Unsafe function
    };

    for (const auto& pattern : patterns) {
        std::string grep_cmd =
            "grep -rn '" + pattern + "' " + project_root + "/src/lib/ 2>/dev/null || true";
        CommandResult result = execute_command(grep_cmd);

        if (!result.output.empty()) {
            std::cout << "WARNING: Found pattern '" << pattern << "':" << std::endl;
            std::cout << result.output << std::endl;
        }
    }

    // Don't fail test, just informational
}

}  // anonymous namespace

//==============================================================================
// Main
//==============================================================================
// Note: Main is provided by GTest::Main linkage in CMakeLists.txt
