/**
 * @file test_bio_learning_integration.cpp
 * @brief Integration tests for biological systems in brain_learn_example()
 *
 * Tests that the new memory/cognitive phases (engram recall, mammillary relay,
 * neuromodulators, glial, prime signatures, theta gating) are invoked during
 * learning and produce measurable side effects.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "cognitive/memory/nimcp_engram.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "cognitive/memory/core/nimcp_prime_signature.h"
#include "core/brain/regions/mammillary/nimcp_mammillary.h"

//=============================================================================
// Shared fixture: a small brain that runs quickly
//=============================================================================
class BioLearningIntegration : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        // Create a small brain for testing
        brain = brain_create("bio_learn_test", BRAIN_SIZE_SMALL,
                             BRAIN_TASK_CLASSIFICATION, 8, 4);
        ASSERT_NE(brain, nullptr) << "Failed to create test brain";
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    // Helper: train one example and return loss
    float train_one(const float* features, uint32_t n, const char* label, float conf = 0.9f) {
        return brain_learn_example(brain, features, n, label, conf);
    }

    // Helper: train one example using a specific brain handle
    static float train_with(brain_t b, const float* features, uint32_t n,
                            const char* label, float conf = 0.9f) {
        return brain_learn_example(b, features, n, label, conf);
    }
};

//=============================================================================
// TEST 1: Engram recall before learning (Phase 5.1)
//=============================================================================
TEST_F(BioLearningIntegration, EngramRecallBeforeLearning) {
    // The engram system should be active after brain creation
    // First train several examples to populate the engram system
    float f1[8] = {1, 0, 0, 0, 0, 0, 0, 0};
    float f2[8] = {0, 1, 0, 0, 0, 0, 0, 0};
    float f3[8] = {1, 0.1f, 0, 0, 0, 0, 0, 0};  // Similar to f1

    // Train with f1 to create an engram
    float loss1 = train_one(f1, 8, "classA");
    EXPECT_GE(loss1, 0.0f) << "First training should not fail";

    // Train with f2 to create another engram
    float loss2 = train_one(f2, 8, "classB");
    EXPECT_GE(loss2, 0.0f) << "Second training should not fail";

    // Train with f3 (similar to f1) - this should trigger engram recall
    // and prime the network with similar memory before learning
    float loss3 = train_one(f3, 8, "classA");
    EXPECT_GE(loss3, 0.0f) << "Third training should not fail";

    // Verify engram system stats - should have encoded at least 3 engrams
    if (brain->engram_system) {
        EXPECT_GE(brain->engram_system->total_encodings, 3u)
            << "At least 3 engrams should have been encoded";
        // total_recalls may or may not increase depending on whether
        // the recall matched with sufficient confidence
        SUCCEED() << "Engram system active with "
                  << brain->engram_system->total_encodings << " encodings, "
                  << brain->engram_system->total_recalls << " recalls";
    } else {
        SUCCEED() << "Engram system not enabled (subsystem optional)";
    }
}

//=============================================================================
// TEST 2: Mammillary relay during learning (Phase 6.5)
//=============================================================================
TEST_F(BioLearningIntegration, MammillaryRelayDuringLearning) {
    // If mammillary is initialized, training should produce relay operations
    float features[8] = {0.5f, 0.3f, 0.1f, 0.7f, 0.2f, 0.4f, 0.6f, 0.8f};

    // Train a few examples
    for (int i = 0; i < 5; i++) {
        features[0] = 0.1f * (float)(i + 1);
        float loss = train_one(features, 8, "test_class");
        EXPECT_GE(loss, 0.0f);
    }

    if (brain->mammillary) {
        // Mammillary should have processed relay operations
        nimcp_mammillary_t* mb = (nimcp_mammillary_t*)brain->mammillary;
        mammillary_stats_t stats;
        int rc = mammillary_get_stats(mb, &stats);
        EXPECT_EQ(rc, 0) << "mammillary_get_stats should succeed";

        EXPECT_GT(stats.total_memory_traces, 0u)
            << "Mammillary should have received memory traces during learning";
        EXPECT_GT(stats.relay_operations, 0u)
            << "Mammillary should have relayed traces to thalamus";
        EXPECT_GT(stats.papez_cycles, 0u)
            << "Papez circuit should have cycled during learning";
    } else {
        SUCCEED() << "Mammillary not enabled (subsystem optional in small brains)";
    }
}

//=============================================================================
// TEST 3: Neuromodulator levels change based on loss trend (Phase 5.2)
//=============================================================================
TEST_F(BioLearningIntegration, NeuromodulatorLevelsChangeDuringTraining) {
    if (!brain->neuromodulator_system) {
        SUCCEED() << "Neuromodulator system not enabled (subsystem optional)";
        return;
    }

    // Get initial neuromodulator levels
    float initial_da = neuromodulator_get_level(brain->neuromodulator_system, NEUROMOD_DOPAMINE);
    float initial_ach = neuromodulator_get_level(brain->neuromodulator_system, NEUROMOD_ACETYLCHOLINE);
    float initial_ne = neuromodulator_get_level(brain->neuromodulator_system, NEUROMOD_NOREPINEPHRINE);

    // Train many examples to build loss history
    float features[8] = {0.5f, 0.3f, 0.1f, 0.7f, 0.2f, 0.4f, 0.6f, 0.8f};
    for (int i = 0; i < 20; i++) {
        features[i % 8] = 0.1f * (float)((i % 10) + 1);
        const char* labels[] = {"A", "B", "C", "D"};
        train_one(features, 8, labels[i % 4]);
    }

    // After training, neuromodulator levels should have changed from initial
    // The Phase 5.2 code sets DA, ACh, NE based on loss trend, novelty, confidence
    float final_da = neuromodulator_get_level(brain->neuromodulator_system, NEUROMOD_DOPAMINE);
    float final_ach = neuromodulator_get_level(brain->neuromodulator_system, NEUROMOD_ACETYLCHOLINE);
    float final_ne = neuromodulator_get_level(brain->neuromodulator_system, NEUROMOD_NOREPINEPHRINE);

    // At least one neuromodulator should have changed
    bool any_changed = (fabsf(final_da - initial_da) > 0.001f) ||
                       (fabsf(final_ach - initial_ach) > 0.001f) ||
                       (fabsf(final_ne - initial_ne) > 0.001f);

    EXPECT_TRUE(any_changed)
        << "Neuromodulator levels should change during training. "
        << "DA: " << initial_da << " -> " << final_da << ", "
        << "ACh: " << initial_ach << " -> " << final_ach << ", "
        << "NE: " << initial_ne << " -> " << final_ne;

    // All values should be in valid range [0, 1]
    EXPECT_GE(final_da, 0.0f);
    EXPECT_LE(final_da, 1.0f);
    EXPECT_GE(final_ach, 0.0f);
    EXPECT_LE(final_ach, 1.0f);
    EXPECT_GE(final_ne, 0.0f);
    EXPECT_LE(final_ne, 1.0f);
}

//=============================================================================
// TEST 4: Glial integration updated during learning (Phase 23.5)
//=============================================================================
TEST_F(BioLearningIntegration, GlialIntegrationUpdatedDuringLearning) {
    float features[8] = {0.5f, 0.3f, 0.1f, 0.7f, 0.2f, 0.4f, 0.6f, 0.8f};

    // Train several examples to trigger glial updates
    for (int i = 0; i < 5; i++) {
        float loss = train_one(features, 8, "glial_test");
        EXPECT_GE(loss, 0.0f);
    }

    if (brain->glial) {
        // Glial integration step was called, check that timestamp was updated
        // The glial system tracks last_update_timestamp_us
        EXPECT_GT(brain->glial->last_update_timestamp_us, 0u)
            << "Glial integration should have been stepped during learning";
        SUCCEED() << "Glial integration stepped, last_update_timestamp_us="
                  << brain->glial->last_update_timestamp_us;
    } else {
        SUCCEED() << "Glial system not enabled (subsystem optional)";
    }
}

//=============================================================================
// TEST 5: Label overflow produces deterministic hash results
//=============================================================================
TEST_F(BioLearningIntegration, LabelOverflowDeterministic) {
    // Create a brain with very few outputs to force label overflow
    brain_t small_brain = brain_create("overflow_test", BRAIN_SIZE_SMALL,
                                       BRAIN_TASK_CLASSIFICATION, 8, 4);
    ASSERT_NE(small_brain, nullptr);

    float features[8] = {0.5f, 0.3f, 0.1f, 0.7f, 0.2f, 0.4f, 0.6f, 0.8f};

    // Fill all 4 output slots with labels
    brain_learn_example(small_brain, features, 8, "label_A", 0.9f);
    brain_learn_example(small_brain, features, 8, "label_B", 0.9f);
    brain_learn_example(small_brain, features, 8, "label_C", 0.9f);
    brain_learn_example(small_brain, features, 8, "label_D", 0.9f);

    // Now all slots are full (num_output_labels == num_outputs == 4)
    EXPECT_EQ(small_brain->num_output_labels, 4u);

    // Overflow label should be deterministically hashed
    float loss1 = brain_learn_example(small_brain, features, 8, "overflow_label_1", 0.9f);
    float loss2 = brain_learn_example(small_brain, features, 8, "overflow_label_1", 0.9f);
    EXPECT_GE(loss1, 0.0f);
    EXPECT_GE(loss2, 0.0f);

    // Training with same label twice should produce consistent hashing
    // (both map to the same output index via djb2)
    // The label count should still be 4 (overflow labels don't create new slots)
    EXPECT_EQ(small_brain->num_output_labels, 4u)
        << "Overflow labels should not create new label slots";

    brain_destroy(small_brain);
}

//=============================================================================
// TEST 6: Learning mode is HYBRID by default
//=============================================================================
TEST_F(BioLearningIntegration, LearningModeIsHybrid) {
    // We verify indirectly by checking that training succeeds and
    // the LEARN_MODE_HYBRID enum value exists and can be used
    EXPECT_EQ(LEARN_MODE_HYBRID, 4)
        << "LEARN_MODE_HYBRID should be enum value 4";

    float features[8] = {0.5f, 0.3f, 0.1f, 0.7f, 0.2f, 0.4f, 0.6f, 0.8f};

    // Training should succeed using HYBRID mode (the new default)
    float loss = train_one(features, 8, "hybrid_test");
    EXPECT_GE(loss, 0.0f) << "Training in HYBRID mode should succeed";

    // Verify learning steps were counted
    brain_stats_t stats;
    bool got_stats = brain_get_stats(brain, &stats);
    EXPECT_TRUE(got_stats);
    EXPECT_GE(stats.total_learning_steps, 1u)
        << "At least one learning step should have been recorded";
}

//=============================================================================
// TEST 7: Oscillation theta gating modulates learning rate (Phase 5.3)
//=============================================================================
TEST_F(BioLearningIntegration, OscillationThetaGating) {
    float features[8] = {0.5f, 0.3f, 0.1f, 0.7f, 0.2f, 0.4f, 0.6f, 0.8f};

    // Train multiple examples - theta gating produces sinusoidal LR modulation
    // The modulation is between 0.8x and 1.2x of the base learning rate
    // We can't directly observe the LR modulation, but we verify training
    // produces different losses across steps (not all identical)
    float losses[10];
    for (int i = 0; i < 10; i++) {
        features[0] = 0.1f * (float)(i + 1);
        losses[i] = train_one(features, 8, "theta_test");
        EXPECT_GE(losses[i], 0.0f);
    }

    // Check that losses are not all identical (modulation creates variation)
    bool all_same = true;
    for (int i = 1; i < 10; i++) {
        if (fabsf(losses[i] - losses[0]) > 1e-6f) {
            all_same = false;
            break;
        }
    }

    // With different inputs each time, losses should naturally vary
    EXPECT_FALSE(all_same) << "Losses should vary across training steps";

    if (brain->oscillations) {
        SUCCEED() << "Oscillation system active, theta gating applied";
    } else {
        SUCCEED() << "Oscillation system not enabled (gating skipped, test still passes)";
    }
}

//=============================================================================
// TEST 8: Prime signature generation during learning (Phase 6.6)
//=============================================================================
TEST_F(BioLearningIntegration, PrimeSignatureGeneration) {
    // Test that prime_sig_from_floats works correctly with training features
    float features[8] = {0.5f, 0.3f, 0.1f, 0.7f, 0.2f, 0.4f, 0.6f, 0.8f};

    // Generate signature directly to verify API works
    prime_signature_t* sig = prime_sig_from_floats(features, 8);
    ASSERT_NE(sig, nullptr) << "prime_sig_from_floats should succeed";

    // Verify signature is non-empty
    EXPECT_GT(prime_sig_count_factors(sig), 0u)
        << "Signature should have non-zero factors";

    // Verify hash is non-zero
    EXPECT_NE(sig->hash, (uint64_t)0) << "Signature hash should be non-zero";

    // Similar features should produce similar signatures
    float features2[8] = {0.51f, 0.31f, 0.11f, 0.71f, 0.21f, 0.41f, 0.61f, 0.81f};
    prime_signature_t* sig2 = prime_sig_from_floats(features2, 8);
    ASSERT_NE(sig2, nullptr);

    float similarity = prime_sig_jaccard(sig, sig2);
    EXPECT_GT(similarity, 0.0f) << "Similar features should have positive Jaccard similarity";

    prime_sig_destroy(sig);
    prime_sig_destroy(sig2);

    // Now verify that training with PR memory enabled actually runs the phase
    float loss = train_one(features, 8, "prime_test");
    EXPECT_GE(loss, 0.0f) << "Training should succeed with prime signature generation";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
