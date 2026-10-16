/**
 * @file test_config.cpp
 * @brief Comprehensive unit tests for NIMCP configuration system
 *
 * Tests both YAML/JSON config loading (nimcp_config.c) and
 * dynamic runtime configuration (nimcp_dynamic_config.c)
 */

#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <cstdio>
#include <sys/stat.h>

// Headers have their own extern "C" guards
#include "utils/config/nimcp_config.h"
#include "utils/config/nimcp_dynamic_config.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temp directory for test files
        system("mkdir -p /tmp/nimcp_config_test");
    }

    void TearDown() override {
        // Clean up test files
        system("rm -rf /tmp/nimcp_config_test");
    }

    void CreateYAMLConfig(const char* filename, const char* content) {
        std::string path = std::string("/tmp/nimcp_config_test/") + filename;
        std::ofstream file(path);
        file << content;
        file.close();
    }

    void CreateJSONConfig(const char* filename, const char* content) {
        std::string path = std::string("/tmp/nimcp_config_test/") + filename;
        std::ofstream file(path);
        file << content;
        file.close();
    }

    void CreateINIConfig(const char* filename, const char* content) {
        std::string path = std::string("/tmp/nimcp_config_test/") + filename;
        std::ofstream file(path);
        file << content;
        file.close();
    }

    std::string GetTestPath(const char* filename) {
        return std::string("/tmp/nimcp_config_test/") + filename;
    }
};

//=============================================================================
// Tests for nimcp_config.h (YAML/JSON Brain Config)
//=============================================================================

TEST_F(ConfigTest, InitDefaults)
{
    nimcp_brain_config_t config;
    nimcp_config_init_defaults(&config);

    // Verify all default values
    EXPECT_STREQ(config.name, "default_brain");
    EXPECT_EQ(config.size, 1);  // small
    EXPECT_EQ(config.task, 0);  // classification

    EXPECT_EQ(config.num_inputs, 10);
    EXPECT_EQ(config.num_outputs, 3);
    EXPECT_EQ(config.num_hidden, 100);
    EXPECT_FLOAT_EQ(config.learning_rate, 0.01f);

    EXPECT_EQ(config.max_epochs, 100);
    EXPECT_EQ(config.batch_size, 32);
    EXPECT_FLOAT_EQ(config.validation_split, 0.2f);
    EXPECT_TRUE(config.early_stopping);
    EXPECT_EQ(config.patience, 10);

    EXPECT_TRUE(config.enable_bcm);
    EXPECT_FLOAT_EQ(config.bcm_tau, 1000.0f);
    EXPECT_FALSE(config.enable_stdp);
    EXPECT_FLOAT_EQ(config.stdp_window, 20.0f);

    EXPECT_FALSE(config.ethics_enabled);
    EXPECT_FLOAT_EQ(config.golden_rule_threshold, 0.0f);
    EXPECT_FLOAT_EQ(config.empathy_weight, 0.5f);

    EXPECT_STREQ(config.model_path, "/tmp/brain.model");
    EXPECT_EQ(config.checkpoint_interval, 10);
}

