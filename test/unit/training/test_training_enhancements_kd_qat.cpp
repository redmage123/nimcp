/**
 * @file test_training_enhancements_kd_qat.cpp
 * @brief Unit tests for Knowledge Distillation (KD) and Quantization-Aware Training (QAT)
 *
 * Tests cover:
 * - KD: Default config, context lifecycle, teacher management, statistics, utilities
 * - QAT: Default config, INT4 config, context lifecycle, observer management, statistics, utilities
 *
 * @author NIMCP Training Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "training/nimcp_knowledge_distillation.h"
#include "training/nimcp_quantization_aware.h"

//=============================================================================
// Knowledge Distillation Test Fixture
//=============================================================================

class KnowledgeDistillationTest : public ::testing::Test {
protected:
    kd_ctx_t* ctx = nullptr;
    kd_config_t config;

    void SetUp() override {
        memset(&config, 0, sizeof(config));
    }

    void TearDown() override {
        if (ctx) {
            kd_destroy(ctx);
            ctx = nullptr;
        }
    }

    // Helper to create a basic KD context with default config
    kd_ctx_t* createDefaultContext() {
        kd_config_t cfg;
        int result = kd_default_config(&cfg);
        if (result != 0) {
            return nullptr;
        }
        return kd_create(&cfg);
    }

    // Mock teacher forward function for testing
    static int mock_teacher_forward(void* model, const nimcp_tensor_t* input,
                                    nimcp_tensor_t* output, nimcp_tensor_t** features,
                                    uint32_t* num_features) {
        (void)model;
        (void)input;
        (void)output;
        (void)features;
        if (num_features) {
            *num_features = 0;
        }
        return 0;
    }
};

//=============================================================================
// KD Default Config Tests
//=============================================================================

TEST_F(KnowledgeDistillationTest, DefaultConfigInitialization) {
    int result = kd_default_config(&config);
    EXPECT_EQ(result, 0);

    // Verify default values from header constants
    EXPECT_EQ(config.method, KD_METHOD_RESPONSE);
    EXPECT_FLOAT_EQ(config.response.temperature, KD_DEFAULT_TEMPERATURE);
    EXPECT_FLOAT_EQ(config.response.alpha, KD_DEFAULT_ALPHA);
    EXPECT_EQ(config.response.loss_type, KD_LOSS_KL_DIV);
}

TEST_F(KnowledgeDistillationTest, DefaultConfigNullPointerFails) {
    int result = kd_default_config(nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(KnowledgeDistillationTest, DefaultConfigTemperatureValue) {
    int result = kd_default_config(&config);
    EXPECT_EQ(result, 0);
    // Temperature should be 4.0 as per header constant
    EXPECT_FLOAT_EQ(config.response.temperature, 4.0f);
}

TEST_F(KnowledgeDistillationTest, DefaultConfigAlphaValue) {
    int result = kd_default_config(&config);
    EXPECT_EQ(result, 0);
    // Alpha should be 0.7 (70% soft, 30% hard)
    EXPECT_FLOAT_EQ(config.response.alpha, 0.7f);
}

//=============================================================================
// KD Context Lifecycle Tests
//=============================================================================

TEST_F(KnowledgeDistillationTest, CreateWithValidConfig) {
    int result = kd_default_config(&config);
    ASSERT_EQ(result, 0);

    ctx = kd_create(&config);
    EXPECT_NE(ctx, nullptr);
}

TEST_F(KnowledgeDistillationTest, CreateWithNullConfigFails) {
    ctx = kd_create(nullptr);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(KnowledgeDistillationTest, DestroyNullSafe) {
    // Should not crash when destroying NULL
    kd_destroy(nullptr);
    SUCCEED();
}

TEST_F(KnowledgeDistillationTest, DestroyValidContext) {
    ctx = createDefaultContext();
    ASSERT_NE(ctx, nullptr);

    // Destroy and verify no crash
    kd_destroy(ctx);
    ctx = nullptr;  // Prevent double-free in TearDown
    SUCCEED();
}

TEST_F(KnowledgeDistillationTest, CreateMultipleContexts) {
    kd_ctx_t* ctx1 = createDefaultContext();
    kd_ctx_t* ctx2 = createDefaultContext();

    EXPECT_NE(ctx1, nullptr);
    EXPECT_NE(ctx2, nullptr);
    EXPECT_NE(ctx1, ctx2);

    kd_destroy(ctx1);
    kd_destroy(ctx2);
}

//=============================================================================
// KD Configuration Validation Tests
//=============================================================================

TEST_F(KnowledgeDistillationTest, ValidateDefaultConfig) {
    int result = kd_default_config(&config);
    ASSERT_EQ(result, 0);

    result = kd_validate_config(&config);
    EXPECT_EQ(result, 0);
}

TEST_F(KnowledgeDistillationTest, ValidateNullConfigFails) {
    int result = kd_validate_config(nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(KnowledgeDistillationTest, ValidateInvalidTemperature) {
    int result = kd_default_config(&config);
    ASSERT_EQ(result, 0);

    // Set invalid temperature (must be > 0)
    config.response.temperature = 0.0f;
    result = kd_validate_config(&config);
    EXPECT_LT(result, 0);

    config.response.temperature = -1.0f;
    result = kd_validate_config(&config);
    EXPECT_LT(result, 0);
}

TEST_F(KnowledgeDistillationTest, ValidateInvalidAlpha) {
    int result = kd_default_config(&config);
    ASSERT_EQ(result, 0);

    // Alpha must be in [0, 1]
    config.response.alpha = -0.1f;
    result = kd_validate_config(&config);
    EXPECT_LT(result, 0);

    config.response.alpha = 1.1f;
    result = kd_validate_config(&config);
    EXPECT_LT(result, 0);
}

//=============================================================================
// KD Teacher Management Tests
//=============================================================================

TEST_F(KnowledgeDistillationTest, RegisterTeacher) {
    ctx = createDefaultContext();
    ASSERT_NE(ctx, nullptr);

    kd_teacher_t teacher;
    memset(&teacher, 0, sizeof(teacher));
    teacher.name = "test_teacher";
    teacher.model = (void*)0x12345678;  // Mock model pointer
    teacher.forward = mock_teacher_forward;
    teacher.accuracy = 0.95f;
    teacher.confidence = 0.9f;

    int teacher_idx = kd_register_teacher(ctx, &teacher);
    EXPECT_GE(teacher_idx, 0);
}

TEST_F(KnowledgeDistillationTest, RegisterTeacherNullContext) {
    kd_teacher_t teacher;
    memset(&teacher, 0, sizeof(teacher));
    teacher.name = "test_teacher";

    int result = kd_register_teacher(nullptr, &teacher);
    EXPECT_LT(result, 0);
}

TEST_F(KnowledgeDistillationTest, RegisterTeacherNullTeacher) {
    ctx = createDefaultContext();
    ASSERT_NE(ctx, nullptr);

    int result = kd_register_teacher(ctx, nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(KnowledgeDistillationTest, GetTeacherCountInitiallyZero) {
    ctx = createDefaultContext();
    ASSERT_NE(ctx, nullptr);

    uint32_t count = kd_get_teacher_count(ctx);
    EXPECT_EQ(count, 0u);
}

TEST_F(KnowledgeDistillationTest, GetTeacherCountAfterRegistration) {
    ctx = createDefaultContext();
    ASSERT_NE(ctx, nullptr);

    kd_teacher_t teacher;
    memset(&teacher, 0, sizeof(teacher));
    teacher.name = "test_teacher_1";
    teacher.model = (void*)0x1;
    teacher.forward = mock_teacher_forward;

    int idx = kd_register_teacher(ctx, &teacher);
    EXPECT_GE(idx, 0);

    uint32_t count = kd_get_teacher_count(ctx);
    EXPECT_EQ(count, 1u);

    // Register another teacher
    teacher.name = "test_teacher_2";
    teacher.model = (void*)0x2;
    idx = kd_register_teacher(ctx, &teacher);
    EXPECT_GE(idx, 0);

    count = kd_get_teacher_count(ctx);
    EXPECT_EQ(count, 2u);
}

TEST_F(KnowledgeDistillationTest, UnregisterTeacher) {
    ctx = createDefaultContext();
    ASSERT_NE(ctx, nullptr);

    kd_teacher_t teacher;
    memset(&teacher, 0, sizeof(teacher));
    teacher.name = "test_teacher";
    teacher.model = (void*)0x1;
    teacher.forward = mock_teacher_forward;

    int idx = kd_register_teacher(ctx, &teacher);
    ASSERT_GE(idx, 0);
    EXPECT_EQ(kd_get_teacher_count(ctx), 1u);

    int result = kd_unregister_teacher(ctx, (uint32_t)idx);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(kd_get_teacher_count(ctx), 0u);
}

TEST_F(KnowledgeDistillationTest, UnregisterTeacherInvalidIndex) {
    ctx = createDefaultContext();
    ASSERT_NE(ctx, nullptr);

    int result = kd_unregister_teacher(ctx, 999);
    EXPECT_LT(result, 0);
}

//=============================================================================
// KD Statistics Tests
//=============================================================================

TEST_F(KnowledgeDistillationTest, GetStatsValidContext) {
    ctx = createDefaultContext();
    ASSERT_NE(ctx, nullptr);

    kd_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));  // Fill with non-zero to verify clearing

    int result = kd_get_stats(ctx, &stats);
    EXPECT_EQ(result, 0);

    // Initial stats should be zero
    EXPECT_EQ(stats.total_steps, 0u);
    EXPECT_EQ(stats.teacher_forward_count, 0u);
}

TEST_F(KnowledgeDistillationTest, GetStatsNullContext) {
    kd_stats_t stats;
    int result = kd_get_stats(nullptr, &stats);
    EXPECT_LT(result, 0);
}

TEST_F(KnowledgeDistillationTest, GetStatsNullStats) {
    ctx = createDefaultContext();
    ASSERT_NE(ctx, nullptr);

    int result = kd_get_stats(ctx, nullptr);
    EXPECT_LT(result, 0);
}

//=============================================================================
// KD Utility Function Tests
//=============================================================================

TEST_F(KnowledgeDistillationTest, MethodNameResponse) {
    const char* name = kd_method_name(KD_METHOD_RESPONSE);
    ASSERT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
}

TEST_F(KnowledgeDistillationTest, MethodNameFeature) {
    const char* name = kd_method_name(KD_METHOD_FEATURE);
    ASSERT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
}

TEST_F(KnowledgeDistillationTest, MethodNameAttention) {
    const char* name = kd_method_name(KD_METHOD_ATTENTION);
    ASSERT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
}

TEST_F(KnowledgeDistillationTest, MethodNameAllValid) {
    // Test all valid methods return non-null strings
    for (int i = 0; i < KD_METHOD_COUNT; i++) {
        const char* name = kd_method_name(static_cast<kd_method_t>(i));
        ASSERT_NE(name, nullptr) << "Method " << i << " returned null name";
        EXPECT_STRNE(name, "") << "Method " << i << " returned empty name";
    }
}

TEST_F(KnowledgeDistillationTest, MethodNameInvalidReturnsUnknown) {
    const char* name = kd_method_name(static_cast<kd_method_t>(999));
    ASSERT_NE(name, nullptr);
    // Should return some placeholder like "unknown" rather than crash
}

//=============================================================================
// Quantization-Aware Training Test Fixture
//=============================================================================

class QuantizationAwareTrainingTest : public ::testing::Test {
protected:
    qat_ctx_t* ctx = nullptr;
    qat_config_t config;

    void SetUp() override {
        memset(&config, 0, sizeof(config));
    }

    void TearDown() override {
        if (ctx) {
            qat_destroy(ctx);
            ctx = nullptr;
        }
    }

    // Helper to create a basic QAT context with default config
    qat_ctx_t* createDefaultContext() {
        qat_config_t cfg;
        int result = qat_default_config(&cfg);
        if (result != 0) {
            return nullptr;
        }
        return qat_create(&cfg);
    }

    // Helper to create INT4 context
    qat_ctx_t* createInt4Context() {
        qat_config_t cfg;
        int result = qat_int4_config(&cfg);
        if (result != 0) {
            return nullptr;
        }
        return qat_create(&cfg);
    }
};

//=============================================================================
// QAT Default Config Tests
//=============================================================================

TEST_F(QuantizationAwareTrainingTest, DefaultConfigInitialization) {
    int result = qat_default_config(&config);
    EXPECT_EQ(result, 0);

    // Verify INT8 defaults
    EXPECT_EQ(config.default_weight_dtype, QAT_DTYPE_INT8);
    EXPECT_EQ(config.default_activation_dtype, QAT_DTYPE_INT8);
    EXPECT_EQ(config.default_scheme, QAT_SCHEME_SYMMETRIC);
    // Implementation defaults to MINMAX observer method
    EXPECT_EQ(config.observer.method, QAT_OBSERVER_MINMAX);
    EXPECT_EQ(config.fake_quant.method, QAT_FAKE_QUANT_STE);
}

TEST_F(QuantizationAwareTrainingTest, DefaultConfigNullPointerFails) {
    int result = qat_default_config(nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(QuantizationAwareTrainingTest, DefaultConfigObserverDecay) {
    int result = qat_default_config(&config);
    EXPECT_EQ(result, 0);
    // EMA decay should be as per header constant
    EXPECT_FLOAT_EQ(config.observer.ema_decay, QAT_DEFAULT_EMA_DECAY);
}

//=============================================================================
// QAT INT4 Config Tests
//=============================================================================

TEST_F(QuantizationAwareTrainingTest, Int4ConfigInitialization) {
    int result = qat_int4_config(&config);
    EXPECT_EQ(result, 0);

    // Verify INT4 settings
    EXPECT_EQ(config.default_weight_dtype, QAT_DTYPE_INT4);
}

TEST_F(QuantizationAwareTrainingTest, Int4ConfigNullPointerFails) {
    int result = qat_int4_config(nullptr);
    EXPECT_LT(result, 0);
}

//=============================================================================
// QAT Context Lifecycle Tests
//=============================================================================

TEST_F(QuantizationAwareTrainingTest, CreateWithValidConfig) {
    int result = qat_default_config(&config);
    ASSERT_EQ(result, 0);

    ctx = qat_create(&config);
    EXPECT_NE(ctx, nullptr);
}

TEST_F(QuantizationAwareTrainingTest, CreateWithNullConfigFails) {
    ctx = qat_create(nullptr);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(QuantizationAwareTrainingTest, CreateWithInt4Config) {
    int result = qat_int4_config(&config);
    ASSERT_EQ(result, 0);

    ctx = qat_create(&config);
    EXPECT_NE(ctx, nullptr);
}

TEST_F(QuantizationAwareTrainingTest, DestroyNullSafe) {
    // Should not crash when destroying NULL
    qat_destroy(nullptr);
    SUCCEED();
}

TEST_F(QuantizationAwareTrainingTest, DestroyValidContext) {
    ctx = createDefaultContext();
    ASSERT_NE(ctx, nullptr);

    // Destroy and verify no crash
    qat_destroy(ctx);
    ctx = nullptr;  // Prevent double-free in TearDown
    SUCCEED();
}

TEST_F(QuantizationAwareTrainingTest, CreateMultipleContexts) {
    qat_ctx_t* ctx1 = createDefaultContext();
    qat_ctx_t* ctx2 = createInt4Context();

    EXPECT_NE(ctx1, nullptr);
    EXPECT_NE(ctx2, nullptr);
    EXPECT_NE(ctx1, ctx2);

    qat_destroy(ctx1);
    qat_destroy(ctx2);
}

//=============================================================================
// QAT Configuration Validation Tests
//=============================================================================

TEST_F(QuantizationAwareTrainingTest, ValidateDefaultConfig) {
    int result = qat_default_config(&config);
    ASSERT_EQ(result, 0);

    result = qat_validate_config(&config);
    EXPECT_EQ(result, 0);
}

TEST_F(QuantizationAwareTrainingTest, ValidateInt4Config) {
    int result = qat_int4_config(&config);
    ASSERT_EQ(result, 0);

    result = qat_validate_config(&config);
    EXPECT_EQ(result, 0);
}

TEST_F(QuantizationAwareTrainingTest, ValidateNullConfigFails) {
    int result = qat_validate_config(nullptr);
    EXPECT_LT(result, 0);
}

//=============================================================================
// QAT Observer Management Tests
//=============================================================================

TEST_F(QuantizationAwareTrainingTest, RegisterObserverWeights) {
    ctx = createDefaultContext();
    ASSERT_NE(ctx, nullptr);

    int observer_id = qat_register_observer(ctx, "layer1_weights", QAT_TARGET_WEIGHTS);
    EXPECT_GE(observer_id, 0);
}

TEST_F(QuantizationAwareTrainingTest, RegisterObserverActivations) {
    ctx = createDefaultContext();
    ASSERT_NE(ctx, nullptr);

    int observer_id = qat_register_observer(ctx, "layer1_activations", QAT_TARGET_ACTIVATIONS);
    EXPECT_GE(observer_id, 0);
}

TEST_F(QuantizationAwareTrainingTest, RegisterObserverBoth) {
    ctx = createDefaultContext();
    ASSERT_NE(ctx, nullptr);

    int observer_id = qat_register_observer(ctx, "layer1_both", QAT_TARGET_BOTH);
    EXPECT_GE(observer_id, 0);
}

TEST_F(QuantizationAwareTrainingTest, RegisterObserverNullContext) {
    int result = qat_register_observer(nullptr, "test", QAT_TARGET_WEIGHTS);
    EXPECT_LT(result, 0);
}

TEST_F(QuantizationAwareTrainingTest, RegisterObserverNullName) {
    ctx = createDefaultContext();
    ASSERT_NE(ctx, nullptr);

    int result = qat_register_observer(ctx, nullptr, QAT_TARGET_WEIGHTS);
    EXPECT_LT(result, 0);
}

TEST_F(QuantizationAwareTrainingTest, RegisterMultipleObservers) {
    ctx = createDefaultContext();
    ASSERT_NE(ctx, nullptr);

    int id1 = qat_register_observer(ctx, "observer1", QAT_TARGET_WEIGHTS);
    int id2 = qat_register_observer(ctx, "observer2", QAT_TARGET_ACTIVATIONS);
    int id3 = qat_register_observer(ctx, "observer3", QAT_TARGET_BOTH);

    EXPECT_GE(id1, 0);
    EXPECT_GE(id2, 0);
    EXPECT_GE(id3, 0);

    // All IDs should be unique
    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_NE(id1, id3);
}

TEST_F(QuantizationAwareTrainingTest, FreezeObservers) {
    ctx = createDefaultContext();
    ASSERT_NE(ctx, nullptr);

    // Register some observers first
    int id = qat_register_observer(ctx, "test_observer", QAT_TARGET_WEIGHTS);
    ASSERT_GE(id, 0);

    int result = qat_freeze_observers(ctx);
    EXPECT_EQ(result, 0);
}

TEST_F(QuantizationAwareTrainingTest, FreezeObserversNullContext) {
    int result = qat_freeze_observers(nullptr);
    EXPECT_LT(result, 0);
}

//=============================================================================
// QAT Statistics Tests
//=============================================================================

TEST_F(QuantizationAwareTrainingTest, GetStatsValidContext) {
    ctx = createDefaultContext();
    ASSERT_NE(ctx, nullptr);

    qat_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));  // Fill with non-zero to verify clearing

    int result = qat_get_stats(ctx, &stats);
    EXPECT_EQ(result, 0);

    // Initial stats should be zero
    EXPECT_EQ(stats.total_steps, 0u);
    EXPECT_EQ(stats.calibration_batches, 0u);
}

TEST_F(QuantizationAwareTrainingTest, GetStatsNullContext) {
    qat_stats_t stats;
    int result = qat_get_stats(nullptr, &stats);
    EXPECT_LT(result, 0);
}

TEST_F(QuantizationAwareTrainingTest, GetStatsNullStats) {
    ctx = createDefaultContext();
    ASSERT_NE(ctx, nullptr);

    int result = qat_get_stats(ctx, nullptr);
    EXPECT_LT(result, 0);
}

//=============================================================================
// QAT Utility Function Tests
//=============================================================================

TEST_F(QuantizationAwareTrainingTest, DtypeNameInt8) {
    const char* name = qat_dtype_name(QAT_DTYPE_INT8);
    ASSERT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
}

TEST_F(QuantizationAwareTrainingTest, DtypeNameInt4) {
    const char* name = qat_dtype_name(QAT_DTYPE_INT4);
    ASSERT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
}

TEST_F(QuantizationAwareTrainingTest, DtypeNameAllValid) {
    // Test all valid dtypes return non-null strings
    for (int i = 0; i < QAT_DTYPE_COUNT; i++) {
        const char* name = qat_dtype_name(static_cast<qat_dtype_t>(i));
        ASSERT_NE(name, nullptr) << "Dtype " << i << " returned null name";
        EXPECT_STRNE(name, "") << "Dtype " << i << " returned empty name";
    }
}

TEST_F(QuantizationAwareTrainingTest, DtypeNameInvalidReturnsUnknown) {
    const char* name = qat_dtype_name(static_cast<qat_dtype_t>(999));
    ASSERT_NE(name, nullptr);
    // Should return some placeholder like "unknown" rather than crash
}

TEST_F(QuantizationAwareTrainingTest, DtypeBitsInt8) {
    uint32_t bits = qat_dtype_bits(QAT_DTYPE_INT8);
    EXPECT_EQ(bits, 8u);
}

TEST_F(QuantizationAwareTrainingTest, DtypeBitsInt4) {
    uint32_t bits = qat_dtype_bits(QAT_DTYPE_INT4);
    EXPECT_EQ(bits, 4u);
}

TEST_F(QuantizationAwareTrainingTest, DtypeBitsInt2) {
    uint32_t bits = qat_dtype_bits(QAT_DTYPE_INT2);
    EXPECT_EQ(bits, 2u);
}

TEST_F(QuantizationAwareTrainingTest, DtypeBitsInt1) {
    uint32_t bits = qat_dtype_bits(QAT_DTYPE_INT1);
    EXPECT_EQ(bits, 1u);
}

TEST_F(QuantizationAwareTrainingTest, DtypeBitsUint8) {
    uint32_t bits = qat_dtype_bits(QAT_DTYPE_UINT8);
    EXPECT_EQ(bits, 8u);
}

TEST_F(QuantizationAwareTrainingTest, DtypeBitsFP8) {
    uint32_t bits_e4m3 = qat_dtype_bits(QAT_DTYPE_FP8_E4M3);
    uint32_t bits_e5m2 = qat_dtype_bits(QAT_DTYPE_FP8_E5M2);
    EXPECT_EQ(bits_e4m3, 8u);
    EXPECT_EQ(bits_e5m2, 8u);
}

TEST_F(QuantizationAwareTrainingTest, DtypeBitsFP4) {
    uint32_t bits = qat_dtype_bits(QAT_DTYPE_FP4);
    EXPECT_EQ(bits, 4u);
}

//=============================================================================
// QAT Scheme Name Tests
//=============================================================================

TEST_F(QuantizationAwareTrainingTest, SchemeNameSymmetric) {
    const char* name = qat_scheme_name(QAT_SCHEME_SYMMETRIC);
    ASSERT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
}

TEST_F(QuantizationAwareTrainingTest, SchemeNameAffine) {
    const char* name = qat_scheme_name(QAT_SCHEME_AFFINE);
    ASSERT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
}

TEST_F(QuantizationAwareTrainingTest, SchemeNamePowerOfTwo) {
    const char* name = qat_scheme_name(QAT_SCHEME_POWER_OF_TWO);
    ASSERT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
}

TEST_F(QuantizationAwareTrainingTest, SchemeNameAllValid) {
    // Test all valid schemes return non-null strings
    for (int i = 0; i < QAT_SCHEME_COUNT; i++) {
        const char* name = qat_scheme_name(static_cast<qat_scheme_t>(i));
        ASSERT_NE(name, nullptr) << "Scheme " << i << " returned null name";
        EXPECT_STRNE(name, "") << "Scheme " << i << " returned empty name";
    }
}

//=============================================================================
// Cross-Module Integration Tests
//=============================================================================

class KDQATIntegrationTest : public ::testing::Test {
protected:
    kd_ctx_t* kd_ctx = nullptr;
    qat_ctx_t* qat_ctx = nullptr;

    void TearDown() override {
        if (kd_ctx) {
            kd_destroy(kd_ctx);
            kd_ctx = nullptr;
        }
        if (qat_ctx) {
            qat_destroy(qat_ctx);
            qat_ctx = nullptr;
        }
    }
};

TEST_F(KDQATIntegrationTest, BothContextsCanCoexist) {
    // Create KD context
    kd_config_t kd_cfg;
    ASSERT_EQ(kd_default_config(&kd_cfg), 0);
    kd_ctx = kd_create(&kd_cfg);
    ASSERT_NE(kd_ctx, nullptr);

    // Create QAT context
    qat_config_t qat_cfg;
    ASSERT_EQ(qat_default_config(&qat_cfg), 0);
    qat_ctx = qat_create(&qat_cfg);
    ASSERT_NE(qat_ctx, nullptr);

    // Both should work independently
    uint32_t teacher_count = kd_get_teacher_count(kd_ctx);
    EXPECT_EQ(teacher_count, 0u);

    int observer_id = qat_register_observer(qat_ctx, "test", QAT_TARGET_WEIGHTS);
    EXPECT_GE(observer_id, 0);
}

TEST_F(KDQATIntegrationTest, IndependentLifecycles) {
    // Create both contexts
    kd_config_t kd_cfg;
    ASSERT_EQ(kd_default_config(&kd_cfg), 0);
    kd_ctx = kd_create(&kd_cfg);
    ASSERT_NE(kd_ctx, nullptr);

    qat_config_t qat_cfg;
    ASSERT_EQ(qat_default_config(&qat_cfg), 0);
    qat_ctx = qat_create(&qat_cfg);
    ASSERT_NE(qat_ctx, nullptr);

    // Destroy KD first, QAT should still work
    kd_destroy(kd_ctx);
    kd_ctx = nullptr;

    int observer_id = qat_register_observer(qat_ctx, "still_works", QAT_TARGET_ACTIVATIONS);
    EXPECT_GE(observer_id, 0);
}

