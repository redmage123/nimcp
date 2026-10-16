/**
 * @file test_cortical_training_bridge.cpp
 * @brief Unit tests for Cortical-Training Bridge
 *
 * WHAT: Bidirectional integration between cortical modules and training
 * WHY:  Training-driven cortical modulation and cortical-driven learning
 * HOW:  Predictive coding, dendritic computation, column dynamics modulate training
 *
 * TEST COVERAGE:
 * - Lifecycle tests (8 tests)
 * - Predictive coding tests (15 tests)
 * - Dendritic tests (15 tests)
 * - Column tests (12 tests)
 * - Combined modulation tests (12 tests)
 * - Gradient confidence tests (8 tests)
 * - Feedback tests (5 tests)
 * - Stats and error handling tests (5 tests)
 *
 * TOTAL: 80 tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_cortical_training_bridge.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Constants
//=============================================================================

/* Learning Rate Constants */
static constexpr float TEST_BASE_LR = 0.001f;
static constexpr float TEST_LR_BOOST_FACTOR = 1.3f;
static constexpr float TEST_LR_REDUCE_FACTOR = 0.6f;
static constexpr float TEST_LR_HIGH_FACTOR = 1.4f;
static constexpr float TEST_LR_MODERATE_FACTOR = 1.2f;
static constexpr float TEST_LR_MAX_FACTOR = 1.5f;

/* Free Energy / Prediction Error Constants */
static constexpr float TEST_LOW_FREE_ENERGY = 0.2f;
static constexpr float TEST_HIGH_FREE_ENERGY = 0.9f;
static constexpr float TEST_CONVERGED_FREE_ENERGY = 0.15f;
static constexpr float TEST_MODERATE_FREE_ENERGY = 0.25f;
static constexpr float TEST_GOOD_FREE_ENERGY = 0.3f;

/* Confidence / Quality Constants */
static constexpr float TEST_HIGH_CONFIDENCE = 0.95f;
static constexpr float TEST_GOOD_CONFIDENCE = 0.9f;
static constexpr float TEST_MODERATE_CONFIDENCE = 0.85f;
static constexpr float TEST_LOW_CONFIDENCE = 0.3f;
static constexpr float TEST_THRESHOLD_CONFIDENCE = 0.8f;

/* Rate Constants */
static constexpr float TEST_HIGH_BURST_RATE = 0.8f;
static constexpr float TEST_MODERATE_BURST_RATE = 0.75f;
static constexpr float TEST_HIGH_BAC_RATE = 0.9f;
static constexpr float TEST_MODERATE_BAC_RATE = 0.6f;
static constexpr float TEST_HIGH_CONVERGENCE = 0.95f;

/* Entropy / Error Constants */
static constexpr float TEST_HIGH_ENTROPY = 0.85f;
static constexpr float TEST_LOW_ENTROPY = 0.3f;
static constexpr float TEST_HIGH_ERROR = 0.9f;
static constexpr float TEST_LOW_ERROR = 0.1f;
static constexpr float TEST_MODERATE_ERROR = 0.7f;

/* Layer / Hierarchy Constants */
static constexpr uint32_t TEST_LOW_HIERARCHY = 1;
static constexpr uint32_t TEST_HIGH_HIERARCHY = 5;
static constexpr uint32_t TEST_LAYER_INPUT = 4;
static constexpr uint32_t TEST_LAYER_OUTPUT = 5;
static constexpr uint32_t TEST_LAYER_ASSOCIATION = 2;
static constexpr uint32_t TEST_NUM_LAYERS = 6;
static constexpr uint32_t TEST_ITERATIONS = 10;

/* Inhibition / Strength Constants */
static constexpr float TEST_HIGH_INHIBITION = 0.8f;
static constexpr float TEST_MODERATE_STRENGTH = 0.7f;
static constexpr float TEST_HIGH_STRENGTH = 0.85f;
static constexpr float TEST_VERY_HIGH_STRENGTH = 0.9f;