TEST_F(ConfigTest, LoadYAML_Valid)
{
    const char* yaml_content = R"(
brain:
  name: test_brain
  size: large
  task: regression
  model_path: /tmp/test.model
  checkpoint_interval: 20

architecture:
  num_inputs: 256
  num_outputs: 10
  num_hidden: 512
  learning_rate: 0.001

training:
  max_epochs: 200
  batch_size: 64
  validation_split: 0.3
  early_stopping: false
  patience: 15

plasticity:
  enable_bcm: false
  bcm_tau: 500.0
  enable_stdp: true
  stdp_window: 40.0

ethics:
  enabled: true
  golden_rule_threshold: 0.7
  empathy_weight: 0.8
)";

    CreateYAMLConfig("test.yaml", yaml_content);
    std::string path = GetTestPath("test.yaml");

    nimcp_brain_config_t config;
    ASSERT_TRUE(nimcp_config_load_yaml(path.c_str(), &config));

    // Verify parsed values
    EXPECT_STREQ(config.name, "test_brain");
    EXPECT_EQ(config.size, 3);  // large
    EXPECT_EQ(config.task, 1);  // regression

    EXPECT_EQ(config.num_inputs, 256);
    EXPECT_EQ(config.num_outputs, 10);
    EXPECT_EQ(config.num_hidden, 512);
    EXPECT_FLOAT_EQ(config.learning_rate, 0.001f);

    EXPECT_EQ(config.max_epochs, 200);
    EXPECT_EQ(config.batch_size, 64);
    EXPECT_FLOAT_EQ(config.validation_split, 0.3f);
    EXPECT_FALSE(config.early_stopping);
    EXPECT_EQ(config.patience, 15);

    EXPECT_FALSE(config.enable_bcm);
    EXPECT_FLOAT_EQ(config.bcm_tau, 500.0f);
    EXPECT_TRUE(config.enable_stdp);
    EXPECT_FLOAT_EQ(config.stdp_window, 40.0f);

    EXPECT_TRUE(config.ethics_enabled);
    EXPECT_FLOAT_EQ(config.golden_rule_threshold, 0.7f);
    EXPECT_FLOAT_EQ(config.empathy_weight, 0.8f);

    EXPECT_STREQ(config.model_path, "/tmp/test.model");
    EXPECT_EQ(config.checkpoint_interval, 20);
}

TEST_F(ConfigTest, LoadYAML_FileNotFound)
{
    nimcp_brain_config_t config;
    EXPECT_FALSE(nimcp_config_load_yaml("/nonexistent/file.yaml", &config));
}

TEST_F(ConfigTest, LoadYAML_EmptyFile)
{
    CreateYAMLConfig("empty.yaml", "");
    std::string path = GetTestPath("empty.yaml");

    nimcp_brain_config_t config;
    ASSERT_TRUE(nimcp_config_load_yaml(path.c_str(), &config));

    // Should have defaults
    EXPECT_STREQ(config.name, "default_brain");
}

TEST_F(ConfigTest, LoadYAML_CommentsAndWhitespace)
{
    const char* yaml_content = R"(
# This is a comment
brain:
  # Another comment
  name: commented_brain
  size: medium

  # Empty lines should be ignored
  task: classification
)";

    CreateYAMLConfig("comments.yaml", yaml_content);
    std::string path = GetTestPath("comments.yaml");

    nimcp_brain_config_t config;
    ASSERT_TRUE(nimcp_config_load_yaml(path.c_str(), &config));

    EXPECT_STREQ(config.name, "commented_brain");
    EXPECT_EQ(config.size, 2);  // medium
}

TEST_F(ConfigTest, LoadYAML_QuotedStrings)
{
    const char* yaml_content = R"(
brain:
  name: "quoted_brain"
  model_path: '/tmp/quoted.model'
)";

    CreateYAMLConfig("quoted.yaml", yaml_content);
    std::string path = GetTestPath("quoted.yaml");

    nimcp_brain_config_t config;
    ASSERT_TRUE(nimcp_config_load_yaml(path.c_str(), &config));

    EXPECT_STREQ(config.name, "quoted_brain");
    EXPECT_STREQ(config.model_path, "/tmp/quoted.model");
}

TEST_F(ConfigTest, LoadJSON_Valid)
{
    const char* json_content = R"({
  "name": "json_brain",
  "num_inputs": 128,
  "num_outputs": 5,
  "num_hidden": 256,
  "learning_rate": 0.005,
  "max_epochs": 150,
  "batch_size": 48,
  "validation_split": 0.25,
  "early_stopping": true,
  "patience": 12,
  "enable_bcm": true,
  "bcm_tau": 800.0,
  "enable_stdp": false,
  "stdp_window": 30.0,
  "ethics_enabled": false,
  "golden_rule_threshold": 0.5,
  "empathy_weight": 0.6
})";

    CreateJSONConfig("test.json", json_content);
    std::string path = GetTestPath("test.json");

    nimcp_brain_config_t config;
    ASSERT_TRUE(nimcp_config_load_json(path.c_str(), &config));

    EXPECT_STREQ(config.name, "json_brain");
    EXPECT_EQ(config.num_inputs, 128);
    EXPECT_EQ(config.num_outputs, 5);
    EXPECT_EQ(config.num_hidden, 256);
    EXPECT_FLOAT_EQ(config.learning_rate, 0.005f);
}

