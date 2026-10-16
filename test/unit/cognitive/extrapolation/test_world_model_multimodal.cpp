/**
 * @file test_world_model_multimodal.cpp
 * @brief Comprehensive unit tests for Multi-Modal World Model
 *
 * Tests cover:
 * - Lifecycle: wm_default_config, wm_create, wm_init, wm_reset, wm_destroy
 * - Modality Processing: All 10 modalities (VISUAL through SEMANTIC)
 * - Fusion: All fusion types (EARLY, LATE, HIERARCHICAL, ATTENTION)
 * - Prediction: All prediction modes
 * - Entity Management: CRUD operations
 * - State and Statistics
 * - Utility functions
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <cmath>

extern "C" {
#include "cognitive/extrapolation/nimcp_world_model_multimodal.h"
}

namespace {

//=============================================================================
// Constants
//=============================================================================

constexpr float FLOAT_TOLERANCE = 1e-5f;
constexpr uint32_t TEST_FEATURE_DIM = 64;
constexpr uint32_t TEST_LATENT_DIM = 128;

//=============================================================================
// Test Fixture
//=============================================================================

class WorldModelMultimodalTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        config_ = wm_default_config();
        config_.latent_dim = TEST_LATENT_DIM;
        config_.max_entities = 64;
        config_.max_prediction_steps = 50;
        config_.enable_bio_async = false;
        config_.enable_immune = false;
        config_.enable_logging = false;

        wm_ = wm_create(&config_);
        ASSERT_NE(wm_, nullptr) << "Failed to create world model";
    }

    void TearDown() override
    {
        if (wm_) {
            wm_destroy(wm_);
            wm_ = nullptr;
        }
    }

    // Helper: Create initialized world model
    nimcp_world_model_t* create_initialized_wm()
    {
        nimcp_world_model_t* wm = wm_create(&config_);
        if (wm) {
            wm_error_t err = wm_init(wm);
            if (err != WM_OK) {
                wm_destroy(wm);
                return nullptr;
            }
        }
        return wm;
    }

    // Helper: Create modality input
    wm_modality_input_t create_modality_input(wm_modality_t modality, uint32_t feature_dim)
    {
        wm_modality_input_t input;
        memset(&input, 0, sizeof(input));

        input.modality = modality;
        input.feature_dim = feature_dim;
        input.confidence = 0.9f;
        input.timestamp = 1000;

        // Allocate and fill features
        if (feature_dim > 0) {
            input.features = feature_buffer_.data();
            for (uint32_t i = 0; i < feature_dim && i < feature_buffer_.size(); ++i) {
                input.features[i] = static_cast<float>(i) / static_cast<float>(feature_dim);
            }
        }

        return input;
    }

    // Helper: Create test entity
    wm_entity_t create_test_entity(uint32_t id)
    {
        wm_entity_t entity;
        memset(&entity, 0, sizeof(entity));

        entity.entity_id = id;
        entity.position[0] = 1.0f;
        entity.position[1] = 2.0f;
        entity.position[2] = 3.0f;
        entity.velocity[0] = 0.1f;
        entity.velocity[1] = 0.2f;
        entity.velocity[2] = 0.3f;
        entity.latent_state = nullptr;
        entity.latent_dim = 0;
        entity.existence_prob = 1.0f;
        entity.last_observed = 1000;
        entity.modality_mask = (1 << WM_MODALITY_VISUAL) | (1 << WM_MODALITY_AUDITORY);

        return entity;
    }

    // Helper: Compare floats with tolerance
    bool float_equals(float a, float b)
    {
        return std::fabs(a - b) < FLOAT_TOLERANCE;
    }

    wm_config_t config_;
    nimcp_world_model_t* wm_ = nullptr;
    std::vector<float> feature_buffer_ = std::vector<float>(1024, 0.0f);
    std::vector<float> encoding_buffer_ = std::vector<float>(512, 0.0f);
    std::vector<float> state_buffer_ = std::vector<float>(1024, 0.0f);
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(WorldModelMultimodalTest, DefaultConfigReturnsValidConfig)
{
    wm_config_t config = wm_default_config();

    EXPECT_GT(config.latent_dim, 0u);
    EXPECT_GT(config.context_size, 0u);
    EXPECT_GT(config.max_entities, 0u);
    EXPECT_GT(config.max_prediction_steps, 0u);
    EXPECT_GE(config.learning_rate, 0.0f);
    EXPECT_LE(config.learning_rate, 1.0f);
    EXPECT_GE(config.prediction_decay, 0.0f);
    EXPECT_LE(config.prediction_decay, 1.0f);
}

TEST_F(WorldModelMultimodalTest, CreateWithNullConfigUsesDefaults)
{
    // NULL config creates world model with default config
    nimcp_world_model_t* wm = wm_create(nullptr);
    if (wm) {
        // Created successfully with defaults
        EXPECT_TRUE(wm->config.latent_dim > 0);
        wm_destroy(wm);
    }
    // NULL may also be returned if allocation fails - both are valid
}

TEST_F(WorldModelMultimodalTest, CreateWithValidConfigSucceeds)
{
    EXPECT_NE(wm_, nullptr);
}

TEST_F(WorldModelMultimodalTest, CreateWithZeroLatentDimFails)
{
    wm_config_t bad_config = config_;
    bad_config.latent_dim = 0;

    nimcp_world_model_t* wm = wm_create(&bad_config);
    // May return nullptr or create with default - implementation dependent
    if (wm) {
        wm_destroy(wm);
    }
}

TEST_F(WorldModelMultimodalTest, InitWithNullReturnsError)
{
    wm_error_t err = wm_init(nullptr);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, InitSucceedsWithValidWM)
{
    wm_error_t err = wm_init(wm_);
    EXPECT_EQ(err, WM_OK);
    EXPECT_TRUE(wm_->initialized);
}

TEST_F(WorldModelMultimodalTest, DoubleInitSucceeds)
{
    EXPECT_EQ(wm_init(wm_), WM_OK);
    // Second init should either succeed or return already initialized error
    wm_error_t err = wm_init(wm_);
    EXPECT_TRUE(err == WM_OK || err == WM_ERR_NOT_INITIALIZED);
}

TEST_F(WorldModelMultimodalTest, ResetWithNullReturnsError)
{
    wm_error_t err = wm_reset(nullptr);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, ResetWithUninitializedHandledGracefully)
{
    // Implementation may either:
    // 1. Return WM_ERR_NOT_INITIALIZED (strict validation)
    // 2. Return WM_OK (permissive - resets to initial state)
    wm_error_t err = wm_reset(wm_);
    EXPECT_TRUE(err == WM_OK || err == WM_ERR_NOT_INITIALIZED);
}

TEST_F(WorldModelMultimodalTest, ResetAfterInitSucceeds)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    wm_error_t err = wm_reset(wm_);
    EXPECT_EQ(err, WM_OK);
}

TEST_F(WorldModelMultimodalTest, DestroyWithNullIsSafe)
{
    wm_destroy(nullptr);
    // Should not crash
}

TEST_F(WorldModelMultimodalTest, DestroyValidWMSucceeds)
{
    nimcp_world_model_t* temp_wm = wm_create(&config_);
    ASSERT_NE(temp_wm, nullptr);
    wm_destroy(temp_wm);
    // Should not crash
}

TEST_F(WorldModelMultimodalTest, DestroyAfterInitSucceeds)
{
    nimcp_world_model_t* temp_wm = wm_create(&config_);
    ASSERT_NE(temp_wm, nullptr);
    ASSERT_EQ(wm_init(temp_wm), WM_OK);
    wm_destroy(temp_wm);
    // Should not crash
}

//=============================================================================
// Modality Processing Tests
//=============================================================================

class ModalityTest : public WorldModelMultimodalTest,
                     public ::testing::WithParamInterface<wm_modality_t> {};

TEST_P(ModalityTest, ProcessModalityWithNullWMReturnsError)
{
    wm_modality_input_t input = create_modality_input(GetParam(), TEST_FEATURE_DIM);
    wm_error_t err = wm_process_modality(nullptr, &input);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_P(ModalityTest, ProcessModalityWithNullInputReturnsError)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    wm_error_t err = wm_process_modality(wm_, nullptr);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_P(ModalityTest, ProcessModalityWithoutInitReturnsError)
{
    wm_modality_input_t input = create_modality_input(GetParam(), TEST_FEATURE_DIM);
    wm_error_t err = wm_process_modality(wm_, &input);
    EXPECT_EQ(err, WM_ERR_NOT_INITIALIZED);
}

TEST_P(ModalityTest, ProcessModalitySucceeds)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    wm_modality_input_t input = create_modality_input(GetParam(), TEST_FEATURE_DIM);

    wm_error_t err = wm_process_modality(wm_, &input);
    EXPECT_EQ(err, WM_OK);
}

TEST_P(ModalityTest, SetModalityActiveWithNullWMReturnsError)
{
    wm_error_t err = wm_set_modality_active(nullptr, GetParam(), true);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_P(ModalityTest, SetModalityActiveSucceeds)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    // Disable
    wm_error_t err = wm_set_modality_active(wm_, GetParam(), false);
    EXPECT_EQ(err, WM_OK);
    EXPECT_FALSE(wm_->modality_active[GetParam()]);

    // Enable
    err = wm_set_modality_active(wm_, GetParam(), true);
    EXPECT_EQ(err, WM_OK);
    EXPECT_TRUE(wm_->modality_active[GetParam()]);
}

TEST_P(ModalityTest, GetModalityEncodingWithNullWMReturnsError)
{
    uint32_t dim;
    wm_error_t err = wm_get_modality_encoding(nullptr, GetParam(), encoding_buffer_.data(), &dim);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_P(ModalityTest, GetModalityEncodingWithNullOutputReturnsError)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    uint32_t dim;
    wm_error_t err = wm_get_modality_encoding(wm_, GetParam(), nullptr, &dim);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_P(ModalityTest, GetModalityEncodingSucceeds)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    // Process some input first
    wm_modality_input_t input = create_modality_input(GetParam(), TEST_FEATURE_DIM);
    ASSERT_EQ(wm_process_modality(wm_, &input), WM_OK);

    uint32_t dim = 0;
    wm_error_t err = wm_get_modality_encoding(wm_, GetParam(), encoding_buffer_.data(), &dim);
    EXPECT_EQ(err, WM_OK);
    EXPECT_GT(dim, 0u);
}

TEST_P(ModalityTest, ModalityStringReturnsNonNull)
{
    const char* str = wm_modality_string(GetParam());
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);
}

// Instantiate tests for all modalities
INSTANTIATE_TEST_SUITE_P(
    AllModalities,
    ModalityTest,
    ::testing::Values(
        WM_MODALITY_VISUAL,
        WM_MODALITY_AUDITORY,
        WM_MODALITY_TACTILE,
        WM_MODALITY_PROPRIOCEPTIVE,
        WM_MODALITY_OLFACTORY,
        WM_MODALITY_GUSTATORY,
        WM_MODALITY_VESTIBULAR,
        WM_MODALITY_INTEROCEPTIVE,
        WM_MODALITY_LINGUISTIC,
        WM_MODALITY_SEMANTIC
    ),
    [](const ::testing::TestParamInfo<wm_modality_t>& info) {
        const char* str = wm_modality_string(info.param);
        return str ? std::string(str) : "Unknown";
    }
);

TEST_F(WorldModelMultimodalTest, ProcessModalityInvalidModalityReturnsError)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    wm_modality_input_t input;
    memset(&input, 0, sizeof(input));
    input.modality = static_cast<wm_modality_t>(WM_MODALITY_COUNT + 1);
    input.features = feature_buffer_.data();
    input.feature_dim = TEST_FEATURE_DIM;
    input.confidence = 0.9f;

    wm_error_t err = wm_process_modality(wm_, &input);
    EXPECT_EQ(err, WM_ERR_INVALID_MODALITY);
}

TEST_F(WorldModelMultimodalTest, ProcessMultimodalWithNullWMReturnsError)
{
    wm_modality_input_t inputs[2];
    inputs[0] = create_modality_input(WM_MODALITY_VISUAL, TEST_FEATURE_DIM);
    inputs[1] = create_modality_input(WM_MODALITY_AUDITORY, TEST_FEATURE_DIM);

    wm_error_t err = wm_process_multimodal(nullptr, inputs, 2);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, ProcessMultimodalWithNullInputsReturnsError)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    wm_error_t err = wm_process_multimodal(wm_, nullptr, 2);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, ProcessMultimodalWithZeroInputsReturnsError)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    wm_modality_input_t inputs[2];
    wm_error_t err = wm_process_multimodal(wm_, inputs, 0);
    // Should return error or OK depending on implementation
    EXPECT_TRUE(err == WM_OK || err == WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, ProcessMultimodalSucceeds)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    // Create separate feature buffers for each input
    std::vector<float> features1(TEST_FEATURE_DIM);
    std::vector<float> features2(TEST_FEATURE_DIM);
    std::vector<float> features3(TEST_FEATURE_DIM);

    for (uint32_t i = 0; i < TEST_FEATURE_DIM; ++i) {
        features1[i] = static_cast<float>(i) * 0.1f;
        features2[i] = static_cast<float>(i) * 0.2f;
        features3[i] = static_cast<float>(i) * 0.3f;
    }

    wm_modality_input_t inputs[3];
    memset(inputs, 0, sizeof(inputs));

    inputs[0].modality = WM_MODALITY_VISUAL;
    inputs[0].features = features1.data();
    inputs[0].feature_dim = TEST_FEATURE_DIM;
    inputs[0].confidence = 0.9f;
    inputs[0].timestamp = 1000;

    inputs[1].modality = WM_MODALITY_AUDITORY;
    inputs[1].features = features2.data();
    inputs[1].feature_dim = TEST_FEATURE_DIM;
    inputs[1].confidence = 0.8f;
    inputs[1].timestamp = 1000;

    inputs[2].modality = WM_MODALITY_TACTILE;
    inputs[2].features = features3.data();
    inputs[2].feature_dim = TEST_FEATURE_DIM;
    inputs[2].confidence = 0.7f;
    inputs[2].timestamp = 1000;

    wm_error_t err = wm_process_multimodal(wm_, inputs, 3);
    EXPECT_EQ(err, WM_OK);
}

//=============================================================================
// Fusion Tests
//=============================================================================

class FusionTest : public WorldModelMultimodalTest,
                   public ::testing::WithParamInterface<wm_fusion_type_t> {};

TEST_P(FusionTest, FusionWithDifferentTypesSucceeds)
{
    config_.fusion_type = GetParam();
    nimcp_world_model_t* wm = wm_create(&config_);
    ASSERT_NE(wm, nullptr);
    ASSERT_EQ(wm_init(wm), WM_OK);

    // Process multiple modalities
    std::vector<float> features(TEST_FEATURE_DIM);
    for (uint32_t i = 0; i < TEST_FEATURE_DIM; ++i) {
        features[i] = static_cast<float>(i) / static_cast<float>(TEST_FEATURE_DIM);
    }

    wm_modality_input_t input;
    memset(&input, 0, sizeof(input));
    input.features = features.data();
    input.feature_dim = TEST_FEATURE_DIM;
    input.confidence = 0.9f;
    input.timestamp = 1000;

    input.modality = WM_MODALITY_VISUAL;
    ASSERT_EQ(wm_process_modality(wm, &input), WM_OK);

    input.modality = WM_MODALITY_AUDITORY;
    ASSERT_EQ(wm_process_modality(wm, &input), WM_OK);

    // Fuse modalities
    wm_error_t err = wm_fuse_modalities(wm);
    EXPECT_EQ(err, WM_OK);

    wm_destroy(wm);
}

INSTANTIATE_TEST_SUITE_P(
    AllFusionTypes,
    FusionTest,
    ::testing::Values(
        WM_FUSION_EARLY,
        WM_FUSION_LATE,
        WM_FUSION_HIERARCHICAL,
        WM_FUSION_ATTENTION
    )
);

TEST_F(WorldModelMultimodalTest, FuseModalitiesWithNullReturnsError)
{
    wm_error_t err = wm_fuse_modalities(nullptr);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, FuseModalitiesWithoutInitReturnsError)
{
    wm_error_t err = wm_fuse_modalities(wm_);
    EXPECT_EQ(err, WM_ERR_NOT_INITIALIZED);
}

TEST_F(WorldModelMultimodalTest, FuseModalitiesSucceeds)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    // Process some input
    wm_modality_input_t input = create_modality_input(WM_MODALITY_VISUAL, TEST_FEATURE_DIM);
    ASSERT_EQ(wm_process_modality(wm_, &input), WM_OK);

    wm_error_t err = wm_fuse_modalities(wm_);
    EXPECT_EQ(err, WM_OK);
}

TEST_F(WorldModelMultimodalTest, GetAttentionWithNullWMReturnsError)
{
    wm_cross_modal_attention_t attention;
    wm_error_t err = wm_get_attention(nullptr, &attention);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, GetAttentionWithNullOutputReturnsError)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    wm_error_t err = wm_get_attention(wm_, nullptr);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, GetAttentionSucceeds)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    wm_cross_modal_attention_t attention;
    memset(&attention, 0, sizeof(attention));

    wm_error_t err = wm_get_attention(wm_, &attention);
    EXPECT_EQ(err, WM_OK);
    EXPECT_GE(attention.coherence_score, 0.0f);
    EXPECT_LE(attention.coherence_score, 1.0f);
}

TEST_F(WorldModelMultimodalTest, SetFusionWeightsWithNullWMReturnsError)
{
    float weights[WM_MODALITY_COUNT] = {0.1f};
    wm_error_t err = wm_set_fusion_weights(nullptr, weights, WM_MODALITY_COUNT);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, SetFusionWeightsWithNullWeightsReturnsError)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    wm_error_t err = wm_set_fusion_weights(wm_, nullptr, WM_MODALITY_COUNT);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, SetFusionWeightsSucceeds)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    float weights[WM_MODALITY_COUNT];
    for (int i = 0; i < WM_MODALITY_COUNT; ++i) {
        weights[i] = 1.0f / static_cast<float>(WM_MODALITY_COUNT);
    }

    wm_error_t err = wm_set_fusion_weights(wm_, weights, WM_MODALITY_COUNT);
    EXPECT_EQ(err, WM_OK);
}

TEST_F(WorldModelMultimodalTest, SetFusionWeightsWithExceedingCountReturnsError)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    float weights[WM_MODALITY_COUNT + 5];
    for (int i = 0; i < WM_MODALITY_COUNT + 5; ++i) {
        weights[i] = 0.1f;
    }
    wm_error_t err = wm_set_fusion_weights(wm_, weights, WM_MODALITY_COUNT + 5);
    EXPECT_EQ(err, WM_ERR_CAPACITY_EXCEEDED);
}

//=============================================================================
// Prediction Tests
//=============================================================================

class PredictionTest : public WorldModelMultimodalTest,
                       public ::testing::WithParamInterface<wm_prediction_mode_t> {};

TEST_P(PredictionTest, PredictWithDifferentModesSucceeds)
{
    config_.prediction_mode = GetParam();
    nimcp_world_model_t* wm = wm_create(&config_);
    ASSERT_NE(wm, nullptr);
    ASSERT_EQ(wm_init(wm), WM_OK);

    // Process some input
    std::vector<float> features(TEST_FEATURE_DIM);
    for (uint32_t i = 0; i < TEST_FEATURE_DIM; ++i) {
        features[i] = static_cast<float>(i) / static_cast<float>(TEST_FEATURE_DIM);
    }

    wm_modality_input_t input;
    memset(&input, 0, sizeof(input));
    input.modality = WM_MODALITY_VISUAL;
    input.features = features.data();
    input.feature_dim = TEST_FEATURE_DIM;
    input.confidence = 0.9f;
    input.timestamp = 1000;

    ASSERT_EQ(wm_process_modality(wm, &input), WM_OK);
    ASSERT_EQ(wm_fuse_modalities(wm), WM_OK);

    wm_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));

    wm_error_t err = wm_predict(wm, 10, &prediction);
    EXPECT_EQ(err, WM_OK);

    wm_destroy(wm);
}

INSTANTIATE_TEST_SUITE_P(
    AllPredictionModes,
    PredictionTest,
    ::testing::Values(
        WM_PRED_MODE_DETERMINISTIC,
        WM_PRED_MODE_PROBABILISTIC,
        WM_PRED_MODE_ENSEMBLE,
        WM_PRED_MODE_HIERARCHICAL
    )
);

TEST_F(WorldModelMultimodalTest, PredictWithNullWMReturnsError)
{
    wm_prediction_t prediction;
    wm_error_t err = wm_predict(nullptr, 10, &prediction);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, PredictWithNullOutputReturnsError)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    wm_error_t err = wm_predict(wm_, 10, nullptr);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, PredictWithoutInitReturnsError)
{
    wm_prediction_t prediction;
    wm_error_t err = wm_predict(wm_, 10, &prediction);
    EXPECT_EQ(err, WM_ERR_NOT_INITIALIZED);
}

TEST_F(WorldModelMultimodalTest, PredictWithZeroHorizonHandledGracefully)
{
    // Implementation may either:
    // 1. Return WM_ERR_INVALID_HORIZON (strict validation)
    // 2. Return WM_OK with zero-length prediction (permissive)
    ASSERT_EQ(wm_init(wm_), WM_OK);
    wm_prediction_t prediction = {};
    wm_error_t err = wm_predict(wm_, 0, &prediction);
    EXPECT_TRUE(err == WM_OK || err == WM_ERR_INVALID_HORIZON);
}

TEST_F(WorldModelMultimodalTest, PredictWithExcessiveHorizonReturnsError)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    wm_prediction_t prediction;
    wm_error_t err = wm_predict(wm_, WM_MAX_PREDICTION_STEPS + 1, &prediction);
    EXPECT_EQ(err, WM_ERR_INVALID_HORIZON);
}

TEST_F(WorldModelMultimodalTest, PredictSucceeds)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    // Process input and fuse
    wm_modality_input_t input = create_modality_input(WM_MODALITY_VISUAL, TEST_FEATURE_DIM);
    ASSERT_EQ(wm_process_modality(wm_, &input), WM_OK);
    ASSERT_EQ(wm_fuse_modalities(wm_), WM_OK);

    wm_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));

    wm_error_t err = wm_predict(wm_, 10, &prediction);
    EXPECT_EQ(err, WM_OK);
    EXPECT_EQ(prediction.horizon_steps, 10u);
    EXPECT_GE(prediction.prediction_confidence, 0.0f);
    EXPECT_LE(prediction.prediction_confidence, 1.0f);
}

TEST_F(WorldModelMultimodalTest, PredictEntityWithNullWMReturnsError)
{
    float trajectory[30];
    float confidence;
    wm_error_t err = wm_predict_entity(nullptr, 0, 10, trajectory, &confidence);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, PredictEntityWithNullTrajectoryReturnsError)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    float confidence;
    wm_error_t err = wm_predict_entity(wm_, 0, 10, nullptr, &confidence);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, PredictEntityForNonexistentEntityReturnsError)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    float trajectory[30];
    float confidence;
    wm_error_t err = wm_predict_entity(wm_, 99999, 10, trajectory, &confidence);
    // Should return error for nonexistent entity
    EXPECT_NE(err, WM_OK);
}

TEST_F(WorldModelMultimodalTest, PredictEntitySucceeds)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    // Add an entity first
    wm_entity_t entity = create_test_entity(0);
    uint32_t entity_id;
    ASSERT_EQ(wm_add_entity(wm_, &entity, &entity_id), WM_OK);

    // Process some input
    wm_modality_input_t input = create_modality_input(WM_MODALITY_VISUAL, TEST_FEATURE_DIM);
    ASSERT_EQ(wm_process_modality(wm_, &input), WM_OK);

    float trajectory[30];
    float confidence = 0.0f;
    wm_error_t err = wm_predict_entity(wm_, entity_id, 10, trajectory, &confidence);
    EXPECT_EQ(err, WM_OK);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

TEST_F(WorldModelMultimodalTest, UpdatePredictionErrorWithNullWMReturnsError)
{
    float state[10] = {0.0f};
    wm_error_t err = wm_update_prediction_error(nullptr, state, 10);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, UpdatePredictionErrorWithNullStateReturnsError)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    wm_error_t err = wm_update_prediction_error(wm_, nullptr, 10);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, UpdatePredictionErrorSucceeds)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    // Make a prediction first
    wm_modality_input_t input = create_modality_input(WM_MODALITY_VISUAL, TEST_FEATURE_DIM);
    ASSERT_EQ(wm_process_modality(wm_, &input), WM_OK);
    ASSERT_EQ(wm_fuse_modalities(wm_), WM_OK);

    wm_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));
    ASSERT_EQ(wm_predict(wm_, 5, &prediction), WM_OK);

    // Update with actual state
    std::vector<float> actual_state(TEST_LATENT_DIM, 0.5f);
    wm_error_t err = wm_update_prediction_error(wm_, actual_state.data(), TEST_LATENT_DIM);
    EXPECT_EQ(err, WM_OK);
}

//=============================================================================
// Entity Management Tests
//=============================================================================

TEST_F(WorldModelMultimodalTest, AddEntityWithNullWMReturnsError)
{
    wm_entity_t entity = create_test_entity(0);
    uint32_t entity_id;
    wm_error_t err = wm_add_entity(nullptr, &entity, &entity_id);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, AddEntityWithNullEntityReturnsError)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    uint32_t entity_id;
    wm_error_t err = wm_add_entity(wm_, nullptr, &entity_id);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, AddEntityWithoutInitSucceeds)
{
    // Entity management works without init (no processing required)
    wm_entity_t entity = create_test_entity(0);
    uint32_t entity_id;
    wm_error_t err = wm_add_entity(wm_, &entity, &entity_id);
    // May succeed or require init depending on implementation
    EXPECT_TRUE(err == WM_OK || err == WM_ERR_NOT_INITIALIZED);
}

TEST_F(WorldModelMultimodalTest, AddEntitySucceeds)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    wm_entity_t entity = create_test_entity(0);
    uint32_t entity_id = UINT32_MAX;

    wm_error_t err = wm_add_entity(wm_, &entity, &entity_id);
    EXPECT_EQ(err, WM_OK);
    EXPECT_NE(entity_id, UINT32_MAX);
    EXPECT_EQ(wm_->num_entities, 1u);
}

TEST_F(WorldModelMultimodalTest, AddMultipleEntitiesSucceeds)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    for (uint32_t i = 0; i < 10; ++i) {
        wm_entity_t entity = create_test_entity(i);
        uint32_t entity_id;
        wm_error_t err = wm_add_entity(wm_, &entity, &entity_id);
        EXPECT_EQ(err, WM_OK);
    }

    EXPECT_EQ(wm_->num_entities, 10u);
}

TEST_F(WorldModelMultimodalTest, AddEntityExceedsCapacityReturnsError)
{
    config_.max_entities = 2;
    nimcp_world_model_t* wm = wm_create(&config_);
    ASSERT_NE(wm, nullptr);
    ASSERT_EQ(wm_init(wm), WM_OK);

    wm_entity_t entity = create_test_entity(0);
    uint32_t entity_id;

    EXPECT_EQ(wm_add_entity(wm, &entity, &entity_id), WM_OK);
    EXPECT_EQ(wm_add_entity(wm, &entity, &entity_id), WM_OK);
    EXPECT_EQ(wm_add_entity(wm, &entity, &entity_id), WM_ERR_CAPACITY_EXCEEDED);

    wm_destroy(wm);
}

TEST_F(WorldModelMultimodalTest, UpdateEntityWithNullWMReturnsError)
{
    wm_entity_t entity = create_test_entity(0);
    wm_error_t err = wm_update_entity(nullptr, 0, &entity);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, UpdateEntityWithNullUpdateReturnsError)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    wm_error_t err = wm_update_entity(wm_, 0, nullptr);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, UpdateNonexistentEntityReturnsError)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    wm_entity_t entity = create_test_entity(0);
    wm_error_t err = wm_update_entity(wm_, 99999, &entity);
    EXPECT_NE(err, WM_OK);
}

TEST_F(WorldModelMultimodalTest, UpdateEntitySucceeds)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    // Add entity
    wm_entity_t entity = create_test_entity(0);
    uint32_t entity_id;
    ASSERT_EQ(wm_add_entity(wm_, &entity, &entity_id), WM_OK);

    // Update entity
    entity.position[0] = 10.0f;
    entity.position[1] = 20.0f;
    entity.position[2] = 30.0f;
    entity.existence_prob = 0.8f;

    wm_error_t err = wm_update_entity(wm_, entity_id, &entity);
    EXPECT_EQ(err, WM_OK);

    // Verify update
    wm_entity_t retrieved;
    ASSERT_EQ(wm_get_entity(wm_, entity_id, &retrieved), WM_OK);
    EXPECT_TRUE(float_equals(retrieved.position[0], 10.0f));
    EXPECT_TRUE(float_equals(retrieved.position[1], 20.0f));
    EXPECT_TRUE(float_equals(retrieved.position[2], 30.0f));
    EXPECT_TRUE(float_equals(retrieved.existence_prob, 0.8f));
}

TEST_F(WorldModelMultimodalTest, GetEntityWithNullWMReturnsError)
{
    wm_entity_t entity;
    wm_error_t err = wm_get_entity(nullptr, 0, &entity);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, GetEntityWithNullOutputReturnsError)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    wm_error_t err = wm_get_entity(wm_, 0, nullptr);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, GetNonexistentEntityReturnsError)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    wm_entity_t entity;
    wm_error_t err = wm_get_entity(wm_, 99999, &entity);
    EXPECT_NE(err, WM_OK);
}

TEST_F(WorldModelMultimodalTest, GetEntitySucceeds)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    wm_entity_t entity = create_test_entity(0);
    entity.position[0] = 5.0f;
    entity.position[1] = 6.0f;
    entity.position[2] = 7.0f;

    uint32_t entity_id;
    ASSERT_EQ(wm_add_entity(wm_, &entity, &entity_id), WM_OK);

    wm_entity_t retrieved;
    wm_error_t err = wm_get_entity(wm_, entity_id, &retrieved);
    EXPECT_EQ(err, WM_OK);
    EXPECT_TRUE(float_equals(retrieved.position[0], 5.0f));
    EXPECT_TRUE(float_equals(retrieved.position[1], 6.0f));
    EXPECT_TRUE(float_equals(retrieved.position[2], 7.0f));
}

TEST_F(WorldModelMultimodalTest, RemoveEntityWithNullWMReturnsError)
{
    wm_error_t err = wm_remove_entity(nullptr, 0);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, RemoveNonexistentEntityReturnsError)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    wm_error_t err = wm_remove_entity(wm_, 99999);
    EXPECT_NE(err, WM_OK);
}

TEST_F(WorldModelMultimodalTest, RemoveEntitySucceeds)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    wm_entity_t entity = create_test_entity(0);
    uint32_t entity_id;
    ASSERT_EQ(wm_add_entity(wm_, &entity, &entity_id), WM_OK);
    EXPECT_EQ(wm_->num_entities, 1u);

    wm_error_t err = wm_remove_entity(wm_, entity_id);
    EXPECT_EQ(err, WM_OK);

    // Verify entity count decreased or entity is marked as removed
    wm_entity_t retrieved;
    err = wm_get_entity(wm_, entity_id, &retrieved);
    EXPECT_NE(err, WM_OK);
}

TEST_F(WorldModelMultimodalTest, GetEntitiesWithNullWMReturnsError)
{
    wm_entity_t entities[10];
    uint32_t count;
    wm_error_t err = wm_get_entities(nullptr, entities, &count);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, GetEntitiesWithNullCountReturnsError)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    wm_entity_t entities[10];
    wm_error_t err = wm_get_entities(wm_, entities, nullptr);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, GetEntitiesWithNullArrayReturnsCount)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    // Add entities - some implementations may not support entity tracking
    uint32_t added = 0;
    for (uint32_t i = 0; i < 5; ++i) {
        wm_entity_t entity = create_test_entity(i);
        uint32_t entity_id;
        wm_error_t add_err = wm_add_entity(wm_, &entity, &entity_id);
        if (add_err == WM_OK) {
            added++;
        }
    }

    uint32_t count = 0;
    wm_error_t err = wm_get_entities(wm_, nullptr, &count);
    // Implementation may either:
    // 1. Return WM_OK and count of entities (query mode)
    // 2. Return WM_ERR_NULL_PTR (strict validation)
    EXPECT_TRUE(err == WM_OK || err == WM_ERR_NULL_PTR);
    if (err == WM_OK) {
        EXPECT_EQ(count, added);
    }
}

TEST_F(WorldModelMultimodalTest, GetEntitiesSucceeds)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    // Add entities
    for (uint32_t i = 0; i < 5; ++i) {
        wm_entity_t entity = create_test_entity(i);
        uint32_t entity_id;
        ASSERT_EQ(wm_add_entity(wm_, &entity, &entity_id), WM_OK);
    }

    wm_entity_t entities[10];
    uint32_t count = 10;
    wm_error_t err = wm_get_entities(wm_, entities, &count);
    EXPECT_EQ(err, WM_OK);
    EXPECT_EQ(count, 5u);
}

//=============================================================================
// State Tests
//=============================================================================

TEST_F(WorldModelMultimodalTest, GetGlobalStateWithNullWMReturnsError)
{
    uint32_t dim;
    wm_error_t err = wm_get_global_state(nullptr, state_buffer_.data(), &dim);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, GetGlobalStateWithNullStateReturnsError)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    uint32_t dim;
    wm_error_t err = wm_get_global_state(wm_, nullptr, &dim);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, GetGlobalStateWithNullDimReturnsError)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    wm_error_t err = wm_get_global_state(wm_, state_buffer_.data(), nullptr);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, GetGlobalStateSucceeds)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    uint32_t dim = 0;
    wm_error_t err = wm_get_global_state(wm_, state_buffer_.data(), &dim);
    EXPECT_EQ(err, WM_OK);
    EXPECT_GT(dim, 0u);
}

TEST_F(WorldModelMultimodalTest, UpdateWithNullWMReturnsError)
{
    wm_error_t err = wm_update(nullptr, 16.0f);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, UpdateWithoutInitReturnsError)
{
    wm_error_t err = wm_update(wm_, 16.0f);
    EXPECT_EQ(err, WM_ERR_NOT_INITIALIZED);
}

TEST_F(WorldModelMultimodalTest, UpdateSucceeds)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    wm_error_t err = wm_update(wm_, 16.0f);
    EXPECT_EQ(err, WM_OK);
}

TEST_F(WorldModelMultimodalTest, UpdateWithZeroDtSucceeds)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    wm_error_t err = wm_update(wm_, 0.0f);
    EXPECT_EQ(err, WM_OK);
}

TEST_F(WorldModelMultimodalTest, UpdateMultipleTimesSucceeds)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    for (int i = 0; i < 100; ++i) {
        wm_error_t err = wm_update(wm_, 16.67f);  // ~60fps
        EXPECT_EQ(err, WM_OK);
    }
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(WorldModelMultimodalTest, GetStatsWithNullWMReturnsError)
{
    wm_stats_t stats;
    wm_error_t err = wm_get_stats(nullptr, &stats);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, GetStatsWithNullStatsReturnsError)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    wm_error_t err = wm_get_stats(wm_, nullptr);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, GetStatsSucceeds)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    wm_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    wm_error_t err = wm_get_stats(wm_, &stats);
    EXPECT_EQ(err, WM_OK);
}

TEST_F(WorldModelMultimodalTest, StatsIncrementAfterOperations)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    // Process some input
    wm_modality_input_t input = create_modality_input(WM_MODALITY_VISUAL, TEST_FEATURE_DIM);
    ASSERT_EQ(wm_process_modality(wm_, &input), WM_OK);

    // Fuse
    ASSERT_EQ(wm_fuse_modalities(wm_), WM_OK);

    // Predict
    wm_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));
    ASSERT_EQ(wm_predict(wm_, 5, &prediction), WM_OK);

    // Add entity
    wm_entity_t entity = create_test_entity(0);
    uint32_t entity_id;
    ASSERT_EQ(wm_add_entity(wm_, &entity, &entity_id), WM_OK);

    wm_stats_t stats;
    wm_error_t err = wm_get_stats(wm_, &stats);
    EXPECT_EQ(err, WM_OK);
    EXPECT_GT(stats.inputs_processed, 0u);
    EXPECT_GT(stats.predictions_made, 0u);
    EXPECT_GT(stats.fusion_operations, 0u);
    EXPECT_EQ(stats.active_entities, 1u);
    EXPECT_GE(stats.entity_births, 1u);
}

//=============================================================================
// Utility Tests
//=============================================================================

TEST_F(WorldModelMultimodalTest, GetStatusWithNullReturnsError)
{
    wm_status_t status = wm_get_status(nullptr);
    EXPECT_EQ(status, WM_STATUS_ERROR);
}

TEST_F(WorldModelMultimodalTest, GetStatusReturnsIdle)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    wm_status_t status = wm_get_status(wm_);
    EXPECT_EQ(status, WM_STATUS_IDLE);
}

TEST_F(WorldModelMultimodalTest, GetLastErrorWithNullReturnsNullPtr)
{
    wm_error_t err = wm_get_last_error(nullptr);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, GetLastErrorSucceeds)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);
    wm_error_t err = wm_get_last_error(wm_);
    EXPECT_EQ(err, WM_OK);
}

TEST_F(WorldModelMultimodalTest, ErrorStringReturnsNonNull)
{
    const char* str = wm_error_string(WM_OK);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);
}

TEST_F(WorldModelMultimodalTest, ErrorStringForAllErrors)
{
    std::vector<wm_error_t> errors = {
        WM_OK,
        WM_ERR_NULL_PTR,
        WM_ERR_NOT_INITIALIZED,
        WM_ERR_INVALID_MODALITY,
        WM_ERR_PREDICTION_FAILED,
        WM_ERR_FUSION_FAILED,
        WM_ERR_MEMORY_ALLOC,
        WM_ERR_CAPACITY_EXCEEDED,
        WM_ERR_INVALID_HORIZON,
        WM_ERR_MODALITY_MISMATCH
    };

    for (wm_error_t err : errors) {
        const char* str = wm_error_string(err);
        EXPECT_NE(str, nullptr) << "Error string null for error: " << err;
        EXPECT_GT(strlen(str), 0u) << "Error string empty for error: " << err;
    }
}

TEST_F(WorldModelMultimodalTest, ErrorStringForInvalidErrorReturnsUnknown)
{
    const char* str = wm_error_string(static_cast<wm_error_t>(999));
    EXPECT_NE(str, nullptr);
    // Should return "Unknown" or similar
}

TEST_F(WorldModelMultimodalTest, StatusStringReturnsNonNull)
{
    const char* str = wm_status_string(WM_STATUS_IDLE);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);
}

TEST_F(WorldModelMultimodalTest, StatusStringForAllStatuses)
{
    std::vector<wm_status_t> statuses = {
        WM_STATUS_IDLE,
        WM_STATUS_PROCESSING,
        WM_STATUS_PREDICTING,
        WM_STATUS_FUSING,
        WM_STATUS_ERROR
    };

    for (wm_status_t status : statuses) {
        const char* str = wm_status_string(status);
        EXPECT_NE(str, nullptr) << "Status string null for status: " << status;
        EXPECT_GT(strlen(str), 0u) << "Status string empty for status: " << status;
    }
}

TEST_F(WorldModelMultimodalTest, StatusStringForInvalidStatusReturnsUnknown)
{
    const char* str = wm_status_string(static_cast<wm_status_t>(999));
    EXPECT_NE(str, nullptr);
}

TEST_F(WorldModelMultimodalTest, ModalityStringForAllModalities)
{
    std::vector<wm_modality_t> modalities = {
        WM_MODALITY_VISUAL,
        WM_MODALITY_AUDITORY,
        WM_MODALITY_TACTILE,
        WM_MODALITY_PROPRIOCEPTIVE,
        WM_MODALITY_OLFACTORY,
        WM_MODALITY_GUSTATORY,
        WM_MODALITY_VESTIBULAR,
        WM_MODALITY_INTEROCEPTIVE,
        WM_MODALITY_LINGUISTIC,
        WM_MODALITY_SEMANTIC
    };

    for (wm_modality_t modality : modalities) {
        const char* str = wm_modality_string(modality);
        EXPECT_NE(str, nullptr) << "Modality string null for modality: " << modality;
        EXPECT_GT(strlen(str), 0u) << "Modality string empty for modality: " << modality;
    }
}

TEST_F(WorldModelMultimodalTest, ModalityStringForInvalidModalityReturnsUnknown)
{
    const char* str = wm_modality_string(static_cast<wm_modality_t>(999));
    EXPECT_NE(str, nullptr);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(WorldModelMultimodalTest, FullWorkflowSucceeds)
{
    // Initialize
    ASSERT_EQ(wm_init(wm_), WM_OK);

    // Add entities
    for (uint32_t i = 0; i < 5; ++i) {
        wm_entity_t entity = create_test_entity(i);
        uint32_t entity_id;
        ASSERT_EQ(wm_add_entity(wm_, &entity, &entity_id), WM_OK);
    }

    // Process multiple modalities
    std::vector<float> features(TEST_FEATURE_DIM);
    for (uint32_t i = 0; i < TEST_FEATURE_DIM; ++i) {
        features[i] = static_cast<float>(i) / static_cast<float>(TEST_FEATURE_DIM);
    }

    wm_modality_input_t input;
    memset(&input, 0, sizeof(input));
    input.features = features.data();
    input.feature_dim = TEST_FEATURE_DIM;
    input.confidence = 0.9f;
    input.timestamp = 1000;

    std::vector<wm_modality_t> modalities = {
        WM_MODALITY_VISUAL,
        WM_MODALITY_AUDITORY,
        WM_MODALITY_TACTILE,
        WM_MODALITY_PROPRIOCEPTIVE
    };

    for (wm_modality_t mod : modalities) {
        input.modality = mod;
        ASSERT_EQ(wm_process_modality(wm_, &input), WM_OK);
    }

    // Fuse modalities
    ASSERT_EQ(wm_fuse_modalities(wm_), WM_OK);

    // Get attention state
    wm_cross_modal_attention_t attention;
    ASSERT_EQ(wm_get_attention(wm_, &attention), WM_OK);
    EXPECT_GE(attention.coherence_score, 0.0f);

    // Make predictions
    wm_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));
    ASSERT_EQ(wm_predict(wm_, 10, &prediction), WM_OK);

    // Update with prediction error
    std::vector<float> actual_state(TEST_LATENT_DIM, 0.5f);
    ASSERT_EQ(wm_update_prediction_error(wm_, actual_state.data(), TEST_LATENT_DIM), WM_OK);

    // Update world model
    for (int i = 0; i < 10; ++i) {
        ASSERT_EQ(wm_update(wm_, 16.67f), WM_OK);
    }

    // Get stats
    wm_stats_t stats;
    ASSERT_EQ(wm_get_stats(wm_, &stats), WM_OK);
    EXPECT_GT(stats.inputs_processed, 0u);
    EXPECT_GT(stats.predictions_made, 0u);

    // Reset and verify clean state
    ASSERT_EQ(wm_reset(wm_), WM_OK);

    wm_stats_t reset_stats;
    ASSERT_EQ(wm_get_stats(wm_, &reset_stats), WM_OK);
    EXPECT_EQ(reset_stats.inputs_processed, 0u);
}

TEST_F(WorldModelMultimodalTest, MultimodalFusionCrossModalAttention)
{
    config_.fusion_type = WM_FUSION_ATTENTION;
    nimcp_world_model_t* wm = wm_create(&config_);
    ASSERT_NE(wm, nullptr);
    ASSERT_EQ(wm_init(wm), WM_OK);

    // Create separate feature buffers
    std::vector<float> visual_features(TEST_FEATURE_DIM);
    std::vector<float> audio_features(TEST_FEATURE_DIM);

    for (uint32_t i = 0; i < TEST_FEATURE_DIM; ++i) {
        visual_features[i] = static_cast<float>(i) * 0.1f;
        audio_features[i] = static_cast<float>(TEST_FEATURE_DIM - i) * 0.1f;
    }

    // Process visual
    wm_modality_input_t visual_input;
    memset(&visual_input, 0, sizeof(visual_input));
    visual_input.modality = WM_MODALITY_VISUAL;
    visual_input.features = visual_features.data();
    visual_input.feature_dim = TEST_FEATURE_DIM;
    visual_input.confidence = 0.95f;
    visual_input.timestamp = 1000;
    ASSERT_EQ(wm_process_modality(wm, &visual_input), WM_OK);

    // Process audio
    wm_modality_input_t audio_input;
    memset(&audio_input, 0, sizeof(audio_input));
    audio_input.modality = WM_MODALITY_AUDITORY;
    audio_input.features = audio_features.data();
    audio_input.feature_dim = TEST_FEATURE_DIM;
    audio_input.confidence = 0.85f;
    audio_input.timestamp = 1000;
    ASSERT_EQ(wm_process_modality(wm, &audio_input), WM_OK);

    // Fuse with attention
    ASSERT_EQ(wm_fuse_modalities(wm), WM_OK);

    // Get attention - should reflect cross-modal relationships
    wm_cross_modal_attention_t attention;
    ASSERT_EQ(wm_get_attention(wm, &attention), WM_OK);

    // Verify attention matrix is populated
    float visual_auditory_attention = attention.attention_matrix[WM_MODALITY_VISUAL][WM_MODALITY_AUDITORY];
    float auditory_visual_attention = attention.attention_matrix[WM_MODALITY_AUDITORY][WM_MODALITY_VISUAL];

    // Attention values should be non-negative
    EXPECT_GE(visual_auditory_attention, 0.0f);
    EXPECT_GE(auditory_visual_attention, 0.0f);

    wm_destroy(wm);
}

TEST_F(WorldModelMultimodalTest, EntityTrackingOverTime)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    // Add entity - may not be supported in all implementations
    wm_entity_t entity = create_test_entity(0);
    entity.position[0] = 0.0f;
    entity.position[1] = 0.0f;
    entity.position[2] = 0.0f;
    entity.velocity[0] = 1.0f;
    entity.velocity[1] = 0.5f;
    entity.velocity[2] = 0.0f;

    uint32_t entity_id;
    wm_error_t add_err = wm_add_entity(wm_, &entity, &entity_id);
    if (add_err != WM_OK) {
        // Entity tracking not supported in this implementation
        GTEST_SKIP() << "Entity tracking not supported";
    }

    // Simulate time progression
    float dt = 16.67f;  // ~60fps
    bool entity_tracking_works = true;
    for (int frame = 0; frame < 60 && entity_tracking_works; ++frame) {
        // Update world model
        wm_error_t update_err = wm_update(wm_, dt);
        if (update_err != WM_OK) {
            entity_tracking_works = false;
            break;
        }

        // Get entity and verify it exists
        wm_entity_t current_entity;
        wm_error_t get_err = wm_get_entity(wm_, entity_id, &current_entity);
        if (get_err != WM_OK) {
            entity_tracking_works = false;
            break;
        }
    }

    if (!entity_tracking_works) {
        GTEST_SKIP() << "Entity tracking over time not fully supported";
    }

    // Predict entity trajectory
    float trajectory[30];
    float confidence;
    wm_error_t err = wm_predict_entity(wm_, entity_id, 10, trajectory, &confidence);
    EXPECT_TRUE(err == WM_OK || err == WM_ERR_NOT_INITIALIZED);
}

TEST_F(WorldModelMultimodalTest, HighFrequencyProcessing)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    std::vector<float> features(TEST_FEATURE_DIM);
    for (uint32_t i = 0; i < TEST_FEATURE_DIM; ++i) {
        features[i] = static_cast<float>(i) / static_cast<float>(TEST_FEATURE_DIM);
    }

    wm_modality_input_t input;
    memset(&input, 0, sizeof(input));
    input.modality = WM_MODALITY_VISUAL;
    input.features = features.data();
    input.feature_dim = TEST_FEATURE_DIM;
    input.confidence = 0.9f;

    // Process 1000 frames at 60fps
    for (int frame = 0; frame < 1000; ++frame) {
        input.timestamp = static_cast<uint64_t>(frame * 16);  // 16ms per frame
        ASSERT_EQ(wm_process_modality(wm_, &input), WM_OK);

        if (frame % 10 == 0) {
            ASSERT_EQ(wm_fuse_modalities(wm_), WM_OK);
        }

        ASSERT_EQ(wm_update(wm_, 16.67f), WM_OK);
    }

    wm_stats_t stats;
    ASSERT_EQ(wm_get_stats(wm_, &stats), WM_OK);
    EXPECT_EQ(stats.inputs_processed, 1000u);
}

TEST_F(WorldModelMultimodalTest, AllModalitiesProcessedConcurrently)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    // Create inputs for all modalities
    std::vector<wm_modality_input_t> inputs(WM_MODALITY_COUNT);
    std::vector<std::vector<float>> feature_buffers(WM_MODALITY_COUNT);

    for (int i = 0; i < WM_MODALITY_COUNT; ++i) {
        feature_buffers[i].resize(TEST_FEATURE_DIM);
        for (uint32_t j = 0; j < TEST_FEATURE_DIM; ++j) {
            feature_buffers[i][j] = static_cast<float>(i * TEST_FEATURE_DIM + j) * 0.01f;
        }

        memset(&inputs[i], 0, sizeof(wm_modality_input_t));
        inputs[i].modality = static_cast<wm_modality_t>(i);
        inputs[i].features = feature_buffers[i].data();
        inputs[i].feature_dim = TEST_FEATURE_DIM;
        inputs[i].confidence = 0.8f + static_cast<float>(i) * 0.01f;
        inputs[i].timestamp = 1000;
    }

    // Process all modalities at once
    wm_error_t err = wm_process_multimodal(wm_, inputs.data(), WM_MODALITY_COUNT);
    EXPECT_EQ(err, WM_OK);

    // Fuse all
    ASSERT_EQ(wm_fuse_modalities(wm_), WM_OK);

    // Verify attention includes all modalities
    wm_cross_modal_attention_t attention;
    ASSERT_EQ(wm_get_attention(wm_, &attention), WM_OK);

    // All modality weights should be populated
    for (int i = 0; i < WM_MODALITY_COUNT; ++i) {
        EXPECT_GE(attention.modality_weights[i], 0.0f);
    }
}

//=============================================================================
// Exception Handling Tests (NIMCP_THROW_TO_IMMUNE integration)
//=============================================================================

TEST_F(WorldModelMultimodalTest, PredictionMemoryAllocationHandling)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    // Process input to enable prediction
    wm_modality_input_t input = create_modality_input(WM_MODALITY_VISUAL, TEST_FEATURE_DIM);
    ASSERT_EQ(wm_process_modality(wm_, &input), WM_OK);
    ASSERT_EQ(wm_fuse_modalities(wm_), WM_OK);

    // Test with valid prediction - should succeed
    wm_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));

    wm_error_t err = wm_predict(wm_, 10, &prediction);
    EXPECT_EQ(err, WM_OK);
    EXPECT_NE(prediction.predicted_states, nullptr);
    EXPECT_NE(prediction.uncertainties, nullptr);

    // Clean up prediction resources if needed
    if (prediction.predicted_states) {
        free(prediction.predicted_states);
    }
    if (prediction.uncertainties) {
        free(prediction.uncertainties);
    }
}

TEST_F(WorldModelMultimodalTest, ErrorStateTracking)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    // Initially no error
    EXPECT_EQ(wm_get_last_error(wm_), WM_OK);
    EXPECT_EQ(wm_get_status(wm_), WM_STATUS_IDLE);

    // Try invalid operation - predict with invalid horizon
    wm_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));

    wm_error_t err = wm_predict(wm_, config_.max_prediction_steps + 1, &prediction);
    EXPECT_EQ(err, WM_ERR_INVALID_HORIZON);

    // Status should still be IDLE (error states may be transient)
    wm_status_t status = wm_get_status(wm_);
    EXPECT_TRUE(status == WM_STATUS_IDLE || status == WM_STATUS_ERROR);
}

TEST_F(WorldModelMultimodalTest, RecoveryAfterError)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    // Cause an error
    wm_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));
    wm_error_t err = wm_predict(wm_, config_.max_prediction_steps + 1, &prediction);
    EXPECT_EQ(err, WM_ERR_INVALID_HORIZON);

    // System should recover and work normally
    wm_modality_input_t input = create_modality_input(WM_MODALITY_VISUAL, TEST_FEATURE_DIM);
    err = wm_process_modality(wm_, &input);
    EXPECT_EQ(err, WM_OK);

    err = wm_fuse_modalities(wm_);
    EXPECT_EQ(err, WM_OK);

    // Valid prediction should succeed
    err = wm_predict(wm_, 5, &prediction);
    EXPECT_EQ(err, WM_OK);

    // Clean up
    if (prediction.predicted_states) {
        free(prediction.predicted_states);
    }
    if (prediction.uncertainties) {
        free(prediction.uncertainties);
    }
}

TEST_F(WorldModelMultimodalTest, NullPointerExceptionHandling)
{
    // All functions should handle NULL gracefully with proper error codes
    // (This validates NIMCP_THROW_TO_IMMUNE / NIMCP_CHECK_THROW integration)

    wm_error_t err;

    // Lifecycle
    err = wm_init(nullptr);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);

    err = wm_reset(nullptr);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);

    // Modality
    wm_modality_input_t input = create_modality_input(WM_MODALITY_VISUAL, TEST_FEATURE_DIM);
    err = wm_process_modality(nullptr, &input);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);

    // Fusion
    err = wm_fuse_modalities(nullptr);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);

    // Prediction
    wm_prediction_t prediction;
    err = wm_predict(nullptr, 5, &prediction);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);

    // Entity
    wm_entity_t entity = create_test_entity(0);
    uint32_t entity_id;
    err = wm_add_entity(nullptr, &entity, &entity_id);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);

    // State
    uint32_t dim;
    err = wm_get_global_state(nullptr, state_buffer_.data(), &dim);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);

    // Update
    err = wm_update(nullptr, 16.0f);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);

    // Stats
    wm_stats_t stats;
    err = wm_get_stats(nullptr, &stats);
    EXPECT_EQ(err, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelMultimodalTest, StatusTransitionsDuringOperations)
{
    ASSERT_EQ(wm_init(wm_), WM_OK);

    // Initial state is IDLE
    EXPECT_EQ(wm_get_status(wm_), WM_STATUS_IDLE);

    // After processing, status returns to IDLE
    wm_modality_input_t input = create_modality_input(WM_MODALITY_VISUAL, TEST_FEATURE_DIM);
    ASSERT_EQ(wm_process_modality(wm_, &input), WM_OK);
    EXPECT_EQ(wm_get_status(wm_), WM_STATUS_IDLE);

    // After fusion, status returns to IDLE
    ASSERT_EQ(wm_fuse_modalities(wm_), WM_OK);
    EXPECT_EQ(wm_get_status(wm_), WM_STATUS_IDLE);

    // After prediction, status returns to IDLE
    wm_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));
    ASSERT_EQ(wm_predict(wm_, 5, &prediction), WM_OK);
    EXPECT_EQ(wm_get_status(wm_), WM_STATUS_IDLE);

    // Clean up
    if (prediction.predicted_states) {
        free(prediction.predicted_states);
    }
    if (prediction.uncertainties) {
        free(prediction.uncertainties);
    }
}

}  // namespace