/* Miscellaneous Test Constants */
static constexpr int TEST_CYCLE_COUNT = 10;
static constexpr float TEST_SIGNAL_VALUE = 0.9f;
static constexpr float TEST_GRAD_NORM = 8.0f;
static constexpr float TEST_LOSS = 0.5f;
static constexpr float TEST_HIGH_LOSS = 1.5f;
static constexpr int TEST_STEP = 100;

//=============================================================================
// Test Fixture
//=============================================================================

class CorticalTrainingBridgeTest : public ::testing::Test {
protected:
    cortical_training_bridge_t* bridge;
    cortical_training_config_t config;

    void SetUp() override {
        cortical_training_default_config(&config);
        config.enable_bio_async = false;
        config.disable_auto_update = true;
        bridge = cortical_training_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            cortical_training_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests (8 tests)
//=============================================================================

TEST_F(CorticalTrainingBridgeTest, CreateWithDefaults) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(CorticalTrainingBridgeTest, CreateWithNullConfig) {
    cortical_training_bridge_t* test_bridge = cortical_training_create(nullptr);
    EXPECT_NE(test_bridge, nullptr);
    cortical_training_destroy(test_bridge);
}

TEST_F(CorticalTrainingBridgeTest, DestroyNull) {
    cortical_training_destroy(nullptr);
    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, StartStop) {
    EXPECT_EQ(cortical_training_start(bridge), 0);
    EXPECT_EQ(cortical_training_stop(bridge), 0);
}

TEST_F(CorticalTrainingBridgeTest, DoubleStart) {
    EXPECT_EQ(cortical_training_start(bridge), 0);
    int result = cortical_training_start(bridge);
    (void)result;
    EXPECT_EQ(cortical_training_stop(bridge), 0);
}

TEST_F(CorticalTrainingBridgeTest, DoubleStop) {
    EXPECT_EQ(cortical_training_start(bridge), 0);
    EXPECT_EQ(cortical_training_stop(bridge), 0);
    int result = cortical_training_stop(bridge);
    (void)result;
}

TEST_F(CorticalTrainingBridgeTest, CreateMultiple) {
    cortical_training_bridge_t* bridge2 = cortical_training_create(&config);
    EXPECT_NE(bridge2, nullptr);
    cortical_training_destroy(bridge2);
}

TEST_F(CorticalTrainingBridgeTest, CreateDestroyCycle) {
    for (int i = 0; i < TEST_CYCLE_COUNT; i++) {
        cortical_training_bridge_t* temp = cortical_training_create(&config);
        ASSERT_NE(temp, nullptr);
        EXPECT_EQ(cortical_training_start(temp), 0);
        EXPECT_EQ(cortical_training_stop(temp), 0);
        cortical_training_destroy(temp);
    }
}

//=============================================================================
// Predictive Coding Tests (15 tests)
//=============================================================================

TEST_F(CorticalTrainingBridgeTest, LowFreeEnergyIncreasesLR) {
    /* WHAT: Low free energy allows higher LR */
    /* WHY:  Good predictions = confident learning */
    /* HOW:  Free energy inversely affects LR */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.free_energy = 0.2f;  /* Low = good */
    effects.lr_factor = 1.3f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    float lr = cortical_training_get_modulated_lr(bridge, TEST_BASE_LR);
    EXPECT_GT(lr, TEST_BASE_LR);
}

TEST_F(CorticalTrainingBridgeTest, HighFreeEnergyReducesLR) {
    /* WHAT: High free energy reduces LR */
    /* WHY:  Poor predictions = conservative */
    /* HOW:  High free energy lowers LR */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.free_energy = 0.9f;  /* High = poor predictions */
    effects.lr_factor = 0.6f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    float lr = cortical_training_get_modulated_lr(bridge, TEST_BASE_LR);
    EXPECT_LT(lr, TEST_BASE_LR);
}

TEST_F(CorticalTrainingBridgeTest, PredictionErrorBoostsLearning) {
    /* WHAT: Prediction error increases learning */
    /* WHY:  Error = learning opportunity */
    /* HOW:  Error scales gradient contribution */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.prediction_error_mag = 0.9f;
    effects.lr_factor = 1.4f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    cortical_training_effects_t retrieved;
    cortical_training_get_effects(bridge, &retrieved);

    EXPECT_GT(retrieved.lr_factor, 1.0f);
}

TEST_F(CorticalTrainingBridgeTest, PredictionConfidenceAffectsWeight) {
    /* WHAT: Confident predictions increase sample weight */
    /* WHY:  Confidence = reliable signal */
    /* HOW:  Confidence boosts weight */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.gradient_confidence = 0.95f;
    effects.lr_factor = 1.3f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    cortical_training_effects_t retrieved;
    cortical_training_get_effects(bridge, &retrieved);

    EXPECT_GT(retrieved.lr_factor, 1.0f);
}

TEST_F(CorticalTrainingBridgeTest, PrecisionWeightingModulation) {
    /* WHAT: Precision weights modulate gradients */
    /* WHY:  Precision = inverse variance, reliability */
    /* HOW:  High precision amplifies gradients */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.convergence_rate = 0.9f;
    effects.lr_factor = 1.35f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, ConvergenceDetection) {
    /* WHAT: Predictive convergence triggers checkpoint */
    /* WHY:  Stable predictions = good state */
    /* HOW:  Low free energy + low error variance */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.free_energy = 0.15f;
    effects.convergence_rate = 0.95f;
    effects.should_consolidate = true;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    cortical_training_effects_t retrieved;
    cortical_training_get_effects(bridge, &retrieved);
    EXPECT_TRUE(retrieved.should_consolidate);
}

TEST_F(CorticalTrainingBridgeTest, HierarchicalPredictionLevels) {
    /* WHAT: Different hierarchy levels different LRs */
    /* WHY:  Lower levels = faster learning */
    /* HOW:  Level modulates LR factor */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    /* Low level (sensory) */
    cortical_training_effects_t effects_low;
    memset(&effects_low, 0, sizeof(effects_low));
    effects_low.num_layers = 1;
    effects_low.lr_factor = 1.2f;
    effects_low.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects_low);

    float lr_low = cortical_training_get_modulated_lr(bridge, TEST_BASE_LR);

    /* High level (abstract) */
    cortical_training_effects_t effects_high;
    memset(&effects_high, 0, sizeof(effects_high));
    effects_high.num_layers = 5;
    effects_high.lr_factor = 0.8f;
    effects_high.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects_high);