TEST_F(ConfigTest, LoadJSON_FileNotFound)
{
    nimcp_brain_config_t config;
    EXPECT_FALSE(nimcp_config_load_json("/nonexistent/file.json", &config));
}

TEST_F(ConfigTest, LoadJSON_InvalidSyntax)
{
    const char* invalid_json = R"({
  "name": "invalid
  "num_inputs": 128
})";

    CreateJSONConfig("invalid.json", invalid_json);
    std::string path = GetTestPath("invalid.json");

    nimcp_brain_config_t config;
    EXPECT_FALSE(nimcp_config_load_json(path.c_str(), &config));
}

TEST_F(ConfigTest, AutoLoad_YAML)
{
    const char* yaml_content = R"(
brain:
  name: auto_yaml
)";

    CreateYAMLConfig("auto.yaml", yaml_content);
    std::string path = GetTestPath("auto.yaml");

    nimcp_brain_config_t config;
    ASSERT_TRUE(nimcp_config_load(path.c_str(), &config));
    EXPECT_STREQ(config.name, "auto_yaml");
}

TEST_F(ConfigTest, AutoLoad_YML)
{
    const char* yaml_content = R"(
brain:
  name: auto_yml
)";

    CreateYAMLConfig("auto.yml", yaml_content);
    std::string path = GetTestPath("auto.yml");

    nimcp_brain_config_t config;
    ASSERT_TRUE(nimcp_config_load(path.c_str(), &config));
    EXPECT_STREQ(config.name, "auto_yml");
}

TEST_F(ConfigTest, AutoLoad_JSON)
{
    const char* json_content = R"({
  "name": "auto_json"
})";

    CreateJSONConfig("auto.json", json_content);
    std::string path = GetTestPath("auto.json");

    nimcp_brain_config_t config;
    ASSERT_TRUE(nimcp_config_load(path.c_str(), &config));
    EXPECT_STREQ(config.name, "auto_json");
}

TEST_F(ConfigTest, SizeEnumParsing)
{
    const char* yaml_content = R"(
brain:
  size: tiny
)";
    CreateYAMLConfig("size_tiny.yaml", yaml_content);
    nimcp_brain_config_t config;
    ASSERT_TRUE(nimcp_config_load_yaml(GetTestPath("size_tiny.yaml").c_str(), &config));
    EXPECT_EQ(config.size, 0);

    yaml_content = R"(
brain:
  size: small
)";
    CreateYAMLConfig("size_small.yaml", yaml_content);
    ASSERT_TRUE(nimcp_config_load_yaml(GetTestPath("size_small.yaml").c_str(), &config));
    EXPECT_EQ(config.size, 1);

    yaml_content = R"(
brain:
  size: medium
)";
    CreateYAMLConfig("size_medium.yaml", yaml_content);
    ASSERT_TRUE(nimcp_config_load_yaml(GetTestPath("size_medium.yaml").c_str(), &config));
    EXPECT_EQ(config.size, 2);

    yaml_content = R"(
brain:
  size: large
)";
    CreateYAMLConfig("size_large.yaml", yaml_content);
    ASSERT_TRUE(nimcp_config_load_yaml(GetTestPath("size_large.yaml").c_str(), &config));
    EXPECT_EQ(config.size, 3);
}

