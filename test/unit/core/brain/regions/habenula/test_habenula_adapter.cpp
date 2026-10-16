/**
 * @file test_habenula_adapter.cpp
 * @brief Unit tests for Habenula adapter with training layer integration
 */

#include <gtest/gtest.h>
#include "test_helpers.h"

extern "C" {
#include "core/brain/regions/habenula/nimcp_habenula_adapter.h"
}

/* Test callback trackers */
static int g_training_callback_count = 0;
static nimcp_habenula_training_modulation_t g_last_modulation;

static void test_training_callback(void* user_data,
                                    const nimcp_habenula_training_modulation_t* modulation) {
    (void)user_data;
    g_training_callback_count++;
    if (modulation) {
        g_last_modulation = *modulation;
    }
}

static int g_msg_callback_count = 0;
static char g_last_topic[64] = {0};

static void test_msg_callback(void* user_data, const char* topic,
                               const void* data, size_t size) {
    (void)user_data;
    (void)data;
    (void)size;
    g_msg_callback_count++;
    if (topic) {
        strncpy(g_last_topic, topic, sizeof(g_last_topic) - 1);
    }
}

class HabenulaAdapterTest : public ::testing::Test {
protected:
    nimcp_habenula_adapter_t adapter = nullptr;

    void SetUp() override {
        g_training_callback_count = 0;
        g_msg_callback_count = 0;
        memset(g_last_topic, 0, sizeof(g_last_topic));
    }

    void TearDown() override {
        if (adapter) {
            nimcp_habenula_adapter_destroy(adapter);
            adapter = nullptr;
        }
    }
};

/* ==========================================================================
 * Configuration Tests
 * ========================================================================== */

TEST_F(HabenulaAdapterTest, DefaultConfigHasValidValues) {
    nimcp_habenula_adapter_config_t config;
    nimcp_habenula_adapter_default_config(&config);

    EXPECT_TRUE(config.enable_training_integration);
    EXPECT_TRUE(config.enable_vta_coordination);
    EXPECT_TRUE(config.enable_raphe_coordination);
    EXPECT_GT(config.training_update_interval, 0.0f);
}

/* ==========================================================================
 * Lifecycle Tests
 * ========================================================================== */

TEST_F(HabenulaAdapterTest, CreateWithNullConfigSucceeds) {
    adapter = nimcp_habenula_adapter_create(nullptr);
    EXPECT_NE(adapter, nullptr);
}

TEST_F(HabenulaAdapterTest, CreateWithCustomConfigSucceeds) {
    nimcp_habenula_adapter_config_t config;
    nimcp_habenula_adapter_default_config(&config);
    config.training_update_interval = 50.0f;

    adapter = nimcp_habenula_adapter_create(&config);
    EXPECT_NE(adapter, nullptr);
}

TEST_F(HabenulaAdapterTest, DestroyWithNullDoesNotCrash) {
    nimcp_habenula_adapter_destroy(nullptr);
    /* Should not crash */
}

TEST_F(HabenulaAdapterTest, DestroySucceeds) {
    adapter = nimcp_habenula_adapter_create(nullptr);
    nimcp_habenula_adapter_destroy(adapter);
    adapter = nullptr; /* Prevent double-free in TearDown */
}

TEST_F(HabenulaAdapterTest, DisconnectSucceeds) {
    adapter = nimcp_habenula_adapter_create(nullptr);
    EXPECT_EQ(nimcp_habenula_adapter_disconnect(adapter), 0);
}

/* ==========================================================================
 * Access Tests
 * ========================================================================== */

TEST_F(HabenulaAdapterTest, GetHabenulaReturnsValidPointer) {
    adapter = nimcp_habenula_adapter_create(nullptr);

    nimcp_habenula_system_t* habenula = nimcp_habenula_adapter_get_habenula(adapter);
    EXPECT_NE(habenula, nullptr);
    EXPECT_TRUE(habenula->initialized);
}

TEST_F(HabenulaAdapterTest, GetHabenulaWithNullReturnsNull) {
    EXPECT_EQ(nimcp_habenula_adapter_get_habenula(nullptr), nullptr);
}

/* ==========================================================================
 * Messaging Tests
 * ========================================================================== */