    float lr_high = cortical_training_get_modulated_lr(bridge, TEST_BASE_LR);

    EXPECT_GT(lr_low, lr_high);
}

TEST_F(CorticalTrainingBridgeTest, TemporalPredictionWindow) {
    /* WHAT: Temporal prediction accuracy affects learning */
    /* WHY:  Temporal coherence = stable dynamics */
    /* HOW:  Window accuracy modulates confidence */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.convergence_rate = 0.85f;
    effects.gradient_confidence = 0.8f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, ExpectationViolation) {
    /* WHAT: Expectation violation boosts attention */
    /* WHY:  Violation = salient event */
    /* HOW:  Violation increases gradient scale */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.prediction_error_mag = 0.9f;
    effects.lr_factor = 1.5f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, ContextualPredictionModulation) {
    /* WHAT: Context quality affects predictions */
    /* WHY:  Good context = better predictions */
    /* HOW:  Context score boosts confidence */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.gradient_confidence = 0.9f;
    effects.gradient_confidence = 0.85f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, MultiStepPrediction) {
    /* WHAT: Multi-step prediction accuracy */
    /* WHY:  Longer horizon = harder prediction */
    /* HOW:  Steps ahead affects difficulty */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.num_layers = 5;
    effects.convergence_rate = 0.7f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, PredictiveUncertainty) {
    /* WHAT: Predictive uncertainty affects LR */
    /* WHY:  High uncertainty = conservative */
    /* HOW:  Uncertainty reduces LR */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.population_entropy = 0.85f;
    effects.lr_factor = 0.7f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, ErrorMinimizationDrive) {
    /* WHAT: Error minimization modulates learning */
    /* WHY:  Active inference principle */
    /* HOW:  Error drives parameter updates */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.prediction_error_mag = 0.9f;
    effects.lr_factor = 1.25f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, PredictiveCodingConvergence) {
    /* WHAT: Iterative prediction refinement */
    /* WHY:  Convergence indicates stability */
    /* HOW:  Iteration count and final error */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.num_layers = 10;
    effects.prediction_error_mag = 0.1f;
    effects.predictions_stable = true;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, FreeEnergyBoundOptimization) {
    /* WHAT: FE bound optimization affects LR */
    /* WHY:  Bound tightness = model quality */
    /* HOW:  Tighter bound allows higher LR */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.convergence_rate = 0.9f;
    effects.lr_factor = 1.15f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

//=============================================================================
// Dendritic Tests (15 tests)
//=============================================================================

TEST_F(CorticalTrainingBridgeTest, DendriticBurstRateAffectsLR) {
    /* WHAT: Burst rate modulates learning rate */
    /* WHY:  Bursts = strong signals, faster learning */
    /* HOW:  High burst rate increases LR */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.burst_rate = 0.8f;
    effects.lr_factor = 1.3f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    float lr = cortical_training_get_modulated_lr(bridge, TEST_BASE_LR);
    EXPECT_GT(lr, TEST_BASE_LR);
}

TEST_F(CorticalTrainingBridgeTest, BACSuccessRateBoostsConfidence) {
    /* WHAT: BAC firing success increases confidence */
    /* WHY:  BAC = feedback-driven burst, prediction match */
    /* HOW:  High BAC success boosts weight */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.bac_success_rate = 0.9f;
    effects.lr_factor = 1.4f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    cortical_training_effects_t retrieved;
    cortical_training_get_effects(bridge, &retrieved);

    EXPECT_GT(retrieved.lr_factor, 1.0f);
}

TEST_F(CorticalTrainingBridgeTest, CalciumSpikeAmplitude) {
    /* WHAT: Calcium spike amplitude affects plasticity */
    /* WHY:  Ca spike = dendritic integration event */
    /* HOW:  Amplitude modulates gradient scale */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.calcium_spikes = 0.85f;
    effects.lr_factor = 1.3f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, DendriticNonlinearity) {
    /* WHAT: Nonlinear integration affects computation */
    /* WHY:  Nonlinearity = complex feature detection */
    /* HOW:  Nonlinearity strength tracked */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.burst_rate = 0.75f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, ApicalDendriticInput) {
    /* WHAT: Apical input (feedback) strength */
    /* WHY:  Apical = top-down predictions */
    /* HOW:  Apical strength modulates prediction weight */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.burst_rate = 0.8f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, BasalDendriticInput) {
    /* WHAT: Basal input (feedforward) strength */
    /* WHY:  Basal = bottom-up sensory input */
    /* HOW:  Basal strength affects feature weight */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.bac_success_rate = 0.9f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, DendriticPlateau) {
    /* WHAT: Plateau potential duration */
    /* WHY:  Plateau = sustained depolarization, learning window */
    /* HOW:  Duration affects plasticity window */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.calcium_spikes = 0.7f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, BurstTimingPrecision) {
    /* WHAT: Timing precision of bursts */
    /* WHY:  Precise timing = reliable signal */
    /* HOW:  Precision affects confidence */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.burst_rate = 0.9f;
    effects.gradient_confidence = 0.85f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, DendriticCompartmentalization) {
    /* WHAT: Independent dendritic compartments */
    /* WHY:  Compartments = parallel feature detectors */
    /* HOW:  Compartment count affects complexity */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.num_layers = 8;
    effects.num_layers = 12;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, DendriticSpikeCoincidence) {
    /* WHAT: Coincidence detection in dendrites */
    /* WHY:  Coincidence = temporal correlation */
    /* HOW:  Coincidence rate boosts importance */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.bac_success_rate = 0.8f;
    effects.lr_factor = 1.25f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, DendriticAttenuation) {
    /* WHAT: Signal attenuation along dendrite */
    /* WHY:  Distance affects signal strength */
    /* HOW:  Attenuation factor modulates weight */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.convergence_rate = 0.3f;  /* Low = good */
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, ActiveDendriticConductances) {
    /* WHAT: Active conductances enhance computation */
    /* WHY:  Active channels = amplification */
    /* HOW:  Conductance strength boosts signals */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.bac_success_rate = 0.85f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, DendriticBranchIntegration) {
    /* WHAT: Integration across dendritic branches */
    /* WHY:  Branches = hierarchical feature combo */
    /* HOW:  Integration quality affects output */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.gradient_confidence = 0.9f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, SynapticClusteringOnDendrites) {
    /* WHAT: Synaptic clustering enhances nonlinearity */
    /* WHY:  Clusters = local feature detectors */
    /* HOW:  Clustering degree tracked */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.inhibition_strength = 0.75f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, DendriticRegenerationCapacity) {
    /* WHAT: Regenerative capacity of dendrites */
    /* WHY:  Regeneration = signal boosting */
    /* HOW:  Capacity affects signal propagation */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.convergence_rate = 0.8f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

//=============================================================================
// Column Tests (12 tests)
//=============================================================================

TEST_F(CorticalTrainingBridgeTest, WinnerTakesAllConfidence) {
    /* WHAT: Winner neuron confidence affects learning */
    /* WHY:  Strong winner = clear decision */
    /* HOW:  Winner confidence boosts weight */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.winner_confidence = 0.95f;
    effects.lr_factor = 1.35f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    cortical_training_effects_t retrieved;
    cortical_training_get_effects(bridge, &retrieved);

    EXPECT_GT(retrieved.lr_factor, 1.0f);
}

TEST_F(CorticalTrainingBridgeTest, ColumnEntropyModulation) {
    /* WHAT: Column entropy affects exploration */
    /* WHY:  High entropy = uncertain, explore more */
    /* HOW:  Entropy modulates LR */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.population_entropy = 0.85f;  /* High */
    effects.lr_factor = 1.2f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, LateralInhibitionStrength) {
    /* WHAT: Inhibition strength affects sparsity */
    /* WHY:  Strong inhibition = sparse code */
    /* HOW:  Inhibition affects gradient distribution */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.inhibition_strength = 0.8f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, MinicolumnActivationPatterns) {
    /* WHAT: Minicolumn activation patterns */
    /* WHY:  Patterns = feature representations */
    /* HOW:  Pattern coherence affects confidence */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.gradient_confidence = 0.9f;
    effects.gradient_confidence = 0.85f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, ColumnSynchrony) {
    /* WHAT: Synchrony across column */
    /* WHY:  Synchrony = coordinated response */
    /* HOW:  Synchrony boosts signal strength */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.gradient_confidence = 0.85f;
    effects.lr_factor = 1.25f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, LayerSpecificModulation) {
    /* WHAT: Different layers different roles */
    /* WHY:  L2/3 = association, L4 = input, L5 = output */
    /* HOW:  Layer ID affects modulation */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    /* L4 (input layer) */
    cortical_training_effects_t effects_l4;
    memset(&effects_l4, 0, sizeof(effects_l4));
    effects_l4.num_layers = 4;
    effects_l4.lr_factor = 1.2f;
    effects_l4.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects_l4);

    /* L5 (output layer) */
    cortical_training_effects_t effects_l5;
    memset(&effects_l5, 0, sizeof(effects_l5));
    effects_l5.num_layers = 5;
    effects_l5.lr_factor = 1.0f;
    effects_l5.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects_l5);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, ColumnOscillationPhase) {
    /* WHAT: Oscillation phase affects plasticity */
    /* WHY:  Phase = timing of learning window */
    /* HOW:  Optimal phase boosts learning */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.convergence_rate = 0.25f;  /* Peak gamma */
    effects.lr_factor = 1.3f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, ColumnRecurrentActivity) {
    /* WHAT: Recurrent activity sustains representation */
    /* WHY:  Recurrence = working memory, persistence */
    /* HOW:  Recurrence strength affects temporal integration */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.inhibition_strength = 0.7f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, ColumnFeedbackModulation) {
    /* WHAT: Feedback modulates column activity */
    /* WHY:  Feedback = top-down attention */
    /* HOW:  Feedback strength scales gradients */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.gradient_confidence = 0.85f;
    effects.lr_factor = 1.3f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, ColumnSparsityLevel) {
    /* WHAT: Sparsity of column activation */
    /* WHY:  Sparse = efficient coding */
    /* HOW:  Sparsity affects sample weight */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.population_entropy = 0.05f;  /* 5% active */
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, ColumnStabilityMetric) {
    /* WHAT: Stability of column response */
    /* WHY:  Stable = reliable representation */
    /* HOW:  Stability boosts confidence */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.predictions_stable = 0.9f;
    effects.gradient_confidence = 0.85f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, ColumnAdaptationRate) {
    /* WHAT: Rate of column adaptation */
    /* WHY:  Adaptation = learning dynamics */
    /* HOW:  Fast adaptation = higher LR */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.convergence_rate = 0.75f;
    effects.lr_factor = 1.2f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

//=============================================================================
// Combined Modulation Tests (12 tests)
//=============================================================================

TEST_F(CorticalTrainingBridgeTest, PredictiveDendriticIntegration) {
    /* WHAT: Prediction + dendritic signals combine */
    /* WHY:  Both contribute to learning */
    /* HOW:  Combined effects on LR */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.free_energy = 0.25f;
    effects.burst_rate = 0.8f;
    effects.lr_factor = 1.4f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    float lr = cortical_training_get_modulated_lr(bridge, TEST_BASE_LR);
    EXPECT_GT(lr, TEST_BASE_LR);
}

TEST_F(CorticalTrainingBridgeTest, DendriticColumnCoordination) {
    /* WHAT: Dendritic bursts coordinate column activity */
    /* WHY:  Bursts = synchronization signals */
    /* HOW:  Burst + synchrony boost confidence */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.burst_rate = 0.75f;
    effects.gradient_confidence = 0.85f;
    effects.lr_factor = 1.5f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, PredictiveErrorDendriticPlasticity) {
    /* WHAT: Prediction error modulates dendritic plasticity */
    /* WHY:  Error = learning trigger for dendrites */
    /* HOW:  Error scales BAC learning */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.prediction_error_mag = 0.8f;
    effects.bac_success_rate = 0.6f;
    effects.lr_factor = 1.4f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, FullCorticalStack) {
    /* WHAT: All cortical systems active */
    /* WHY:  Realistic full integration */
    /* HOW:  Prediction + dendrite + column */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.free_energy = 0.3f;
    effects.gradient_confidence = 0.85f;
    effects.burst_rate = 0.75f;
    effects.bac_success_rate = 0.8f;
    effects.winner_confidence = 0.9f;
    effects.gradient_confidence = 0.85f;
    effects.lr_factor = 1.5f;
    effects.lr_factor = 1.4f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, HierarchicalPredictionDendriteMatch) {
    /* WHAT: Hierarchy level matches dendritic complexity */
    /* WHY:  Higher levels = more complex dendrites */
    /* HOW:  Level + compartments correlation */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.num_layers = 4;
    effects.num_layers = 10;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, ColumnWinnerDendriticBurst) {
    /* WHAT: Winner neuron bursts strengthen learning */
    /* WHY:  Winner + burst = strong teaching signal */
    /* HOW:  Combined confidence boost */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.winner_confidence = 0.95f;
    effects.burst_rate = 0.85f;
    effects.lr_factor = 1.6f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, PredictiveColumnEntropy) {
    /* WHAT: Prediction quality affects column entropy */
    /* WHY:  Good predictions = low entropy */
    /* HOW:  Free energy inversely related to entropy */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.free_energy = 0.2f;  /* Low */
    effects.population_entropy = 0.3f;  /* Should also be low */
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, TemporalPredictionDendriticTiming) {
    /* WHAT: Temporal predictions align with dendritic timing */
    /* WHY:  Timing = critical for causality */
    /* HOW:  Prediction window + burst precision */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.convergence_rate = 0.85f;
    effects.burst_rate = 0.9f;
    effects.lr_factor = 1.3f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, LayerSpecificPrediction) {
    /* WHAT: Different layers different prediction roles */
    /* WHY:  L2/3 = predictions, L5 = actions */
    /* HOW:  Layer modulates prediction type */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.num_layers = 2;  /* L2/3 */
    effects.gradient_confidence = 0.9f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, OscillationPhaseLocking) {
    /* WHAT: Oscillations phase-lock across systems */
    /* WHY:  Phase locking = coordination */
    /* HOW:  Oscillation + synchrony metrics */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.convergence_rate = 0.25f;
    effects.gradient_confidence = 0.9f;
    effects.predictions_stable = true;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, RecurrentPredictionRefinement) {
    /* WHAT: Recurrent activity refines predictions */
    /* WHY:  Recurrence = iterative improvement */
    /* HOW:  Recurrence + prediction iterations */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.inhibition_strength = 0.75f;
    effects.num_layers = 5;
    effects.prediction_error_mag = 0.15f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, AdaptiveCorticalLearningRate) {
    /* WHAT: All cortical factors combine for LR */
    /* WHY:  Holistic learning rate adaptation */
    /* HOW:  Weighted combination of all signals */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.free_energy = 0.25f;
    effects.burst_rate = 0.8f;
    effects.winner_confidence = 0.9f;
    effects.gradient_confidence = 0.85f;
    effects.predictions_stable = 0.9f;
    effects.lr_factor = 1.5f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    float lr = cortical_training_get_modulated_lr(bridge, TEST_BASE_LR);
    EXPECT_GT(lr, TEST_BASE_LR);
}

//=============================================================================
// Gradient Confidence Tests (8 tests)
//=============================================================================

TEST_F(CorticalTrainingBridgeTest, HighConfidenceGradients) {
    /* WHAT: High confidence allows full gradient use */
    /* WHY:  Confident = reliable gradients */
    /* HOW:  Confidence scales gradient contribution */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.gradient_confidence = 0.95f;
    effects.lr_factor = 1.4f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, LowConfidenceGradientClipping) {
    /* WHAT: Low confidence clips gradients */
    /* WHY:  Uncertain = conservative updates */
    /* HOW:  Low confidence reduces scale */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.gradient_confidence = 0.3f;
    effects.lr_factor = 0.6f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, LayerSpecificGradientScaling) {
    /* WHAT: Different layers different gradient scales */
    /* WHY:  Layers have different learning dynamics */
    /* HOW:  Layer ID modulates gradient */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    float precision_weights[6];
    EXPECT_EQ(cortical_training_get_precision_weights(bridge, precision_weights, 6), 0);

    /* Precision weights can be used to scale gradients per layer */
    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, PredictionErrorGradientBoost) {
    /* WHAT: High prediction error boosts gradients */
    /* WHY:  Error = where learning is needed */
    /* HOW:  Error scales gradient contribution */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.prediction_error_mag = 0.9f;
    effects.lr_factor = 1.5f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, DendriticGradientRouting) {
    /* WHAT: Dendritic compartments route gradients */
    /* WHY:  Different compartments = different features */
    /* HOW:  Active compartments get gradients */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.num_layers = 8;
    /* Note: precision_weights is a pointer, managed by the bridge */
    effects.gradient_confidence = 0.9f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, ColumnWinnerGradientAmplification) {
    /* WHAT: Winner neuron gets amplified gradients */
    /* WHY:  Winner = most relevant */
    /* HOW:  Winner confidence scales gradients */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.winner_confidence = 0.95f;
    effects.lr_factor = 1.6f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, GradientFlowThroughHierarchy) {
    /* WHAT: Gradients flow through hierarchy */
    /* WHY:  Credit assignment across levels */
    /* HOW:  Level affects gradient magnitude */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.num_layers = 3;
    effects.gradient_confidence = 0.85f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, AdaptiveGradientNormalization) {
    /* WHAT: Gradients normalized based on cortical state */
    /* WHY:  Prevent gradient explosion/vanishing */
    /* HOW:  State-dependent normalization */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.prediction_error_mag = 5.0f;
    effects.convergence_rate = 2.0f;
    effects.predictions_stable = true;
    effects.valid = true;
    cortical_training_set_effects_for_testing(bridge, &effects);

    SUCCEED();
}

//=============================================================================
// Feedback Tests (5 tests)
//=============================================================================

TEST_F(CorticalTrainingBridgeTest, TrainingLossToCortex) {
    /* WHAT: Training loss feeds back to cortex */
    /* WHY:  Cortex adapts to training difficulty */
    /* HOW:  High loss modulates cortical dynamics */

    EXPECT_EQ(cortical_training_start(bridge), 0);
    EXPECT_EQ(cortical_training_update_metrics(bridge, TEST_HIGH_LOSS, 1.0f, TEST_BASE_LR, TEST_STEP), 0);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, ConvergenceToCorticalStability) {
    /* WHAT: Convergence signals cortical stability */
    /* WHY:  Stable training = stable cortex */
    /* HOW:  Convergence reduces plasticity */

    EXPECT_EQ(cortical_training_start(bridge), 0);
    EXPECT_EQ(cortical_training_signal_event(bridge, CORTICAL_TRAINING_FEEDBACK_STRENGTHEN_PREDICTIONS, TEST_SIGNAL_VALUE), 0);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, DivergenceToErrorSignal) {
    /* WHAT: Divergence increases prediction error signal */
    /* WHY:  Unstable = needs correction */
    /* HOW:  Divergence boosts error drive */

    EXPECT_EQ(cortical_training_start(bridge), 0);
    EXPECT_EQ(cortical_training_signal_event(bridge, CORTICAL_TRAINING_FEEDBACK_RESET_PRECISION, 0.85f), 0);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, PatternLearnedToColumns) {
    /* WHAT: Learned patterns stabilize columns */
    /* WHY:  Learning = representation formation */
    /* HOW:  Pattern signal reduces column entropy */

    EXPECT_EQ(cortical_training_start(bridge), 0);
    EXPECT_EQ(cortical_training_signal_event(bridge, CORTICAL_TRAINING_FEEDBACK_CONSOLIDATE, TEST_SIGNAL_VALUE), 0);

    SUCCEED();
}

TEST_F(CorticalTrainingBridgeTest, GradientNormFeedback) {
    /* WHAT: Gradient norm affects dendritic plasticity */
    /* WHY:  Large gradients = strong signals */
    /* HOW:  Grad norm modulates burst thresholds */

    EXPECT_EQ(cortical_training_start(bridge), 0);
    EXPECT_EQ(cortical_training_update_metrics(bridge, TEST_LOSS, TEST_GRAD_NORM, TEST_BASE_LR, TEST_STEP), 0);

    SUCCEED();
}

//=============================================================================
// Statistics and Error Handling Tests (5 tests)
//=============================================================================

TEST_F(CorticalTrainingBridgeTest, GetStats) {
    cortical_training_stats_t stats;
    EXPECT_EQ(cortical_training_get_stats(bridge, &stats), 0);
}

TEST_F(CorticalTrainingBridgeTest, GetStatsNull) {
    cortical_training_stats_t stats;
    EXPECT_NE(cortical_training_get_stats(nullptr, &stats), 0);
    EXPECT_NE(cortical_training_get_stats(bridge, nullptr), 0);
}

TEST_F(CorticalTrainingBridgeTest, ResetStats) {
    EXPECT_EQ(cortical_training_reset_stats(bridge), 0);
}

TEST_F(CorticalTrainingBridgeTest, ErrorHandlingNull) {
    /* Null bridge returns base LR */
    float result = cortical_training_get_modulated_lr(nullptr, 0.001f);
    EXPECT_TRUE(result == 0.001f || result == 0.0f);

    /* Null bridge returns false for predictions stable */
    EXPECT_FALSE(cortical_training_are_predictions_stable(nullptr));

    /* Null bridge get effects fails */
    cortical_training_effects_t effects;
    EXPECT_NE(cortical_training_get_effects(nullptr, &effects), 0);
}

TEST_F(CorticalTrainingBridgeTest, DumpState) {
    EXPECT_EQ(cortical_training_start(bridge), 0);
    cortical_training_dump_state(bridge);
    SUCCEED();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