TEST_F(ConfigTest, TaskEnumParsing)
{
    nimcp_brain_config_t config;

    // classification
    const char* yaml_content = R"(brain:
  task: classification)";
    CreateYAMLConfig("task1.yaml", yaml_content);
    ASSERT_TRUE(nimcp_config_load_yaml(GetTestPath("task1.yaml").c_str(), &config));
    EXPECT_EQ(config.task, 0);

    // regression
    yaml_content = R"(brain:
  task: regression)";
    CreateYAMLConfig("task2.yaml", yaml_content);
    ASSERT_TRUE(nimcp_config_load_yaml(GetTestPath("task2.yaml").c_str(), &config));
    EXPECT_EQ(config.task, 1);

    // pattern_matching
    yaml_content = R"(brain:
  task: pattern_matching)";
    CreateYAMLConfig("task3.yaml", yaml_content);
    ASSERT_TRUE(nimcp_config_load_yaml(GetTestPath("task3.yaml").c_str(), &config));
    EXPECT_EQ(config.task, 2);

    // sequence
    yaml_content = R"(brain:
  task: sequence)";
    CreateYAMLConfig("task4.yaml", yaml_content);
    ASSERT_TRUE(nimcp_config_load_yaml(GetTestPath("task4.yaml").c_str(), &config));
    EXPECT_EQ(config.task, 3);

    // association
    yaml_content = R"(brain:
  task: association)";
    CreateYAMLConfig("task5.yaml", yaml_content);
    ASSERT_TRUE(nimcp_config_load_yaml(GetTestPath("task5.yaml").c_str(), &config));
    EXPECT_EQ(config.task, 4);
}

//=============================================================================
// Tests for nimcp_dynamic_config.h (Runtime Config)
//=============================================================================

class DynamicConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        system("mkdir -p /tmp/nimcp_dynamic_test");
        config_shutdown();  // Clean state
    }

    void TearDown() override {
        config_shutdown();
        system("rm -rf /tmp/nimcp_dynamic_test");
    }

    void CreateConfig(const char* filename, const char* content) {
        std::string path = std::string("/tmp/nimcp_dynamic_test/") + filename;
        std::ofstream file(path);
        file << content;
        file.close();
    }

    std::string GetTestPath(const char* filename) {
        return std::string("/tmp/nimcp_dynamic_test/") + filename;
    }
};

TEST_F(DynamicConfigTest, Init_Valid)
{
    const char* config_content = R"(
# Test config
learning_rate = 0.001
batch_size = 32
enable_cache = true
model_path = /tmp/model_file
)";

    CreateConfig("test.ini", config_content);
    std::string path = GetTestPath("test.ini");

    ASSERT_TRUE(config_init(path.c_str()));

    EXPECT_DOUBLE_EQ(config_get_float("learning_rate", 0.0), 0.001);
    EXPECT_EQ(config_get_int("batch_size", 0), 32);
    EXPECT_TRUE(config_get_bool("enable_cache", false));
    EXPECT_STREQ(config_get_string("model_path", ""), "/tmp/model_file");
}

TEST_F(DynamicConfigTest, Init_NullPath)
{
    EXPECT_FALSE(config_init(nullptr));
}

TEST_F(DynamicConfigTest, Init_FileNotFound)
{
    EXPECT_FALSE(config_init("/nonexistent/path/config.ini"));
}

TEST_F(DynamicConfigTest, GetInt_Valid)
{
    const char* config_content = "epochs = 100\n";
    CreateConfig("int_test.ini", config_content);

    ASSERT_TRUE(config_init(GetTestPath("int_test.ini").c_str()));
    EXPECT_EQ(config_get_int("epochs", 0), 100);
}

TEST_F(DynamicConfigTest, GetInt_NotFound)
{
    const char* config_content = "epochs = 100\n";
    CreateConfig("int_test2.ini", config_content);

    ASSERT_TRUE(config_init(GetTestPath("int_test2.ini").c_str()));
    EXPECT_EQ(config_get_int("nonexistent", 42), 42);
}