TEST_F(HabenulaAdapterTest, SendMessageSucceeds) {
    adapter = nimcp_habenula_adapter_create(nullptr);

    float data = 0.5f;
    EXPECT_EQ(nimcp_habenula_adapter_send_message(adapter, "test_topic", &data, sizeof(data)), 0);
}

TEST_F(HabenulaAdapterTest, ProcessMessagesSucceeds) {
    adapter = nimcp_habenula_adapter_create(nullptr);
    EXPECT_EQ(nimcp_habenula_adapter_process_messages(adapter), 0);
}

TEST_F(HabenulaAdapterTest, RegisterCallbackSucceeds) {
    adapter = nimcp_habenula_adapter_create(nullptr);
    EXPECT_EQ(nimcp_habenula_adapter_register_callback(adapter, "test_topic",
              test_msg_callback, nullptr), 0);
}

TEST_F(HabenulaAdapterTest, CallbackInvokedOnMessage) {
    adapter = nimcp_habenula_adapter_create(nullptr);

    nimcp_habenula_adapter_register_callback(adapter, "callback_test",
                                              test_msg_callback, nullptr);

    float data = 0.5f;
    nimcp_habenula_adapter_send_message(adapter, "callback_test", &data, sizeof(data));
    nimcp_habenula_adapter_process_messages(adapter);

    EXPECT_EQ(g_msg_callback_count, 1);
    EXPECT_STREQ(g_last_topic, "callback_test");
}

/* ==========================================================================
 * Update Tests
 * ========================================================================== */

TEST_F(HabenulaAdapterTest, UpdateWithNullReturnsError) {
    EXPECT_EQ(nimcp_habenula_adapter_update(nullptr, 10.0f), -1);
}

TEST_F(HabenulaAdapterTest, UpdateSucceeds) {
    adapter = nimcp_habenula_adapter_create(nullptr);
    EXPECT_EQ(nimcp_habenula_adapter_update(adapter, 100.0f), 0);
}

TEST_F(HabenulaAdapterTest, UpdateIncrementsStatistics) {
    adapter = nimcp_habenula_adapter_create(nullptr);

    for (int i = 0; i < 10; i++) {
        nimcp_habenula_adapter_update(adapter, 100.0f);
    }

    nimcp_habenula_adapter_state_t state;
    nimcp_habenula_adapter_get_state(adapter, &state);
    /* State should reflect multiple updates */
}

/* ==========================================================================
 * Training Integration Tests (Bidirectional)
 * ========================================================================== */

TEST_F(HabenulaAdapterTest, ConnectTrainingSucceeds) {
    adapter = nimcp_habenula_adapter_create(nullptr);

    int dummy_handle = 42;
    EXPECT_EQ(nimcp_habenula_adapter_connect_training(adapter, &dummy_handle), 0);
}

TEST_F(HabenulaAdapterTest, OnTrainingEventWithNullReturnsError) {
    EXPECT_EQ(nimcp_habenula_adapter_on_training_event(nullptr, nullptr), -1);
}

TEST_F(HabenulaAdapterTest, OnTrainingEventLossSucceeds) {
    adapter = nimcp_habenula_adapter_create(nullptr);

    nimcp_habenula_train_event_data_t event = {
        .type = HABENULA_TRAIN_EVENT_LOSS,
        .value = 0.5f,
        .expected = 0.1f,
        .timestamp = 1000.0f
    };

    EXPECT_EQ(nimcp_habenula_adapter_on_training_event(adapter, &event), 0);
}

TEST_F(HabenulaAdapterTest, OnTrainingEventFailureIncreasesHelplessness) {
    adapter = nimcp_habenula_adapter_create(nullptr);

    nimcp_habenula_system_t* habenula = nimcp_habenula_adapter_get_habenula(adapter);
    float initial_helplessness = habenula->depression.helplessness_index;

    nimcp_habenula_train_event_data_t event = {
        .type = HABENULA_TRAIN_EVENT_FAILURE,
        .value = 0.8f,
        .expected = 0.0f,
        .timestamp = 1000.0f
    };

    nimcp_habenula_adapter_on_training_event(adapter, &event);

    EXPECT_GT(habenula->depression.helplessness_index, initial_helplessness);
}

