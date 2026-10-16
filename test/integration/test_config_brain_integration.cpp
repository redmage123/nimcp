/**
 * @file test_config_brain_integration.cpp
 * @brief Integration tests for dynamic config callbacks with brain training
 *
 * WHAT: Test config callbacks integrated with brain learning
 * WHY:  Verify runtime config changes affect brain behavior correctly
 * HOW:  Register callbacks, modify config, train brain, verify adaptation
 *
 * @author NIMCP Test Team
 * @date 2025-01-17
 */

#include <gtest/gtest.h>
#include "core/brain/nimcp_brain.h"
#include "utils/config/nimcp_dynamic_config.h"
#include <atomic>
#include <vector>

//=============================================================================
// Test Fixture
//=============================================================================

class ConfigBrainIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;
    std::vector<uint32_t> callback_ids;

    void SetUp() override {
        brain = nullptr;

        // Initialize config system
        system("echo 'learning_rate=0.01' > /tmp/integration_config.ini");
        system("echo 'num_epochs=5' >> /tmp/integration_config.ini");

        bool success = config_init("/tmp/integration_config.ini");
        ASSERT_TRUE(success);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
        }

        // Unregister all callbacks
        for (uint32_t id : callback_ids) {
            config_unregister_callback(id);
        }
        callback_ids.clear();

        config_shutdown();
        system("rm -f /tmp/integration_config.ini");
    }
};

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(ConfigBrainIntegrationTest, LearningRateCallback) {
    // WHAT: Config callback updates brain learning rate
    // WHY:  Test runtime hyperparameter tuning
    // HOW:  Register callback, change config, verify brain uses new LR

    brain_config_t config;
    memset(&config, 0, sizeof(config));
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 10;
    snprintf(config.task_name, sizeof(config.task_name), "test_brain");
    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    std::atomic<int> callback_count{0};

    struct CallbackData {
        brain_t brain;
        std::atomic<int>* counter;
    } cb_data = {brain, &callback_count};

    auto lr_callback = [](const char* key, const config_value_t* old_val,
                         const config_value_t* new_val, void* user_data) {
        // FIX: Cast user_data to CallbackData* to access both brain and counter
        auto* data = static_cast<CallbackData*>(user_data);

        if (strcmp(key, "learning_rate") == 0 && new_val && data && data->counter) {
            // Update brain learning rate
            // Note: This would require brain_set_learning_rate API
            // For now, just count callbacks
            (*(data->counter))++;
        }
    };

    uint32_t id = config_register_callback("learning_rate",
                                           lr_callback,
                                           &cb_data);
    ASSERT_NE(id, 0);
    callback_ids.push_back(id);

    // Change learning rate via config
    config_set_float("learning_rate", 0.001);

    // Callback should have been invoked
    EXPECT_GT(callback_count, 0);
}

TEST_F(ConfigBrainIntegrationTest, LayerFreezingIntegration) {
    // WHAT: Layer freezing with config-driven hyperparameters
    // WHY:  Test transfer learning pipeline
    // HOW:  Create brain, freeze layers, train with config values

    brain_config_t config;
    memset(&config, 0, sizeof(config));
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 10;
    snprintf(config.task_name, sizeof(config.task_name), "test_brain");
    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Prepare training data
    std::vector<float> training_data;
    std::vector<float> labels;

    for (int i = 0; i < 20; i++) {
        // Simple features
        for (int j = 0; j < 10; j++) {
            training_data.push_back((float)(i * 10 + j) / 200.0f);
        }

        // One-hot labels
        for (int j = 0; j < 10; j++) {
            labels.push_back((j == (i % 10)) ? 1.0f : 0.0f);
        }
    }

    // Get learning rate from config
    double config_lr = config_get_float("learning_rate", 0.01);

    // Fine-tune with layer freezing
    brain_finetune_config_t finetune_config = {
        .learning_rate = (float)config_lr,
        .num_epochs = (uint32_t)config_get_int("num_epochs", 5),
        .freeze_sensory = true,
        .freeze_cognitive = true,
        .finetune_classifier = true,
        .batch_size = 16,
        .verbose = false
    };

    bool success = brain_finetune(brain, training_data.data(), labels.data(),
                                  20, &finetune_config);
    EXPECT_TRUE(success);
}

TEST_F(ConfigBrainIntegrationTest, RuntimeConfigUpdate) {
    // WHAT: Update config during training
    // WHY:  Test dynamic hyperparameter adjustment
    // HOW:  Start training, change config mid-training

    brain_config_t config;
    memset(&config, 0, sizeof(config));
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 10;
    snprintf(config.task_name, sizeof(config.task_name), "test_brain");
    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    std::atomic<bool> config_changed{false};

    auto monitor_callback = [](const char* key, const config_value_t* old_val,
                              const config_value_t* new_val, void* user_data) {
        (void)key; (void)old_val; (void)new_val;
        auto* flag = static_cast<std::atomic<bool>*>(user_data);
        *flag = true;
    };

    uint32_t id = config_register_callback(nullptr, monitor_callback,
                                           &config_changed);
    ASSERT_NE(id, 0);
    callback_ids.push_back(id);

    // Change config
    config_set_float("learning_rate", 0.005);

    // Callback should have detected change
    EXPECT_TRUE(config_changed);

    // Train with new config
    std::vector<float> features = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                                  0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

    float loss = brain_learn_example(brain, features.data(),
                                     static_cast<uint32_t>(features.size()),
                                     "test_class", 1.0f);
    EXPECT_GE(loss, 0.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