TEST_F(DynamicConfigTest, GetInt_NullKey)
{
    const char* config_content = "epochs = 100\n";
    CreateConfig("int_test3.ini", config_content);

    ASSERT_TRUE(config_init(GetTestPath("int_test3.ini").c_str()));
    EXPECT_EQ(config_get_int(nullptr, 99), 99);
}

TEST_F(DynamicConfigTest, GetFloat_Valid)
{
    const char* config_content = "learning_rate = 0.001\n";
    CreateConfig("float_test.ini", config_content);

    ASSERT_TRUE(config_init(GetTestPath("float_test.ini").c_str()));
    EXPECT_DOUBLE_EQ(config_get_float("learning_rate", 0.0), 0.001);
}

TEST_F(DynamicConfigTest, GetFloat_NotFound)
{
    const char* config_content = "learning_rate = 0.001\n";
    CreateConfig("float_test2.ini", config_content);

    ASSERT_TRUE(config_init(GetTestPath("float_test2.ini").c_str()));
    EXPECT_DOUBLE_EQ(config_get_float("nonexistent", 3.14), 3.14);
}

TEST_F(DynamicConfigTest, GetFloat_NullKey)
{
    const char* config_content = "learning_rate = 0.001\n";
    CreateConfig("float_test3.ini", config_content);

    ASSERT_TRUE(config_init(GetTestPath("float_test3.ini").c_str()));
    EXPECT_DOUBLE_EQ(config_get_float(nullptr, 2.71), 2.71);
}

TEST_F(DynamicConfigTest, GetBool_True)
{
    const char* config_content = "enable_feature = true\n";
    CreateConfig("bool_test.ini", config_content);

    ASSERT_TRUE(config_init(GetTestPath("bool_test.ini").c_str()));
    EXPECT_TRUE(config_get_bool("enable_feature", false));
}

TEST_F(DynamicConfigTest, GetBool_False)
{
    const char* config_content = "enable_feature = false\n";
    CreateConfig("bool_test2.ini", config_content);

    ASSERT_TRUE(config_init(GetTestPath("bool_test2.ini").c_str()));
    EXPECT_FALSE(config_get_bool("enable_feature", true));
}

TEST_F(DynamicConfigTest, GetBool_NotFound)
{
    const char* config_content = "enable_feature = true\n";
    CreateConfig("bool_test3.ini", config_content);

    ASSERT_TRUE(config_init(GetTestPath("bool_test3.ini").c_str()));
    EXPECT_TRUE(config_get_bool("nonexistent", true));
    EXPECT_FALSE(config_get_bool("nonexistent", false));
}

TEST_F(DynamicConfigTest, GetBool_NullKey)
{
    const char* config_content = "enable_feature = true\n";
    CreateConfig("bool_test4.ini", config_content);

    ASSERT_TRUE(config_init(GetTestPath("bool_test4.ini").c_str()));
    EXPECT_TRUE(config_get_bool(nullptr, true));
}

TEST_F(DynamicConfigTest, GetString_Valid)
{
    const char* config_content = "model_path = /tmp/model_file\n";
    CreateConfig("string_test.ini", config_content);

    ASSERT_TRUE(config_init(GetTestPath("string_test.ini").c_str()));
    EXPECT_STREQ(config_get_string("model_path", "default"), "/tmp/model_file");
}

TEST_F(DynamicConfigTest, GetString_NotFound)
{
    const char* config_content = "model_path = /tmp/model.bin\n";
    CreateConfig("string_test2.ini", config_content);

    ASSERT_TRUE(config_init(GetTestPath("string_test2.ini").c_str()));
    EXPECT_STREQ(config_get_string("nonexistent", "default"), "default");
}

TEST_F(DynamicConfigTest, GetString_NullKey)
{
    const char* config_content = "model_path = /tmp/model.bin\n";
    CreateConfig("string_test3.ini", config_content);

    ASSERT_TRUE(config_init(GetTestPath("string_test3.ini").c_str()));
    EXPECT_STREQ(config_get_string(nullptr, "default"), "default");
}

