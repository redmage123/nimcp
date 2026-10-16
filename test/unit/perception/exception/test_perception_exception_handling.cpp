/**
 * @file test_perception_exception_handling.cpp
 * @brief Unit tests for perception module exception handling
 *
 * WHAT: Test exception handling patterns across perception modules
 * WHY:  Verify error code to exception mapping, handler dispatch, immune integration
 * HOW:  Create exceptions for perception-specific errors, verify correct handling
 *
 * PERCEPTION MODULES COVERED:
 * - Visual Cortex: Image processing errors, feature extraction failures
 * - Audio Cortex: Audio processing errors, frequency analysis failures
 * - Speech Cortex: Phoneme recognition errors, lexical access failures
 * - Cochlea: Basilar membrane errors, hair cell damage, ANF failures
 * - Retina: Photoreceptor errors, ganglion cell processing failures
 *
 * TEST CATEGORIES:
 * 1. Error code to exception mapping
 * 2. Exception creation with sensory-specific context
 * 3. Exception dispatch through handler chain
 * 4. Recovery strategy generation for perception errors
 * 5. Epitope generation for immune pattern matching
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <atomic>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class PerceptionExceptionTest : public ::testing::Test {
protected:
    static std::atomic<int> handler_call_count;
    static std::atomic<int> last_exception_code;
    static std::atomic<nimcp_exception_category_t> last_category;
    static std::atomic<bool> visual_handler_called;
    static std::atomic<bool> audio_handler_called;
    static std::atomic<bool> temporal_handler_called;

    void SetUp() override {
        handler_call_count = 0;
        last_exception_code = 0;
        last_category = EXCEPTION_CATEGORY_GENERIC;
        visual_handler_called = false;
        audio_handler_called = false;
        temporal_handler_called = false;

        nimcp_exception_system_init();
    }

    void TearDown() override {
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    // Generic test handler that tracks all exceptions
    static bool generic_test_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        last_exception_code = ex->code;
        last_category = ex->category;
        return false;  // Don't consume - allow other handlers
    }

    // Visual cortex specific handler
    static bool visual_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        if (ex->code >= NIMCP_ERROR_OCCIPITAL_BASE &&
            ex->code < NIMCP_ERROR_OCCIPITAL_BASE + 100) {
            visual_handler_called = true;
            return false;
        }
        return false;
    }

    // Audio cortex / temporal lobe specific handler
    static bool audio_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        if (ex->code >= NIMCP_ERROR_TEMPORAL_BASE &&
            ex->code < NIMCP_ERROR_TEMPORAL_BASE + 100) {
            audio_handler_called = true;
            temporal_handler_called = true;
            return false;
        }
        return false;
    }
};

std::atomic<int> PerceptionExceptionTest::handler_call_count(0);
std::atomic<int> PerceptionExceptionTest::last_exception_code(0);
std::atomic<nimcp_exception_category_t> PerceptionExceptionTest::last_category(EXCEPTION_CATEGORY_GENERIC);
std::atomic<bool> PerceptionExceptionTest::visual_handler_called(false);
std::atomic<bool> PerceptionExceptionTest::audio_handler_called(false);
std::atomic<bool> PerceptionExceptionTest::temporal_handler_called(false);

//=============================================================================
// Error Code Mapping Tests
//=============================================================================

TEST_F(PerceptionExceptionTest, OccipitalErrorCodeMapping) {
    // WHAT: Test occipital lobe (visual) error code to exception mapping
    // WHY:  Verify visual processing errors create correct exceptions

    // Test NIMCP_ERROR_OCCIPITAL_INVALID_INPUT
    nimcp_error_t code = NIMCP_ERROR_OCCIPITAL_INVALID_INPUT;
    nimcp_exception_category_t cat = nimcp_exception_get_category_from_code(code);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_BRAIN_REGION);

    // Test NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING
    code = NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING;
    cat = nimcp_exception_get_category_from_code(code);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_BRAIN_REGION);

    // Test NIMCP_ERROR_OCCIPITAL_FEATURE_EXTRACTION
    code = NIMCP_ERROR_OCCIPITAL_FEATURE_EXTRACTION;
    cat = nimcp_exception_get_category_from_code(code);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_BRAIN_REGION);
}

TEST_F(PerceptionExceptionTest, TemporalErrorCodeMapping) {
    // WHAT: Test temporal lobe (audio) error code to exception mapping
    // WHY:  Verify audio processing errors create correct exceptions

    nimcp_error_t code = NIMCP_ERROR_TEMPORAL_INVALID_INPUT;
    nimcp_exception_category_t cat = nimcp_exception_get_category_from_code(code);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_BRAIN_REGION);

    code = NIMCP_ERROR_TEMPORAL_AUDITORY;
    cat = nimcp_exception_get_category_from_code(code);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_BRAIN_REGION);

    code = NIMCP_ERROR_TEMPORAL_SEMANTIC;
    cat = nimcp_exception_get_category_from_code(code);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_BRAIN_REGION);
}

TEST_F(PerceptionExceptionTest, WernickeErrorCodeMapping) {
    // WHAT: Test Wernicke's area (speech comprehension) error code mapping
    // WHY:  Verify speech comprehension errors are categorized correctly

    nimcp_error_t code = NIMCP_ERROR_WERNICKE_INVALID_INPUT;
    nimcp_exception_category_t cat = nimcp_exception_get_category_from_code(code);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_BRAIN_REGION);

    code = NIMCP_ERROR_WERNICKE_COMPREHENSION;
    cat = nimcp_exception_get_category_from_code(code);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_BRAIN_REGION);

    code = NIMCP_ERROR_WERNICKE_SEMANTIC;
    cat = nimcp_exception_get_category_from_code(code);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_BRAIN_REGION);
}

TEST_F(PerceptionExceptionTest, BrocaErrorCodeMapping) {
    // WHAT: Test Broca's area (speech production) error code mapping
    // WHY:  Verify speech production errors are categorized correctly

    nimcp_error_t code = NIMCP_ERROR_BROCA_INVALID_INPUT;
    nimcp_exception_category_t cat = nimcp_exception_get_category_from_code(code);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_BRAIN_REGION);

    code = NIMCP_ERROR_BROCA_PRODUCTION;
    cat = nimcp_exception_get_category_from_code(code);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_BRAIN_REGION);

    code = NIMCP_ERROR_BROCA_SYNTAX;
    cat = nimcp_exception_get_category_from_code(code);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_BRAIN_REGION);
}

//=============================================================================
// Exception Creation Tests
//=============================================================================

TEST_F(PerceptionExceptionTest, CreateVisualProcessingException) {
    // WHAT: Create exception for visual processing failure
    // WHY:  Verify exception contains correct visual-specific context

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Visual cortex processing failed: invalid image dimensions %dx%d", 640, 480
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_BRAIN_REGION);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_ERROR);
    EXPECT_STRNE(ex->message, "");

    // Add perception-specific context
    int result = nimcp_exception_set_context(ex, "module", "visual_cortex");
    EXPECT_EQ(result, 0);
    result = nimcp_exception_set_context(ex, "image_width", "640");
    EXPECT_EQ(result, 0);
    result = nimcp_exception_set_context(ex, "image_height", "480");
    EXPECT_EQ(result, 0);

    // Verify context
    EXPECT_STREQ(nimcp_exception_get_context(ex, "module"), "visual_cortex");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "image_width"), "640");

    nimcp_exception_unref(ex);
}

TEST_F(PerceptionExceptionTest, CreateAudioProcessingException) {
    // WHAT: Create exception for audio processing failure
    // WHY:  Verify exception contains correct audio-specific context

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_TEMPORAL_AUDITORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Audio cortex FFT failed: sample rate %u Hz, buffer size %u",
        44100, 2048
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_TEMPORAL_AUDITORY);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_BRAIN_REGION);

    // Add audio-specific context
    nimcp_exception_set_context(ex, "module", "audio_cortex");
    nimcp_exception_set_context(ex, "sample_rate", "44100");
    nimcp_exception_set_context(ex, "buffer_size", "2048");
    nimcp_exception_set_context(ex, "fft_bins", "1024");

    EXPECT_EQ(nimcp_exception_context_count(ex), 4u);

    nimcp_exception_unref(ex);
}

TEST_F(PerceptionExceptionTest, CreateSpeechRecognitionException) {
    // WHAT: Create exception for speech recognition failure
    // WHY:  Verify exception contains correct speech-specific context

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_WERNICKE_COMPREHENSION,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Phoneme recognition failed: confidence %.2f below threshold %.2f",
        0.35f, 0.50f
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_WERNICKE_COMPREHENSION);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_WARNING);

    // Add speech-specific context
    nimcp_exception_set_context(ex, "module", "speech_cortex");
    nimcp_exception_set_context(ex, "phoneme", "UNKNOWN");
    nimcp_exception_set_context(ex, "confidence", "0.35");
    nimcp_exception_set_context(ex, "formant_f1", "500");
    nimcp_exception_set_context(ex, "formant_f2", "1500");

    EXPECT_STREQ(nimcp_exception_get_context(ex, "module"), "speech_cortex");

    nimcp_exception_unref(ex);
}

TEST_F(PerceptionExceptionTest, CreateCochleaProcessingException) {
    // WHAT: Create exception for cochlea processing failure
    // WHY:  Verify exception contains correct cochlea-specific context

    // Note: Cochlea errors map to generic BRAIN category since no specific range
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_TEMPORAL_INVALID_INPUT,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Basilar membrane filterbank failed: %u channels, mode %s",
        64, "human"
    );

    ASSERT_NE(ex, nullptr);

    // Add cochlea-specific context
    nimcp_exception_set_context(ex, "module", "cochlea");
    nimcp_exception_set_context(ex, "component", "basilar_membrane");
    nimcp_exception_set_context(ex, "num_channels", "64");
    nimcp_exception_set_context(ex, "hearing_mode", "human");
    nimcp_exception_set_context(ex, "sample_rate", "44100");

    EXPECT_STREQ(nimcp_exception_get_context(ex, "component"), "basilar_membrane");

    nimcp_exception_unref(ex);
}

//=============================================================================
// Brain Exception Type Tests
//=============================================================================

TEST_F(PerceptionExceptionTest, CreateBrainExceptionForVisualCortex) {
    // WHAT: Create nimcp_brain_exception_t for visual cortex
    // WHY:  Verify brain exception captures region-specific fields

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1,  // brain_id
        "visual_cortex_v1",  // region_name
        "V1 Gabor filter convolution failed"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.type, EXCEPTION_TYPE_BRAIN);
    EXPECT_EQ(ex->base.code, NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING);
    EXPECT_EQ(ex->brain_id, 1u);
    EXPECT_STREQ(ex->region_name, "visual_cortex_v1");

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(PerceptionExceptionTest, CreateBrainExceptionForAudioCortex) {
    // WHAT: Create nimcp_brain_exception_t for audio cortex
    // WHY:  Verify brain exception captures region-specific fields

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_TEMPORAL_AUDITORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1,  // brain_id
        "audio_cortex_a1",  // region_name
        "A1 tonotopic mapping failed"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.type, EXCEPTION_TYPE_BRAIN);
    EXPECT_EQ(ex->brain_id, 1u);
    EXPECT_STREQ(ex->region_name, "audio_cortex_a1");

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(PerceptionExceptionTest, CreateBrainExceptionForSpeechCortex) {
    // WHAT: Create nimcp_brain_exception_t for speech cortex
    // WHY:  Verify brain exception captures region-specific fields

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_WERNICKE_COMPREHENSION,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        1,  // brain_id
        "speech_cortex_stg",  // region_name (superior temporal gyrus)
        "STG phoneme classification below confidence threshold"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_STREQ(ex->region_name, "speech_cortex_stg");

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Handler Dispatch Tests
//=============================================================================

TEST_F(PerceptionExceptionTest, DispatchVisualException) {
    // WHAT: Test exception dispatch for visual processing errors
    // WHY:  Verify visual exceptions reach appropriate handlers

    // Register handlers
    nimcp_handler_options_t generic_opts;
    nimcp_handler_default_options(&generic_opts);
    generic_opts.name = "generic_perception_handler";
    generic_opts.handler = generic_test_handler;
    generic_opts.priority = NIMCP_HANDLER_PRIORITY_LOW;
    nimcp_handler_registration_t* generic_reg = nimcp_handler_register(&generic_opts);

    nimcp_handler_options_t visual_opts;
    nimcp_handler_default_options(&visual_opts);
    visual_opts.name = "visual_exception_handler";
    visual_opts.handler = visual_exception_handler;
    visual_opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
    visual_opts.category_filter = EXCEPTION_CATEGORY_BRAIN_REGION;
    nimcp_handler_registration_t* visual_reg = nimcp_handler_register(&visual_opts);

    // Create and dispatch visual exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OCCIPITAL_FEATURE_EXTRACTION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Feature extraction failed in V1 layer"
    );
    ASSERT_NE(ex, nullptr);

    handler_call_count = 0;
    visual_handler_called = false;

    nimcp_exception_dispatch(ex);

    EXPECT_GE(handler_call_count.load(), 1);
    EXPECT_TRUE(visual_handler_called.load());
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_OCCIPITAL_FEATURE_EXTRACTION);

    nimcp_exception_unref(ex);
    nimcp_handler_unregister(generic_reg);
    nimcp_handler_unregister(visual_reg);
}

TEST_F(PerceptionExceptionTest, DispatchAudioException) {
    // WHAT: Test exception dispatch for audio processing errors
    // WHY:  Verify audio exceptions reach appropriate handlers

    nimcp_handler_options_t generic_opts;
    nimcp_handler_default_options(&generic_opts);
    generic_opts.name = "generic_handler";
    generic_opts.handler = generic_test_handler;
    generic_opts.priority = NIMCP_HANDLER_PRIORITY_LOW;
    nimcp_handler_registration_t* generic_reg = nimcp_handler_register(&generic_opts);

    nimcp_handler_options_t audio_opts;
    nimcp_handler_default_options(&audio_opts);
    audio_opts.name = "audio_handler";
    audio_opts.handler = audio_exception_handler;
    audio_opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
    nimcp_handler_registration_t* audio_reg = nimcp_handler_register(&audio_opts);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_TEMPORAL_AUDITORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "FFT analysis failed for audio frame"
    );
    ASSERT_NE(ex, nullptr);

    handler_call_count = 0;
    audio_handler_called = false;
    temporal_handler_called = false;

    nimcp_exception_dispatch(ex);

    EXPECT_GE(handler_call_count.load(), 1);
    EXPECT_TRUE(audio_handler_called.load());
    EXPECT_TRUE(temporal_handler_called.load());

    nimcp_exception_unref(ex);
    nimcp_handler_unregister(generic_reg);
    nimcp_handler_unregister(audio_reg);
}

//=============================================================================
// Recovery Strategy Tests
//=============================================================================

TEST_F(PerceptionExceptionTest, RecoveryStrategyForVisualError) {
    // WHAT: Test recovery strategy for visual processing error
    // WHY:  Verify correct recovery action suggested for visual failures

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Visual processing pipeline failed"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    memset(&strategy, 0, sizeof(strategy));
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Brain region errors should get some recovery action
    // The exact action is implementation-specific
    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE)
        << "Perception errors should have a recovery strategy";

    nimcp_exception_unref(ex);
}

TEST_F(PerceptionExceptionTest, RecoveryStrategyForAudioError) {
    // WHAT: Test recovery strategy for audio processing error
    // WHY:  Verify correct recovery action suggested for audio failures

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_TEMPORAL_AUDITORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Audio cortex processing failed"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Should have fallback defined
    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref(ex);
}

TEST_F(PerceptionExceptionTest, RecoveryStrategyForSpeechError) {
    // WHAT: Test recovery strategy for speech processing error
    // WHY:  Verify correct recovery action suggested for speech failures

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_WERNICKE_COMPREHENSION,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Speech comprehension failed"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Warning severity should still get recovery action
    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Epitope Generation Tests
//=============================================================================

TEST_F(PerceptionExceptionTest, EpitopeGenerationForVisualException) {
    // WHAT: Test epitope generation for visual exception
    // WHY:  Verify immune fingerprint captures visual-specific pattern

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "V1 processing failed"
    );
    ASSERT_NE(ex, nullptr);

    size_t epitope_len = nimcp_exception_generate_epitope(ex);
    EXPECT_GT(epitope_len, 0u);
    EXPECT_LE(epitope_len, NIMCP_EXCEPTION_EPITOPE_SIZE);

    // Epitope should be populated
    bool has_nonzero = false;
    for (size_t i = 0; i < epitope_len; i++) {
        if (ex->epitope[i] != 0) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);

    nimcp_exception_unref(ex);
}

TEST_F(PerceptionExceptionTest, EpitopeUniquenessForDifferentPerceptionModules) {
    // WHAT: Test epitopes are different for different perception module errors
    // WHY:  Immune system needs to distinguish different error patterns

    nimcp_exception_t* visual_ex = nimcp_exception_create(
        NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Visual processing failed"
    );

    nimcp_exception_t* audio_ex = nimcp_exception_create(
        NIMCP_ERROR_TEMPORAL_AUDITORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Audio processing failed"
    );

    nimcp_exception_t* speech_ex = nimcp_exception_create(
        NIMCP_ERROR_WERNICKE_COMPREHENSION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Speech processing failed"
    );

    ASSERT_NE(visual_ex, nullptr);
    ASSERT_NE(audio_ex, nullptr);
    ASSERT_NE(speech_ex, nullptr);

    nimcp_exception_generate_epitope(visual_ex);
    nimcp_exception_generate_epitope(audio_ex);
    nimcp_exception_generate_epitope(speech_ex);

    // Epitopes should be different
    bool visual_audio_different = (memcmp(visual_ex->epitope, audio_ex->epitope,
                                          NIMCP_EXCEPTION_EPITOPE_SIZE) != 0);
    bool visual_speech_different = (memcmp(visual_ex->epitope, speech_ex->epitope,
                                           NIMCP_EXCEPTION_EPITOPE_SIZE) != 0);
    bool audio_speech_different = (memcmp(audio_ex->epitope, speech_ex->epitope,
                                          NIMCP_EXCEPTION_EPITOPE_SIZE) != 0);

    EXPECT_TRUE(visual_audio_different);
    EXPECT_TRUE(visual_speech_different);
    EXPECT_TRUE(audio_speech_different);

    nimcp_exception_unref(visual_ex);
    nimcp_exception_unref(audio_ex);
    nimcp_exception_unref(speech_ex);
}

//=============================================================================
// Antigen Source Mapping Tests
//=============================================================================

TEST_F(PerceptionExceptionTest, AntigenSourceMappingForBrainRegion) {
    // WHAT: Test exception category to antigen source mapping
    // WHY:  Verify perception errors map to correct immune antigen source

    exception_antigen_source_t source = nimcp_exception_to_antigen_source(
        EXCEPTION_CATEGORY_BRAIN_REGION
    );

    // Brain region errors should map to BBB (brain security) or ANOMALY
    EXPECT_TRUE(source == EX_ANTIGEN_SOURCE_BBB ||
                source == EX_ANTIGEN_SOURCE_ANOMALY);
}

TEST_F(PerceptionExceptionTest, ImmuneSeverityMapping) {
    // WHAT: Test exception severity to immune severity mapping
    // WHY:  Verify correct severity scaling for immune system

    // Test various severity levels
    uint32_t debug_sev = nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_DEBUG);
    uint32_t info_sev = nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_INFO);
    uint32_t warning_sev = nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_WARNING);
    uint32_t error_sev = nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_ERROR);
    uint32_t severe_sev = nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_SEVERE);
    uint32_t critical_sev = nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_CRITICAL);
    uint32_t fatal_sev = nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_FATAL);

    // Severity should increase monotonically
    EXPECT_LE(debug_sev, info_sev);
    EXPECT_LE(info_sev, warning_sev);
    EXPECT_LE(warning_sev, error_sev);
    EXPECT_LE(error_sev, severe_sev);
    EXPECT_LE(severe_sev, critical_sev);
    EXPECT_LE(critical_sev, fatal_sev);

    // All should be in valid range (1-10)
    EXPECT_GE(debug_sev, 1u);
    EXPECT_LE(fatal_sev, 10u);
}

//=============================================================================
// Exception Chaining Tests (Perception Pipeline)
//=============================================================================

TEST_F(PerceptionExceptionTest, ChainedPerceptionExceptions) {
    // WHAT: Test exception chaining for perception pipeline failures
    // WHY:  Perception errors often cascade (e.g., cochlea -> audio cortex)

    // Create root cause exception (cochlea failure)
    nimcp_exception_t* cochlea_ex = nimcp_exception_create(
        NIMCP_ERROR_TEMPORAL_INVALID_INPUT,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Basilar membrane filterbank overflow"
    );
    nimcp_exception_set_context(cochlea_ex, "module", "cochlea");
    nimcp_exception_set_context(cochlea_ex, "component", "basilar_membrane");

    // Create intermediate exception (audio cortex failure)
    nimcp_exception_t* audio_ex = nimcp_exception_create(
        NIMCP_ERROR_TEMPORAL_AUDITORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Audio cortex processing failed due to upstream error"
    );
    nimcp_exception_set_context(audio_ex, "module", "audio_cortex");
    nimcp_exception_set_cause(audio_ex, cochlea_ex);

    // Create final exception (speech cortex failure)
    nimcp_exception_t* speech_ex = nimcp_exception_create(
        NIMCP_ERROR_WERNICKE_COMPREHENSION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Speech comprehension failed due to corrupted audio input"
    );
    nimcp_exception_set_context(speech_ex, "module", "speech_cortex");
    nimcp_exception_set_cause(speech_ex, audio_ex);

    // Verify chain
    ASSERT_NE(speech_ex, nullptr);
    nimcp_exception_t* cause1 = nimcp_exception_get_cause(speech_ex);
    ASSERT_NE(cause1, nullptr);
    EXPECT_EQ(cause1->code, NIMCP_ERROR_TEMPORAL_AUDITORY);

    nimcp_exception_t* cause2 = nimcp_exception_get_cause(cause1);
    ASSERT_NE(cause2, nullptr);
    EXPECT_EQ(cause2->code, NIMCP_ERROR_TEMPORAL_INVALID_INPUT);

    nimcp_exception_t* cause3 = nimcp_exception_get_cause(cause2);
    EXPECT_EQ(cause3, nullptr);  // End of chain

    nimcp_exception_unref(speech_ex);
    // Causes are unreffed automatically via chaining
}

//=============================================================================
// Aggregate Exception Tests
//=============================================================================

TEST_F(PerceptionExceptionTest, AggregateMultiSensoryException) {
    // WHAT: Test aggregate exception for multi-sensory processing failure
    // WHY:  Multi-sensory integration can fail in multiple modalities

    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Multi-sensory integration failed"
    );
    ASSERT_NE(agg, nullptr);

    // Add visual failure
    nimcp_exception_t* visual_ex = nimcp_exception_create(
        NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Visual stream failed"
    );
    nimcp_aggregate_exception_add(agg, visual_ex);

    // Add audio failure
    nimcp_exception_t* audio_ex = nimcp_exception_create(
        NIMCP_ERROR_TEMPORAL_AUDITORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Audio stream failed"
    );
    nimcp_aggregate_exception_add(agg, audio_ex);

    // Verify aggregate
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 2u);

    nimcp_exception_t* child0 = nimcp_aggregate_exception_get(agg, 0);
    ASSERT_NE(child0, nullptr);
    EXPECT_EQ(child0->code, NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING);

    nimcp_exception_t* child1 = nimcp_aggregate_exception_get(agg, 1);
    ASSERT_NE(child1, nullptr);
    EXPECT_EQ(child1->code, NIMCP_ERROR_TEMPORAL_AUDITORY);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

//=============================================================================
// Exception String Conversion Tests
//=============================================================================

TEST_F(PerceptionExceptionTest, ExceptionToStringForVisual) {
    // WHAT: Test exception string formatting for visual errors
    // WHY:  Verify human-readable output contains useful information

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "V1 convolution layer failed"
    );
    ASSERT_NE(ex, nullptr);

    char buffer[2048];
    size_t len = nimcp_exception_to_string(ex, buffer, sizeof(buffer));

    EXPECT_GT(len, 0u);
    EXPECT_LT(len, sizeof(buffer));

    // Should contain relevant information
    EXPECT_NE(strstr(buffer, "V1"), nullptr);

    nimcp_exception_unref(ex);
}

TEST_F(PerceptionExceptionTest, SeverityToString) {
    // WHAT: Test severity to string conversion
    // WHY:  Verify readable severity names

    const char* debug_str = nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_DEBUG);
    const char* error_str = nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_ERROR);
    const char* fatal_str = nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_FATAL);

    EXPECT_NE(debug_str, nullptr);
    EXPECT_NE(error_str, nullptr);
    EXPECT_NE(fatal_str, nullptr);

    // Strings should be different
    EXPECT_STRNE(debug_str, error_str);
    EXPECT_STRNE(error_str, fatal_str);
}

TEST_F(PerceptionExceptionTest, CategoryToString) {
    // WHAT: Test category to string conversion
    // WHY:  Verify readable category names

    const char* brain_region_str = nimcp_exception_category_to_string(EXCEPTION_CATEGORY_BRAIN_REGION);
    const char* memory_str = nimcp_exception_category_to_string(EXCEPTION_CATEGORY_MEMORY);

    EXPECT_NE(brain_region_str, nullptr);
    EXPECT_NE(memory_str, nullptr);
    EXPECT_STRNE(brain_region_str, memory_str);
}

//=============================================================================
// Thread-Local Exception Context Tests
//=============================================================================

TEST_F(PerceptionExceptionTest, ThreadLocalPerceptionException) {
    // WHAT: Test thread-local exception storage for perception errors
    // WHY:  Verify perception exceptions work with thread-local context

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Thread-local visual error"
    );
    ASSERT_NE(ex, nullptr);

    // Set as current thread's exception
    nimcp_exception_set_current(ex);

    // Retrieve it
    nimcp_exception_t* current = nimcp_exception_get_current();
    EXPECT_EQ(current, ex);
    EXPECT_EQ(current->code, NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING);

    // Clear
    nimcp_exception_clear_current();
    current = nimcp_exception_get_current();
    EXPECT_EQ(current, nullptr);

    // Exception should have been unreffed by clear
}

//=============================================================================
// Error Code Conversion Tests
//=============================================================================

TEST_F(PerceptionExceptionTest, OccipitalErrorToNimcpConversion) {
    // WHAT: Test occipital_error_to_nimcp conversion
    // WHY:  Verify module-local errors convert correctly to NIMCP errors

    nimcp_error_t nimcp_err = occipital_error_to_nimcp(0);  // No error
    EXPECT_EQ(nimcp_err, NIMCP_SUCCESS);

    nimcp_err = occipital_error_to_nimcp(1);  // Invalid input
    EXPECT_EQ(nimcp_err, NIMCP_ERROR_OCCIPITAL_INVALID_INPUT);

    nimcp_err = occipital_error_to_nimcp(2);  // Visual processing
    EXPECT_EQ(nimcp_err, NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING);
}

TEST_F(PerceptionExceptionTest, TemporalErrorToNimcpConversion) {
    // WHAT: Test temporal_error_to_nimcp conversion
    // WHY:  Verify module-local errors convert correctly to NIMCP errors

    nimcp_error_t nimcp_err = temporal_error_to_nimcp(0);  // No error
    EXPECT_EQ(nimcp_err, NIMCP_SUCCESS);

    nimcp_err = temporal_error_to_nimcp(1);  // Invalid input
    EXPECT_EQ(nimcp_err, NIMCP_ERROR_TEMPORAL_INVALID_INPUT);

    nimcp_err = temporal_error_to_nimcp(2);  // Auditory
    EXPECT_EQ(nimcp_err, NIMCP_ERROR_TEMPORAL_AUDITORY);
}

TEST_F(PerceptionExceptionTest, WernickeErrorToNimcpConversion) {
    // WHAT: Test wernicke_error_to_nimcp conversion
    // WHY:  Verify module-local errors convert correctly to NIMCP errors

    nimcp_error_t nimcp_err = wernicke_error_to_nimcp(0);  // No error
    EXPECT_EQ(nimcp_err, NIMCP_SUCCESS);

    nimcp_err = wernicke_error_to_nimcp(1);  // Invalid input
    EXPECT_EQ(nimcp_err, NIMCP_ERROR_WERNICKE_INVALID_INPUT);

    nimcp_err = wernicke_error_to_nimcp(2);  // Comprehension
    EXPECT_EQ(nimcp_err, NIMCP_ERROR_WERNICKE_COMPREHENSION);
}

TEST_F(PerceptionExceptionTest, BrocaErrorToNimcpConversion) {
    // WHAT: Test broca_error_to_nimcp conversion
    // WHY:  Verify module-local errors convert correctly to NIMCP errors

    nimcp_error_t nimcp_err = broca_error_to_nimcp(0);  // No error
    EXPECT_EQ(nimcp_err, NIMCP_SUCCESS);

    nimcp_err = broca_error_to_nimcp(1);  // Invalid input
    EXPECT_EQ(nimcp_err, NIMCP_ERROR_BROCA_INVALID_INPUT);

    nimcp_err = broca_error_to_nimcp(2);  // Production
    EXPECT_EQ(nimcp_err, NIMCP_ERROR_BROCA_PRODUCTION);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