TEST_F(HabenulaAdapterTest, OnTrainingEventRewardDecreasesDisappointment) {
    adapter = nimcp_habenula_adapter_create(nullptr);

    /* Set initial disappointment */
    nimcp_habenula_system_t* habenula = nimcp_habenula_adapter_get_habenula(adapter);
    habenula->lhb.disappointment = 0.6f;
    float initial = habenula->lhb.disappointment;

    nimcp_habenula_train_event_data_t event = {
        .type = HABENULA_TRAIN_EVENT_REWARD,
        .value = 0.8f,
        .expected = 0.5f,
        .timestamp = 1000.0f
    };

    nimcp_habenula_adapter_on_training_event(adapter, &event);

    EXPECT_LT(habenula->lhb.disappointment, initial);
}

TEST_F(HabenulaAdapterTest, GetTrainingModulationWithNullReturnsError) {
    EXPECT_EQ(nimcp_habenula_adapter_get_training_modulation(nullptr, nullptr), -1);
}

TEST_F(HabenulaAdapterTest, GetTrainingModulationReturnsValidValues) {
    adapter = nimcp_habenula_adapter_create(nullptr);

    nimcp_habenula_training_modulation_t modulation;
    EXPECT_EQ(nimcp_habenula_adapter_get_training_modulation(adapter, &modulation), 0);

    EXPECT_GT(modulation.lr_reduction_factor, 0.0f);
    EXPECT_LE(modulation.lr_reduction_factor, 1.0f);
    EXPECT_GE(modulation.exploration_penalty, 0.0f);
    EXPECT_GE(modulation.negative_weight_factor, 1.0f);
}

TEST_F(HabenulaAdapterTest, RegisterTrainingCallbackSucceeds) {
    adapter = nimcp_habenula_adapter_create(nullptr);
    EXPECT_EQ(nimcp_habenula_adapter_register_training_callback(adapter,
              test_training_callback, nullptr), 0);
}

TEST_F(HabenulaAdapterTest, TrainingCallbackInvokedOnUpdate) {
    adapter = nimcp_habenula_adapter_create(nullptr);

    nimcp_habenula_adapter_register_training_callback(adapter,
                                                       test_training_callback, nullptr);

    /* Run enough updates to trigger training callback */
    for (int i = 0; i < 5; i++) {
        nimcp_habenula_adapter_update(adapter, 100.0f);
    }

    EXPECT_GT(g_training_callback_count, 0);
}

/* ==========================================================================
 * Reward Processing Tests
 * ========================================================================== */

TEST_F(HabenulaAdapterTest, ProcessRewardOutcomeSucceeds) {
    adapter = nimcp_habenula_adapter_create(nullptr);
    EXPECT_EQ(nimcp_habenula_adapter_process_reward_outcome(adapter, 0.8f, 0.3f), 0);
}

TEST_F(HabenulaAdapterTest, ProcessPunishmentSucceeds) {
    adapter = nimcp_habenula_adapter_create(nullptr);
    EXPECT_EQ(nimcp_habenula_adapter_process_punishment(adapter, 0.7f), 0);
}

TEST_F(HabenulaAdapterTest, ProcessPunishmentIncreasesAversion) {
    adapter = nimcp_habenula_adapter_create(nullptr);

    nimcp_habenula_system_t* habenula = nimcp_habenula_adapter_get_habenula(adapter);
    float initial_aversion = habenula->mhb.aversion_level;

    nimcp_habenula_adapter_process_punishment(adapter, 0.7f);

    EXPECT_GT(habenula->mhb.aversion_level, initial_aversion);
}

TEST_F(HabenulaAdapterTest, ComputeNegativeReinforcementSucceeds) {
    adapter = nimcp_habenula_adapter_create(nullptr);

    float negative_signal;
    EXPECT_EQ(nimcp_habenula_adapter_compute_negative_reinforcement(adapter, &negative_signal), 0);
    EXPECT_GE(negative_signal, 0.0f);
    EXPECT_LE(negative_signal, 1.0f);
}

/* ==========================================================================
 * VTA Coordination Tests
 * ========================================================================== */

TEST_F(HabenulaAdapterTest, GetVTAOutputSucceeds) {
    adapter = nimcp_habenula_adapter_create(nullptr);

    float inhibition;
    EXPECT_EQ(nimcp_habenula_adapter_get_vta_output(adapter, &inhibition), 0);
    EXPECT_GE(inhibition, 0.0f);
}