TEST_F(DynamicConfigTest, SetInt_Valid)
{
    const char* config_content = "epochs = 100\n";
    CreateConfig("set_int.ini", config_content);

    ASSERT_TRUE(config_init(GetTestPath("set_int.ini").c_str()));
    EXPECT_EQ(config_get_int("epochs", 0), 100);

    EXPECT_TRUE(config_set_int("epochs", 200));
    EXPECT_EQ(config_get_int("epochs", 0), 200);
}

TEST_F(DynamicConfigTest, SetInt_TypeMismatch)
{
    const char* config_content = "learning_rate = 0.001\n";
    CreateConfig("set_int2.ini", config_content);

    ASSERT_TRUE(config_init(GetTestPath("set_int2.ini").c_str()));
    EXPECT_FALSE(config_set_int("learning_rate", 100));
}

TEST_F(DynamicConfigTest, SetInt_NotFound)
{
    const char* config_content = "epochs = 100\n";
    CreateConfig("set_int3.ini", config_content);

    ASSERT_TRUE(config_init(GetTestPath("set_int3.ini").c_str()));
    EXPECT_FALSE(config_set_int("nonexistent", 200));
}

TEST_F(DynamicConfigTest, SetInt_NullKey)
{
    const char* config_content = "epochs = 100\n";
    CreateConfig("set_int4.ini", config_content);

    ASSERT_TRUE(config_init(GetTestPath("set_int4.ini").c_str()));
    EXPECT_FALSE(config_set_int(nullptr, 200));
}

TEST_F(DynamicConfigTest, SetFloat_Valid)
{
    const char* config_content = "learning_rate = 0.001\n";
    CreateConfig("set_float.ini", config_content);

    ASSERT_TRUE(config_init(GetTestPath("set_float.ini").c_str()));
    EXPECT_TRUE(config_set_float("learning_rate", 0.002));
    EXPECT_DOUBLE_EQ(config_get_float("learning_rate", 0.0), 0.002);
}

TEST_F(DynamicConfigTest, SetFloat_TypeMismatch)
{
    const char* config_content = "epochs = 100\n";
    CreateConfig("set_float2.ini", config_content);

    ASSERT_TRUE(config_init(GetTestPath("set_float2.ini").c_str()));
    EXPECT_FALSE(config_set_float("epochs", 0.5));
}

TEST_F(DynamicConfigTest, SetBool_Valid)
{
    const char* config_content = "enable_cache = true\n";
    CreateConfig("set_bool.ini", config_content);

    ASSERT_TRUE(config_init(GetTestPath("set_bool.ini").c_str()));
    EXPECT_TRUE(config_set_bool("enable_cache", false));
    EXPECT_FALSE(config_get_bool("enable_cache", true));
}

TEST_F(DynamicConfigTest, SetString_Valid)
{
    const char* config_content = "model_path = old_model\n";
    CreateConfig("set_string.ini", config_content);

    ASSERT_TRUE(config_init(GetTestPath("set_string.ini").c_str()));
    EXPECT_TRUE(config_set_string("model_path", "new_model"));
    EXPECT_STREQ(config_get_string("model_path", ""), "new_model");
}

TEST_F(DynamicConfigTest, SetString_NullValue)
{
    const char* config_content = "model_path = /tmp/old.bin\n";
    CreateConfig("set_string2.ini", config_content);

    ASSERT_TRUE(config_init(GetTestPath("set_string2.ini").c_str()));
    EXPECT_FALSE(config_set_string("model_path", nullptr));
}

TEST_F(DynamicConfigTest, Reload_Valid)
{
    const char* config_v1 = "learning_rate = 0.001\n";
    CreateConfig("reload.ini", config_v1);
    std::string path = GetTestPath("reload.ini");

    ASSERT_TRUE(config_init(path.c_str()));
    EXPECT_DOUBLE_EQ(config_get_float("learning_rate", 0.0), 0.001);

    // Modify config file
    const char* config_v2 = "learning_rate = 0.002\n";
    CreateConfig("reload.ini", config_v2);

    EXPECT_TRUE(config_reload());
    EXPECT_DOUBLE_EQ(config_get_float("learning_rate", 0.0), 0.002);
}

