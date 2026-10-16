/**
 * @file test_nimcp_speech_motor.cpp
 * @brief Unit tests for nimcp_speech_motor.c
 *
 * WHAT: Comprehensive unit tests for the speech motor planning module
 * WHY:  Ensure correct phoneme-to-articulator mapping, motor command generation,
 *       and coarticulation handling.
 * HOW:  Use Google Test framework to test lifecycle, phoneme planning,
 *       command queue management, articulator state, and coarticulation.
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h> // For memset
#include <stdlib.h> // For NULL
#include <vector>   // For std::vector
#include <cmath>    // For fabs

#include "core/brain/regions/broca/nimcp_speech_motor.h"

// Test Fixture for Speech Motor Planner
class SpeechMotorTest : public ::testing::Test {
protected:
    speech_motor_planner_t* planner;
    speech_motor_config_t config;

    void SetUp() override {
        config = speech_motor_default_config();
        config.max_commands = 20; // Small queue for testing limits
        planner = speech_motor_create(&config);
        ASSERT_NE(nullptr, planner) << "Failed to create speech motor planner";
    }

    void TearDown() override {
        speech_motor_destroy(planner);
        planner = nullptr;
    }

    void ExpectFloatNear(float actual, float expected, float epsilon = 1e-5f, const char* msg = "") {
        EXPECT_NEAR(actual, expected, epsilon) << msg;
    }

    // Helper to get phoneme features (replicates internal logic for testing)
    struct PhonemeFeatures {
        float lips_position;
        float tongue_position;
        float jaw_height;
        float larynx_tension;
        float velum_opening;
        float duration_ms;
    };

    PhonemeFeatures get_expected_phoneme_features(uint8_t phoneme) {
        static std::map<uint8_t, PhonemeFeatures> phoneme_map;

        if (phoneme_map.empty()) {
            // Populate the map once - matches expanded phoneme table in nimcp_speech_motor.c
            phoneme_map[0] = {0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 50.0f}; // Default/silence
            phoneme_map['a'] = {0.8f, 0.6f, 0.9f, 0.8f, 0.0f, 85.0f};  // /a/ open front
            phoneme_map['e'] = {0.7f, 0.2f, 0.5f, 0.8f, 0.0f, 75.0f};  // /e/ close-mid front
            phoneme_map['i'] = {0.7f, 0.1f, 0.3f, 0.8f, 0.0f, 70.0f};  // /i/ close front
            phoneme_map['o'] = {0.4f, 0.8f, 0.5f, 0.8f, 0.0f, 75.0f};  // /o/ close-mid back
            phoneme_map['u'] = {0.2f, 0.9f, 0.3f, 0.8f, 0.0f, 70.0f};  // /u/ close back
            phoneme_map['p'] = {0.0f, 0.5f, 0.3f, 0.0f, 0.0f, 60.0f};  // /p/ voiceless bilabial
            phoneme_map['b'] = {0.0f, 0.5f, 0.3f, 0.9f, 0.0f, 65.0f};  // /b/ voiced bilabial
            phoneme_map['t'] = {0.5f, 0.2f, 0.3f, 0.0f, 0.0f, 55.0f};  // /t/ voiceless alveolar
            phoneme_map['d'] = {0.5f, 0.2f, 0.3f, 0.9f, 0.0f, 60.0f};  // /d/ voiced alveolar
            phoneme_map['k'] = {0.5f, 0.8f, 0.4f, 0.0f, 0.0f, 65.0f};  // /k/ voiceless velar
            phoneme_map['g'] = {0.5f, 0.8f, 0.4f, 0.9f, 0.0f, 70.0f};  // /g/ voiced velar
            phoneme_map['f'] = {0.1f, 0.5f, 0.4f, 0.0f, 0.0f, 90.0f};  // /f/ voiceless labiodental
            phoneme_map['v'] = {0.1f, 0.5f, 0.4f, 0.9f, 0.0f, 85.0f};  // /v/ voiced labiodental
            phoneme_map['s'] = {0.5f, 0.2f, 0.4f, 0.0f, 0.0f, 100.0f}; // /s/ voiceless alveolar
            phoneme_map['z'] = {0.5f, 0.2f, 0.4f, 0.9f, 0.0f, 95.0f};  // /z/ voiced alveolar
            phoneme_map['h'] = {0.7f, 0.5f, 0.6f, 0.0f, 0.0f, 80.0f};  // /h/ voiceless glottal
            phoneme_map['m'] = {0.0f, 0.5f, 0.4f, 0.9f, 1.0f, 75.0f};  // /m/ bilabial nasal
            phoneme_map['n'] = {0.5f, 0.2f, 0.4f, 0.9f, 1.0f, 70.0f};  // /n/ alveolar nasal
            phoneme_map['l'] = {0.6f, 0.2f, 0.5f, 0.9f, 0.0f, 65.0f};  // /l/ alveolar lateral
            phoneme_map['r'] = {0.6f, 0.35f, 0.5f, 0.9f, 0.0f, 70.0f}; // /r/ alveolar approximant
            phoneme_map['w'] = {0.2f, 0.9f, 0.4f, 0.9f, 0.0f, 60.0f};  // /w/ labial-velar
            phoneme_map['j'] = {0.7f, 0.1f, 0.4f, 0.9f, 0.0f, 55.0f};  // /j/ palatal approximant
        }
        return phoneme_map[phoneme];
    }
};

// =============================================================================
// Lifecycle and Configuration Tests
// =============================================================================

TEST_F(SpeechMotorTest, CreateAndDestroy) {
    // Planner created and destroyed in SetUp/TearDown
    ASSERT_NE(nullptr, planner);
    SUCCEED();
}

TEST_F(SpeechMotorTest, CreateWithNullConfig) {
    speech_motor_planner_t* null_config_planner = speech_motor_create(nullptr);
    ASSERT_NE(nullptr, null_config_planner) << "Should create with default config";
    speech_motor_destroy(null_config_planner);
}

TEST_F(SpeechMotorTest, ValidateConfig) {
    speech_motor_config_t valid_config = speech_motor_default_config();
    EXPECT_TRUE(speech_motor_validate_config(&valid_config));

    speech_motor_config_t invalid_config = valid_config;
    invalid_config.max_commands = 0;
    EXPECT_FALSE(speech_motor_validate_config(&invalid_config));

    invalid_config = valid_config;
    invalid_config.max_commands = 10001;
    EXPECT_FALSE(speech_motor_validate_config(&invalid_config));

    invalid_config = valid_config;
    invalid_config.coarticulation_strength = 1.5f;
    EXPECT_FALSE(speech_motor_validate_config(&invalid_config));

    invalid_config = valid_config;
    invalid_config.default_velocity = 101.0f;
    EXPECT_FALSE(speech_motor_validate_config(&invalid_config));

    EXPECT_FALSE(speech_motor_validate_config(nullptr));
}

// =============================================================================
// Phoneme Planning Tests
// =============================================================================

TEST_F(SpeechMotorTest, PlanSinglePhoneme) {
    EXPECT_TRUE(speech_motor_plan_phoneme(planner, 'a'));

    motor_command_t commands[SPEECH_MOTOR_NUM_ARTICULATORS];
    uint32_t count = SPEECH_MOTOR_NUM_ARTICULATORS;
    EXPECT_TRUE(speech_motor_get_commands(planner, commands, &count));
    EXPECT_EQ(SPEECH_MOTOR_NUM_ARTICULATORS, count);

    PhonemeFeatures expected_features = get_expected_phoneme_features('a');

    EXPECT_EQ(ARTICULATOR_LIPS, commands[0].type);
    ExpectFloatNear(expected_features.lips_position, commands[0].position);

    EXPECT_EQ(ARTICULATOR_TONGUE, commands[1].type);
    ExpectFloatNear(expected_features.tongue_position, commands[1].position);

    EXPECT_EQ(ARTICULATOR_JAW, commands[2].type);
    ExpectFloatNear(expected_features.jaw_height, commands[2].position);

    EXPECT_EQ(ARTICULATOR_LARYNX, commands[3].type);
    ExpectFloatNear(expected_features.larynx_tension, commands[3].position);

    EXPECT_EQ(ARTICULATOR_VELUM, commands[4].type);
    ExpectFloatNear(expected_features.velum_opening, commands[4].position);

    // Verify time advanced
    EXPECT_GT(commands[0].timestamp, 0.0);
}

TEST_F(SpeechMotorTest, PlanSequence) {
    uint8_t phonemes[] = {'p', 'a', 't'}; // CVC
    uint32_t num_phonemes = sizeof(phonemes) / sizeof(phonemes[0]);

    EXPECT_TRUE(speech_motor_plan_sequence(planner, phonemes, num_phonemes));

    uint32_t expected_commands = num_phonemes * SPEECH_MOTOR_NUM_ARTICULATORS;
    motor_command_t commands[expected_commands];
    uint32_t count = expected_commands;

    EXPECT_TRUE(speech_motor_get_commands(planner, commands, &count));
    EXPECT_EQ(expected_commands, count);

    // Check first and last command timestamps
    EXPECT_GT(commands[0].timestamp, 0.0);
    EXPECT_GT(commands[count - 1].timestamp, commands[0].timestamp);
}

TEST_F(SpeechMotorTest, PlanPhonemeNullInputs) {
    EXPECT_FALSE(speech_motor_plan_phoneme(nullptr, 'a'));
}

TEST_F(SpeechMotorTest, PlanSequenceNullInputs) {
    uint8_t phonemes[] = {'p'};
    EXPECT_FALSE(speech_motor_plan_sequence(nullptr, phonemes, 1));
    EXPECT_FALSE(speech_motor_plan_sequence(planner, nullptr, 1));
    EXPECT_FALSE(speech_motor_plan_sequence(planner, phonemes, 0));
}

TEST_F(SpeechMotorTest, CommandQueueLimit) {
    // Fill the queue
    for (uint32_t i = 0; i < config.max_commands / SPEECH_MOTOR_NUM_ARTICULATORS; ++i) {
        ASSERT_TRUE(speech_motor_plan_phoneme(planner, 'a')) << "Failed to plan phoneme " << i;
    }

    // Next plan should fail as queue is full
    EXPECT_FALSE(speech_motor_plan_phoneme(planner, 'b')) << "Should not plan if command queue is full";

    // Retrieve some commands
    motor_command_t commands[SPEECH_MOTOR_NUM_ARTICULATORS];
    uint32_t count = SPEECH_MOTOR_NUM_ARTICULATORS;
    EXPECT_TRUE(speech_motor_get_commands(planner, commands, &count));

    // Now it should be possible to plan again
    EXPECT_TRUE(speech_motor_plan_phoneme(planner, 'c'));
}

TEST_F(SpeechMotorTest, GetCommandsNullInputs) {
    motor_command_t commands[1];
    uint32_t count = 1;
    EXPECT_FALSE(speech_motor_get_commands(nullptr, commands, &count));
    EXPECT_FALSE(speech_motor_get_commands(planner, nullptr, &count));
    EXPECT_FALSE(speech_motor_get_commands(planner, commands, nullptr));
}

// =============================================================================
// Reset and Articulator State Tests
// =============================================================================

TEST_F(SpeechMotorTest, ResetPlanner) {
    speech_motor_plan_phoneme(planner, 'a');
    speech_motor_plan_phoneme(planner, 'p');

    // Ensure commands are in queue
    motor_command_t commands[1];
    uint32_t count = 1;
    speech_motor_get_commands(planner, commands, &count); // Get one to confirm queue is not empty

    EXPECT_TRUE(speech_motor_reset(planner));

    // Verify queue is empty
    count = 1;
    EXPECT_FALSE(speech_motor_get_commands(planner, commands, &count)); // Should return false as no commands

    // Verify articulators are reset to neutral (0.5)
    float pos;
    speech_motor_get_articulator(planner, ARTICULATOR_LIPS, &pos);
    ExpectFloatNear(0.5f, pos);
    speech_motor_get_articulator(planner, ARTICULATOR_TONGUE, &pos);
    ExpectFloatNear(0.5f, pos);
}

TEST_F(SpeechMotorTest, ResetNullPlanner) {
    EXPECT_FALSE(speech_motor_reset(nullptr));
}

TEST_F(SpeechMotorTest, SetGetArticulator) {
    float initial_pos;
    speech_motor_get_articulator(planner, ARTICULATOR_LIPS, &initial_pos);
    ExpectFloatNear(0.5f, initial_pos);

    EXPECT_TRUE(speech_motor_set_articulator(planner, ARTICULATOR_LIPS, 0.9f));
    float new_pos;
    speech_motor_get_articulator(planner, ARTICULATOR_LIPS, &new_pos);
    ExpectFloatNear(0.9f, new_pos);

    // Test invalid articulator
    EXPECT_FALSE(speech_motor_set_articulator(planner, (articulator_type_t)99, 0.5f));
    EXPECT_FALSE(speech_motor_get_articulator(planner, (articulator_type_t)99, &new_pos));

    // Test invalid position
    EXPECT_FALSE(speech_motor_set_articulator(planner, ARTICULATOR_LIPS, -0.1f));
    EXPECT_FALSE(speech_motor_set_articulator(planner, ARTICULATOR_LIPS, 1.1f));
}

TEST_F(SpeechMotorTest, SetGetArticulatorNullInputs) {
    float pos;
    EXPECT_FALSE(speech_motor_set_articulator(nullptr, ARTICULATOR_LIPS, 0.5f));
    EXPECT_FALSE(speech_motor_get_articulator(nullptr, ARTICULATOR_LIPS, &pos));
    EXPECT_FALSE(speech_motor_get_articulator(planner, ARTICULATOR_LIPS, nullptr));
}

// =============================================================================
// Coarticulation Tests
// =============================================================================

TEST_F(SpeechMotorTest, CoarticulationEnabled) {
    // Plan 'p' then 'a'
    // 'p' (lips_pos 0.0), 'a' (lips_pos 0.8)
    // With coarticulation, 'a's lips_pos should be blended: 0.8*(1-0.7) + 0.0*0.7 = 0.8*0.3 = 0.24

    uint8_t phonemes[] = {'p', 'a'};
    EXPECT_TRUE(speech_motor_plan_sequence(planner, phonemes, 2));

    motor_command_t commands[SPEECH_MOTOR_NUM_ARTICULATORS * 2];
    uint32_t count = SPEECH_MOTOR_NUM_ARTICULATORS * 2;
    EXPECT_TRUE(speech_motor_get_commands(planner, commands, &count));
    EXPECT_EQ(SPEECH_MOTOR_NUM_ARTICULATORS * 2, count);

    // Find the lips command for 'a' (which is the second phoneme, commands 5-9)
    // The lips command for 'a' should be commands[5]
    motor_command_t a_lips_cmd = commands[5]; // Assuming ARTICULATOR_LIPS is 0

    PhonemeFeatures p_features = get_expected_phoneme_features('p');
    PhonemeFeatures a_features = get_expected_phoneme_features('a');

    float expected_blended_lips_pos_for_a = a_features.lips_position * (1.0f - config.coarticulation_strength) +
                                            p_features.lips_position * config.coarticulation_strength;
    ExpectFloatNear(expected_blended_lips_pos_for_a, a_lips_cmd.position);
}

TEST_F(SpeechMotorTest, CoarticulationDisabled) {
    config.enable_coarticulation = false;
    speech_motor_destroy(planner);
    planner = speech_motor_create(&config);
    ASSERT_NE(nullptr, planner);

    // Plan 'p' then 'a'
    // 'p' (lips_pos 0.0), 'a' (lips_pos 0.8)
    // Without coarticulation, 'a's lips_pos should be exactly its target (0.8)

    uint8_t phonemes[] = {'p', 'a'};
    EXPECT_TRUE(speech_motor_plan_sequence(planner, phonemes, 2));

    motor_command_t commands[SPEECH_MOTOR_NUM_ARTICULATORS * 2];
    uint32_t count = SPEECH_MOTOR_NUM_ARTICULATORS * 2;
    EXPECT_TRUE(speech_motor_get_commands(planner, commands, &count));
    EXPECT_EQ(SPEECH_MOTOR_NUM_ARTICULATORS * 2, count);

    motor_command_t a_lips_cmd = commands[5]; // Assuming ARTICULATOR_LIPS is 0

    PhonemeFeatures a_features = get_expected_phoneme_features('a');
    ExpectFloatNear(a_features.lips_position, a_lips_cmd.position);
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_F(SpeechMotorTest, GetStats) {
    speech_motor_stats_t stats;
    EXPECT_TRUE(speech_motor_get_stats(planner, &stats));
    EXPECT_EQ(0, stats.phonemes_planned);
    EXPECT_EQ(0, stats.commands_generated);
    EXPECT_EQ(0, stats.queue_size);

    speech_motor_plan_phoneme(planner, 'a');
    EXPECT_TRUE(speech_motor_get_stats(planner, &stats));
    EXPECT_EQ(1, stats.phonemes_planned);
    EXPECT_EQ(SPEECH_MOTOR_NUM_ARTICULATORS, stats.commands_generated);
    EXPECT_EQ(SPEECH_MOTOR_NUM_ARTICULATORS, stats.queue_size);

    motor_command_t commands[1];
    uint32_t count = 1;
    speech_motor_get_commands(planner, commands, &count); // Retrieve one command

    EXPECT_TRUE(speech_motor_get_stats(planner, &stats));
    EXPECT_EQ(1, stats.phonemes_planned);
    EXPECT_EQ(SPEECH_MOTOR_NUM_ARTICULATORS, stats.commands_generated);
    EXPECT_EQ(SPEECH_MOTOR_NUM_ARTICULATORS - 1, stats.queue_size);
}

TEST_F(SpeechMotorTest, GetStatsNullInputs) {
    speech_motor_stats_t stats;
    EXPECT_FALSE(speech_motor_get_stats(nullptr, &stats));
    EXPECT_FALSE(speech_motor_get_stats(planner, nullptr));
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