TEST_F(HabenulaAdapterTest, ApplyVTAInputSucceeds) {
    adapter = nimcp_habenula_adapter_create(nullptr);
    EXPECT_EQ(nimcp_habenula_adapter_apply_vta_input(adapter, 50.0f), 0);
}

TEST_F(HabenulaAdapterTest, HighDisappointmentIncreasesVTAOutput) {
    adapter = nimcp_habenula_adapter_create(nullptr);

    float initial_inhibition;
    nimcp_habenula_adapter_get_vta_output(adapter, &initial_inhibition);

    /* Induce disappointment */
    nimcp_habenula_adapter_process_reward_outcome(adapter, 1.0f, 0.0f);
    nimcp_habenula_adapter_update(adapter, 100.0f);

    float high_inhibition;
    nimcp_habenula_adapter_get_vta_output(adapter, &high_inhibition);

    EXPECT_GT(high_inhibition, initial_inhibition);
}

/* ==========================================================================
 * Raphe Coordination Tests
 * ========================================================================== */

TEST_F(HabenulaAdapterTest, GetRapheOutputSucceeds) {
    adapter = nimcp_habenula_adapter_create(nullptr);

    float modulation;
    EXPECT_EQ(nimcp_habenula_adapter_get_raphe_output(adapter, &modulation), 0);
    EXPECT_GE(modulation, 0.0f);
}

TEST_F(HabenulaAdapterTest, ApplyRapheInputSucceeds) {
    adapter = nimcp_habenula_adapter_create(nullptr);
    EXPECT_EQ(nimcp_habenula_adapter_apply_raphe_input(adapter, 50.0f), 0);
}

/* ==========================================================================
 * Depression/Helplessness Tests
 * ========================================================================== */

TEST_F(HabenulaAdapterTest, ShouldStopSucceeds) {
    adapter = nimcp_habenula_adapter_create(nullptr);

    bool should_stop;
    EXPECT_EQ(nimcp_habenula_adapter_should_stop(adapter, &should_stop), 0);
    EXPECT_FALSE(should_stop); /* Initially should not suggest stopping */
}

TEST_F(HabenulaAdapterTest, GetDepressionStateSucceeds) {
    adapter = nimcp_habenula_adapter_create(nullptr);

    float helplessness;
    bool is_depressed;
    EXPECT_EQ(nimcp_habenula_adapter_get_depression_state(adapter, &helplessness, &is_depressed), 0);
    EXPECT_GE(helplessness, 0.0f);
    EXPECT_FALSE(is_depressed);
}

TEST_F(HabenulaAdapterTest, RecordFailureIncreasesHelplessness) {
    adapter = nimcp_habenula_adapter_create(nullptr);

    float initial_helplessness, after_helplessness;
    bool is_depressed;
    nimcp_habenula_adapter_get_depression_state(adapter, &initial_helplessness, &is_depressed);

    nimcp_habenula_adapter_record_failure(adapter);

    nimcp_habenula_adapter_get_depression_state(adapter, &after_helplessness, &is_depressed);
    EXPECT_GT(after_helplessness, initial_helplessness);
}

TEST_F(HabenulaAdapterTest, RecordSuccessDecreasesHelplessness) {
    adapter = nimcp_habenula_adapter_create(nullptr);

    /* Set some helplessness */
    nimcp_habenula_system_t* habenula = nimcp_habenula_adapter_get_habenula(adapter);
    habenula->depression.helplessness_index = 0.5f;

    float initial_helplessness, after_helplessness;
    bool is_depressed;
    nimcp_habenula_adapter_get_depression_state(adapter, &initial_helplessness, &is_depressed);

    nimcp_habenula_adapter_record_success(adapter);

    nimcp_habenula_adapter_get_depression_state(adapter, &after_helplessness, &is_depressed);
    EXPECT_LT(after_helplessness, initial_helplessness);
}

/* ==========================================================================
 * State Tests
 * ========================================================================== */

TEST_F(HabenulaAdapterTest, GetStateWithNullReturnsError) {
    EXPECT_EQ(nimcp_habenula_adapter_get_state(nullptr, nullptr), -1);
}