TEST_F(DynamicConfigTest, Reload_InvalidFile)
{
    const char* config_v1 = "learning_rate = 0.001\n";
    CreateConfig("reload2.ini", config_v1);
    std::string path = GetTestPath("reload2.ini");

    ASSERT_TRUE(config_init(path.c_str()));

    // Remove config file
    remove(path.c_str());

    EXPECT_FALSE(config_reload());

    // Old value should be retained
    EXPECT_DOUBLE_EQ(config_get_float("learning_rate", 0.0), 0.001);
}

TEST_F(DynamicConfigTest, GetStats)
{
    const char* config_content = "test = 1\n";
    CreateConfig("stats_fresh.ini", config_content);

    ASSERT_TRUE(config_init(GetTestPath("stats_fresh.ini").c_str()));

    config_stats_t stats = config_get_stats();
    EXPECT_EQ(stats.config_version, 1);
    // Note: Previous tests may have incremented counts due to teardown

    uint64_t initial_reload_count = stats.reload_count;

    // Reload once
    EXPECT_TRUE(config_reload());
    stats = config_get_stats();
    EXPECT_EQ(stats.reload_count, initial_reload_count + 1);
    // Version should increment
    EXPECT_GT(stats.config_version, 1);
}

TEST_F(DynamicConfigTest, Dump_Valid)
{
    const char* config_content = R"(
learning_rate = 0.001
batch_size = 32
enable_cache = true
model_path = /tmp/model.bin
)";
    CreateConfig("dump_test.ini", config_content);
    ASSERT_TRUE(config_init(GetTestPath("dump_test.ini").c_str()));

    std::string dump_path = GetTestPath("dumped.ini");
    EXPECT_TRUE(config_dump(dump_path.c_str()));

    // Verify file exists
    struct stat buffer;
    EXPECT_EQ(stat(dump_path.c_str(), &buffer), 0);
}

TEST_F(DynamicConfigTest, Dump_NullPath)
{
    const char* config_content = "test = 1\n";
    CreateConfig("dump_test2.ini", config_content);
    ASSERT_TRUE(config_init(GetTestPath("dump_test2.ini").c_str()));

    EXPECT_FALSE(config_dump(nullptr));
}

TEST_F(DynamicConfigTest, Validate_Valid)
{
    const char* config_content = R"(
learning_rate = 0.001
batch_size = 32
enable_cache = true
)";
    CreateConfig("validate.ini", config_content);
    EXPECT_TRUE(config_validate(GetTestPath("validate.ini").c_str()));
}

TEST_F(DynamicConfigTest, Validate_MissingEquals)
{
    const char* config_content = R"(
learning_rate = 0.001
batch_size 32
enable_cache = true
)";
    CreateConfig("validate_bad.ini", config_content);
    EXPECT_FALSE(config_validate(GetTestPath("validate_bad.ini").c_str()));
}

TEST_F(DynamicConfigTest, Validate_EmptyKey)
{
    const char* config_content = R"(
learning_rate = 0.001
 = value
)";
    CreateConfig("validate_bad2.ini", config_content);
    EXPECT_FALSE(config_validate(GetTestPath("validate_bad2.ini").c_str()));
}

TEST_F(DynamicConfigTest, Validate_NullPath)
{
    EXPECT_FALSE(config_validate(nullptr));
}

TEST_F(DynamicConfigTest, Validate_FileNotFound)
{
    EXPECT_FALSE(config_validate("/nonexistent/config.ini"));
}

TEST_F(DynamicConfigTest, CommentsAndEmptyLines)
{
    const char* config_content = R"(
# This is a comment
learning_rate = 0.001

; Another comment style
batch_size = 32

# Empty lines above should be ignored
)";
    CreateConfig("comments.ini", config_content);
    ASSERT_TRUE(config_init(GetTestPath("comments.ini").c_str()));

    EXPECT_DOUBLE_EQ(config_get_float("learning_rate", 0.0), 0.001);
    EXPECT_EQ(config_get_int("batch_size", 0), 32);
}

TEST_F(DynamicConfigTest, TypeDetection_Int)
{
    const char* config_content = "value = 42\n";
    CreateConfig("type_int.ini", config_content);
    ASSERT_TRUE(config_init(GetTestPath("type_int.ini").c_str()));

    EXPECT_EQ(config_get_int("value", 0), 42);
}

TEST_F(DynamicConfigTest, TypeDetection_NegativeInt)
{
    const char* config_content = "value = -42\n";
    CreateConfig("type_neg_int.ini", config_content);
    ASSERT_TRUE(config_init(GetTestPath("type_neg_int.ini").c_str()));

    EXPECT_EQ(config_get_int("value", 0), -42);
}

TEST_F(DynamicConfigTest, TypeDetection_Float)
{
    const char* config_content = "value = 3.14\n";
    CreateConfig("type_float.ini", config_content);
    ASSERT_TRUE(config_init(GetTestPath("type_float.ini").c_str()));

    EXPECT_DOUBLE_EQ(config_get_float("value", 0.0), 3.14);
}

TEST_F(DynamicConfigTest, TypeDetection_BoolTrue)
{
    const char* config_content = "value = true\n";
    CreateConfig("type_bool_true.ini", config_content);
    ASSERT_TRUE(config_init(GetTestPath("type_bool_true.ini").c_str()));

    EXPECT_TRUE(config_get_bool("value", false));
}

TEST_F(DynamicConfigTest, TypeDetection_BoolFalse)
{
    const char* config_content = "value = false\n";
    CreateConfig("type_bool_false.ini", config_content);
    ASSERT_TRUE(config_init(GetTestPath("type_bool_false.ini").c_str()));

    EXPECT_FALSE(config_get_bool("value", true));
}

TEST_F(DynamicConfigTest, TypeDetection_String)
{
    const char* config_content = "value = hello_world\n";
    CreateConfig("type_string.ini", config_content);
    ASSERT_TRUE(config_init(GetTestPath("type_string.ini").c_str()));

    EXPECT_STREQ(config_get_string("value", ""), "hello_world");
}

TEST_F(DynamicConfigTest, ConfigValueReplacement)
{
    const char* config_content = R"(
learning_rate = 0.001
learning_rate = 0.002
)";
    CreateConfig("replace.ini", config_content);
    ASSERT_TRUE(config_init(GetTestPath("replace.ini").c_str()));

    // Should use last value
    EXPECT_DOUBLE_EQ(config_get_float("learning_rate", 0.0), 0.002);
}

TEST_F(DynamicConfigTest, Print_NoThrow)
{
    const char* config_content = "test = 1\n";
    CreateConfig("print.ini", config_content);
    ASSERT_TRUE(config_init(GetTestPath("print.ini").c_str()));

    // Should not crash
    EXPECT_NO_THROW(config_print());
}

TEST_F(DynamicConfigTest, Shutdown_NoThrow)
{
    const char* config_content = "test = 1\n";
    CreateConfig("shutdown.ini", config_content);
    ASSERT_TRUE(config_init(GetTestPath("shutdown.ini").c_str()));

    // Should not crash
    EXPECT_NO_THROW(config_shutdown());
}

TEST_F(DynamicConfigTest, MultipleShutdowns)
{
    const char* config_content = "test = 1\n";
    CreateConfig("multi_shutdown.ini", config_content);
    ASSERT_TRUE(config_init(GetTestPath("multi_shutdown.ini").c_str()));

    // Multiple shutdowns should be safe
    EXPECT_NO_THROW(config_shutdown());
    EXPECT_NO_THROW(config_shutdown());
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