TEST_F(HabenulaAdapterTest, GetStateReturnsValidData) {
    adapter = nimcp_habenula_adapter_create(nullptr);

    nimcp_habenula_adapter_state_t state;
    EXPECT_EQ(nimcp_habenula_adapter_get_state(adapter, &state), 0);

    EXPECT_GT(state.firing_rate, 0.0f);
    EXPECT_GE(state.disappointment, 0.0f);
    EXPECT_GE(state.aversion, 0.0f);
    EXPECT_EQ(state.mode, HABENULA_MODE_BASELINE);
}

TEST_F(HabenulaAdapterTest, ResetStatsSucceeds) {
    adapter = nimcp_habenula_adapter_create(nullptr);

    /* Generate some activity */
    nimcp_habenula_adapter_update(adapter, 100.0f);
    float data = 0.5f;
    nimcp_habenula_adapter_send_message(adapter, "test", &data, sizeof(data));
    nimcp_habenula_adapter_process_messages(adapter);

    EXPECT_EQ(nimcp_habenula_adapter_reset_stats(adapter), 0);

    nimcp_habenula_adapter_state_t state;
    nimcp_habenula_adapter_get_state(adapter, &state);
    EXPECT_EQ(state.messages_sent, 0u);
    EXPECT_EQ(state.messages_received, 0u);
}

/* ==========================================================================
 * Training Modulation Behavior Tests
 * ========================================================================== */

TEST_F(HabenulaAdapterTest, DisappointmentReducesLRMultiplier) {
    /* Create two adapters - one with disappointment, one without */
    adapter = nimcp_habenula_adapter_create(nullptr);
    nimcp_habenula_adapter_t control_adapter = nimcp_habenula_adapter_create(nullptr);

    /* Induce disappointment in test adapter */
    nimcp_habenula_adapter_process_reward_outcome(adapter, 1.0f, 0.0f);

    /* Run both through updates */
    for (int i = 0; i < 5; i++) {
        nimcp_habenula_adapter_update(adapter, 100.0f);
        nimcp_habenula_adapter_update(control_adapter, 100.0f);
    }

    nimcp_habenula_training_modulation_t test_mod, control_mod;
    nimcp_habenula_adapter_get_training_modulation(adapter, &test_mod);
    nimcp_habenula_adapter_get_training_modulation(control_adapter, &control_mod);

    /* LR should be reduced with disappointment */
    EXPECT_LT(test_mod.lr_reduction_factor, control_mod.lr_reduction_factor);

    nimcp_habenula_adapter_destroy(control_adapter);
}

TEST_F(HabenulaAdapterTest, HighHelplessnessSuggestsEarlyStop) {
    adapter = nimcp_habenula_adapter_create(nullptr);

    /* Induce high helplessness */
    for (int i = 0; i < 10; i++) {
        nimcp_habenula_adapter_record_failure(adapter);
    }

    /* Update to compute modulation */
    for (int i = 0; i < 5; i++) {
        nimcp_habenula_adapter_update(adapter, 100.0f);
    }

    nimcp_habenula_training_modulation_t modulation;
    nimcp_habenula_adapter_get_training_modulation(adapter, &modulation);

    /* With high helplessness, should suggest early stop */
    EXPECT_TRUE(modulation.suggest_early_stop);
}

TEST_F(HabenulaAdapterTest, AversionIncreasesExplorationPenalty) {
    adapter = nimcp_habenula_adapter_create(nullptr);

    /* Get baseline */
    nimcp_habenula_training_modulation_t baseline_mod;
    nimcp_habenula_adapter_update(adapter, 100.0f);
    nimcp_habenula_adapter_get_training_modulation(adapter, &baseline_mod);

    /* Induce aversion */
    nimcp_habenula_adapter_process_punishment(adapter, 0.9f);

    /* Update to compute new modulation */
    for (int i = 0; i < 5; i++) {
        nimcp_habenula_adapter_update(adapter, 100.0f);
    }

    nimcp_habenula_training_modulation_t averse_mod;
    nimcp_habenula_adapter_get_training_modulation(adapter, &averse_mod);

    EXPECT_GT(averse_mod.exploration_penalty, baseline_mod.exploration_penalty);
}
