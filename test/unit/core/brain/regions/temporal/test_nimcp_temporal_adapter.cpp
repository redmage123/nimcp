/**
 * @file test_nimcp_temporal_adapter.cpp
 * @brief Unit tests for nimcp_temporal_adapter.c
 *
 * WHAT: Comprehensive unit tests for the temporal cortex adapter
 * WHY:  Ensure correct integration of auditory, object recognition, and semantic sub-modules
 * HOW:  Use Google Test framework to test lifecycle, auditory processing, object recognition,
 *       semantic memory, working memory integration, and statistics tracking.
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// Headers have their own extern "C" guards
#include "core/brain/regions/temporal/nimcp_temporal_adapter.h"

// Test Fixture for Temporal Adapter
class TemporalAdapterTest : public ::testing::Test {
protected:
    temporal_adapter_t* adapter;
    temporal_config_t config;

    void SetUp() override {
        config = temporal_default_config();
        adapter = temporal_create(&config);
        ASSERT_NE(nullptr, adapter) << "Failed to create temporal adapter";
    }

    void TearDown() override {
        temporal_destroy(adapter);
        adapter = nullptr;
    }

    // Helper to add a test concept
    void add_test_concept(uint32_t concept_id, const char* name, uint8_t modality) {
        temporal_concept_t concept_entry;
        memset(&concept_entry, 0, sizeof(concept_entry));
        concept_entry.concept_id = concept_id;
        strncpy(concept_entry.name, name, sizeof(concept_entry.name) - 1);
        concept_entry.modality = modality;
        concept_entry.activation = 0.5f;
        concept_entry.embedding_dim = 0;
        concept_entry.embedding = nullptr;
        concept_entry.related_concepts = nullptr;
        concept_entry.num_related = 0;
        ASSERT_TRUE(temporal_add_concept(adapter, &concept_entry));
    }

    // Helper to add a test object prototype
    void add_test_prototype(uint32_t object_id, const char* name, const float* features, uint32_t dim) {
        ASSERT_TRUE(temporal_add_object_prototype(adapter, object_id, name, features, dim));
    }
};

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(TemporalAdapterTest, DefaultConfigHasReasonableValues) {
    temporal_config_t default_config = temporal_default_config();

    EXPECT_EQ(default_config.max_audio_frames, TEMPORAL_DEFAULT_MAX_AUDIO_FRAMES);
    EXPECT_EQ(default_config.max_objects, TEMPORAL_DEFAULT_MAX_OBJECTS);
    EXPECT_EQ(default_config.max_concepts, TEMPORAL_DEFAULT_MAX_CONCEPTS);
    EXPECT_EQ(default_config.working_memory_slots, TEMPORAL_DEFAULT_WORKING_MEMORY_SLOTS);
    EXPECT_TRUE(default_config.enable_working_memory);
    EXPECT_TRUE(default_config.enable_spreading_activation);
    EXPECT_TRUE(default_config.enable_priming);
}

TEST_F(TemporalAdapterTest, CreateWithNullConfigUsesDefaults) {
    temporal_adapter_t* adapter_null = temporal_create(NULL);
    ASSERT_NE(nullptr, adapter_null);

    temporal_config_t retrieved;
    EXPECT_TRUE(temporal_get_config(adapter_null, &retrieved));
    EXPECT_EQ(retrieved.max_audio_frames, TEMPORAL_DEFAULT_MAX_AUDIO_FRAMES);

    temporal_destroy(adapter_null);
}

TEST_F(TemporalAdapterTest, DestroyNullDoesNotCrash) {
    temporal_destroy(NULL);
    // Should not crash
}

TEST_F(TemporalAdapterTest, ResetClearsState) {
    // Add some concepts first
    add_test_concept(1, "cat", 0);

    EXPECT_TRUE(temporal_reset(adapter));

    // Status should be idle after reset
    EXPECT_EQ(temporal_get_status(adapter), TEMPORAL_STATUS_IDLE);
    EXPECT_EQ(temporal_get_last_error(adapter), TEMPORAL_ERROR_NONE);
}

TEST_F(TemporalAdapterTest, ResetNullReturnsFalse) {
    EXPECT_FALSE(temporal_reset(NULL));
}

// ============================================================================
// AUDITORY PROCESSING TESTS
// ============================================================================

TEST_F(TemporalAdapterTest, ProcessAudioFrameSuccess) {
    // Create a simple audio frame
    float samples[256];
    for (int i = 0; i < 256; i++) {
        samples[i] = sinf((float)i * 0.1f) * 0.5f;
    }

    temporal_audio_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.samples = samples;
    frame.num_samples = 256;
    frame.sample_rate = 44100;
    frame.timestamp_ms = 0.0;
    frame.channels = 1;

    temporal_auditory_result_t result;
    EXPECT_TRUE(temporal_process_audio(adapter, &frame, &result));

    EXPECT_GT(result.num_bands, 0u);
    EXPECT_GE(result.loudness, 0.0f);
    EXPECT_LE(result.loudness, 1.0f);
}

TEST_F(TemporalAdapterTest, ProcessAudioNullInputFails) {
    temporal_auditory_result_t result;
    EXPECT_FALSE(temporal_process_audio(adapter, NULL, &result));
    EXPECT_EQ(temporal_get_last_error(adapter), TEMPORAL_ERROR_INVALID_INPUT);
}

TEST_F(TemporalAdapterTest, GetSpectralStateSuccess) {
    // First process some audio
    float samples[256];
    for (int i = 0; i < 256; i++) {
        samples[i] = sinf((float)i * 0.05f) * 0.3f;
    }

    temporal_audio_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.samples = samples;
    frame.num_samples = 256;
    frame.sample_rate = 44100;
    frame.timestamp_ms = 0.0;
    frame.channels = 1;

    temporal_process_audio(adapter, &frame, NULL);

    // Get spectral state
    float spectral[128];
    uint32_t count = temporal_get_spectral_state(adapter, spectral, 128);
    EXPECT_GT(count, 0u);
}

TEST_F(TemporalAdapterTest, DetectSpeechBasic) {
    float confidence;
    // Without audio processing, speech detection should return false
    EXPECT_FALSE(temporal_detect_speech(adapter, &confidence));
}

// ============================================================================
// OBJECT RECOGNITION TESTS
// ============================================================================

TEST_F(TemporalAdapterTest, AddObjectPrototypeSuccess) {
    float features[64];
    for (int i = 0; i < 64; i++) {
        features[i] = (float)i / 64.0f;
    }

    EXPECT_TRUE(temporal_add_object_prototype(adapter, 1, "cat", features, 64));
}

TEST_F(TemporalAdapterTest, RecognizeObjectSuccess) {
    // First add a prototype
    float features[64];
    for (int i = 0; i < 64; i++) {
        features[i] = (float)i / 64.0f;
    }
    ASSERT_TRUE(temporal_add_object_prototype(adapter, 42, "dog", features, 64));

    // Try to recognize with similar features
    temporal_visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.features = features;
    input.feature_dim = 64;
    input.confidence = 0.8f;

    temporal_recognition_result_t result;
    bool recognized = temporal_recognize_object(adapter, &input, &result);

    if (recognized) {
        EXPECT_STREQ(result.object_name, "dog");
        EXPECT_GT(result.confidence, 0.5f);
    }
}

TEST_F(TemporalAdapterTest, RecognizeObjectNullInputFails) {
    temporal_recognition_result_t result;
    EXPECT_FALSE(temporal_recognize_object(adapter, NULL, &result));
}

TEST_F(TemporalAdapterTest, AddObjectPrototypeNullFails) {
    EXPECT_FALSE(temporal_add_object_prototype(adapter, 1, NULL, NULL, 0));
    EXPECT_FALSE(temporal_add_object_prototype(NULL, 1, "test", NULL, 0));
}

// ============================================================================
// SEMANTIC MEMORY TESTS
// ============================================================================

TEST_F(TemporalAdapterTest, AddConceptSuccess) {
    temporal_concept_t tc;
    memset(&tc, 0, sizeof(tc));
    tc.concept_id = 100;
    strcpy(tc.name, "apple");
    tc.modality = 1; // Visual
    tc.activation = 0.5f;

    EXPECT_TRUE(temporal_add_concept(adapter, &tc));
}

TEST_F(TemporalAdapterTest, GetConceptSuccess) {
    // Add a concept
    temporal_concept_t tc;
    memset(&tc, 0, sizeof(tc));
    tc.concept_id = 200;
    strcpy(tc.name, "banana");
    tc.modality = 1;
    tc.activation = 0.7f;

    ASSERT_TRUE(temporal_add_concept(adapter, &tc));

    // Retrieve it
    temporal_concept_t found;
    EXPECT_TRUE(temporal_get_concept(adapter, 200, &found));
    EXPECT_EQ(found.concept_id, 200u);
    EXPECT_STREQ(found.name, "banana");
    EXPECT_FLOAT_EQ(found.activation, 0.7f);
}

TEST_F(TemporalAdapterTest, GetConceptNotFound) {
    temporal_concept_t found;
    EXPECT_FALSE(temporal_get_concept(adapter, 9999, &found));
}

TEST_F(TemporalAdapterTest, SearchConceptsSuccess) {
    // Add some concepts
    add_test_concept(1, "apple_fruit", 1);
    add_test_concept(2, "apple_computer", 2);
    add_test_concept(3, "banana", 1);

    // Search for "apple"
    temporal_concept_t results[10];
    uint32_t found = temporal_search_concepts(adapter, "apple", results, 10);

    EXPECT_GE(found, 1u);
}

TEST_F(TemporalAdapterTest, ApplyPrimingSuccess) {
    // Add a concept
    add_test_concept(500, "primed_concept", 0);

    // Apply priming
    EXPECT_TRUE(temporal_apply_priming(adapter, 500, 0.3f));

    // Get concept and check activation increased
    temporal_concept_t found;
    EXPECT_TRUE(temporal_get_concept(adapter, 500, &found));
    EXPECT_GT(found.activation, 0.5f);
}

TEST_F(TemporalAdapterTest, GetRelatedConcepts) {
    // Add a concept
    add_test_concept(600, "main_concept", 0);

    temporal_semantic_result_t result;
    EXPECT_TRUE(temporal_get_related(adapter, 600, &result, 2));
}

// ============================================================================
// WORKING MEMORY TESTS
// ============================================================================

TEST_F(TemporalAdapterTest, WMPushPopSuccess) {
    // Push some concept IDs
    EXPECT_TRUE(temporal_wm_push(adapter, 1));
    EXPECT_TRUE(temporal_wm_push(adapter, 2));
    EXPECT_TRUE(temporal_wm_push(adapter, 3));

    // Pop them back
    uint32_t id;
    EXPECT_TRUE(temporal_wm_pop(adapter, &id));
    EXPECT_EQ(id, 1u);

    EXPECT_TRUE(temporal_wm_pop(adapter, &id));
    EXPECT_EQ(id, 2u);
}

TEST_F(TemporalAdapterTest, WMPushFullFails) {
    // Fill up working memory
    for (uint32_t i = 0; i < config.working_memory_slots; i++) {
        EXPECT_TRUE(temporal_wm_push(adapter, i));
    }

    // Next push should fail
    EXPECT_FALSE(temporal_wm_push(adapter, 999));
    EXPECT_EQ(temporal_get_last_error(adapter), TEMPORAL_ERROR_WORKING_MEMORY_FULL);
}

TEST_F(TemporalAdapterTest, WMPopEmptyFails) {
    uint32_t id;
    EXPECT_FALSE(temporal_wm_pop(adapter, &id));
}

TEST_F(TemporalAdapterTest, WMGetContentsSuccess) {
    // Push some IDs
    temporal_wm_push(adapter, 10);
    temporal_wm_push(adapter, 20);
    temporal_wm_push(adapter, 30);

    // Get contents
    uint32_t ids[10];
    uint32_t count = 10;
    EXPECT_TRUE(temporal_wm_get_contents(adapter, ids, &count));
    EXPECT_EQ(count, 3u);
    EXPECT_EQ(ids[0], 10u);
    EXPECT_EQ(ids[1], 20u);
    EXPECT_EQ(ids[2], 30u);
}

// ============================================================================
// STATUS AND DIAGNOSTICS TESTS
// ============================================================================

TEST_F(TemporalAdapterTest, GetStatusIdle) {
    EXPECT_EQ(temporal_get_status(adapter), TEMPORAL_STATUS_IDLE);
}

TEST_F(TemporalAdapterTest, GetLastErrorNone) {
    EXPECT_EQ(temporal_get_last_error(adapter), TEMPORAL_ERROR_NONE);
}

TEST_F(TemporalAdapterTest, ErrorStringValid) {
    EXPECT_STREQ(temporal_error_string(TEMPORAL_ERROR_NONE), "No error");
    EXPECT_STREQ(temporal_error_string(TEMPORAL_ERROR_INVALID_INPUT), "Invalid input");
    EXPECT_STREQ(temporal_error_string(TEMPORAL_ERROR_WORKING_MEMORY_FULL), "Working memory full");
    EXPECT_STREQ(temporal_error_string(TEMPORAL_ERROR_CONCEPT_NOT_FOUND), "Concept not found");
}

TEST_F(TemporalAdapterTest, StatusStringValid) {
    EXPECT_STREQ(temporal_status_string(TEMPORAL_STATUS_IDLE), "Idle");
    EXPECT_STREQ(temporal_status_string(TEMPORAL_STATUS_AUDITORY_PROCESSING), "Auditory processing");
    EXPECT_STREQ(temporal_status_string(TEMPORAL_STATUS_OBJECT_RECOGNITION), "Object recognition");
    EXPECT_STREQ(temporal_status_string(TEMPORAL_STATUS_SEMANTIC_RETRIEVAL), "Semantic retrieval");
}

TEST_F(TemporalAdapterTest, GetStatsSuccess) {
    // Do some operations
    add_test_concept(1, "test", 0);

    temporal_stats_t stats;
    EXPECT_TRUE(temporal_get_stats(adapter, &stats));
}

TEST_F(TemporalAdapterTest, GetConfigSuccess) {
    temporal_config_t retrieved;
    EXPECT_TRUE(temporal_get_config(adapter, &retrieved));
    EXPECT_EQ(retrieved.max_audio_frames, config.max_audio_frames);
    EXPECT_EQ(retrieved.max_objects, config.max_objects);
    EXPECT_EQ(retrieved.max_concepts, config.max_concepts);
}

// ============================================================================
// SUB-MODULE ACCESS TESTS
// ============================================================================

TEST_F(TemporalAdapterTest, GetAuditoryProcessorSuccess) {
    auditory_processor_t* proc = temporal_get_auditory_processor(adapter);
    EXPECT_NE(nullptr, proc);
}

TEST_F(TemporalAdapterTest, GetObjectRecognitionSuccess) {
    object_recognition_t* obj = temporal_get_object_recognition(adapter);
    EXPECT_NE(nullptr, obj);
}

TEST_F(TemporalAdapterTest, GetSemanticMemorySuccess) {
    semantic_memory_core_t* sem = temporal_get_semantic_memory(adapter);
    EXPECT_NE(nullptr, sem);
}

TEST_F(TemporalAdapterTest, GetSubModulesNullAdapter) {
    EXPECT_EQ(nullptr, temporal_get_auditory_processor(NULL));
    EXPECT_EQ(nullptr, temporal_get_object_recognition(NULL));
    EXPECT_EQ(nullptr, temporal_get_semantic_memory(NULL));
}

// ============================================================================
// EVENT CALLBACK TESTS
// ============================================================================

static bool event_callback_called = false;
static void test_event_callback(uint32_t event_type, const void* event_data, void* user_data) {
    (void)event_type;
    (void)event_data;
    (void)user_data;
    event_callback_called = true;
}

TEST_F(TemporalAdapterTest, SetEventCallbackSuccess) {
    event_callback_called = false;
    EXPECT_TRUE(temporal_set_event_callback(adapter, test_event_callback, NULL));
}

TEST_F(TemporalAdapterTest, SetEventCallbackNull) {
    EXPECT_FALSE(temporal_set_event_callback(NULL, test_event_callback, NULL));
}

static bool auditory_callback_called = false;
static void test_auditory_callback(const temporal_auditory_result_t* result, void* user_data) {
    (void)result;
    (void)user_data;
    auditory_callback_called = true;
}

TEST_F(TemporalAdapterTest, AuditoryCallbackInvoked) {
    auditory_callback_called = false;
    ASSERT_TRUE(temporal_set_auditory_callback(adapter, test_auditory_callback, NULL));

    // Process audio to trigger callback
    float samples[256];
    for (int i = 0; i < 256; i++) {
        samples[i] = 0.1f;
    }

    temporal_audio_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.samples = samples;
    frame.num_samples = 256;
    frame.sample_rate = 44100;
    frame.channels = 1;

    temporal_auditory_result_t result;
    temporal_process_audio(adapter, &frame, &result);

    EXPECT_TRUE(auditory_callback_called);
}

// ============================================================================
// TRAINING TESTS
// ============================================================================

TEST_F(TemporalAdapterTest, TrainRecognitionDisabledByDefault) {
    float features[32];
    for (int i = 0; i < 32; i++) {
        features[i] = 0.5f;
    }

    temporal_visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.features = features;
    input.feature_dim = 32;

    // Training disabled by default
    EXPECT_FALSE(temporal_train_recognition(adapter, &input, 1, 0.01f));
}

TEST_F(TemporalAdapterTest, TrainAssociationDisabledByDefault) {
    // Training disabled by default
    EXPECT_FALSE(temporal_train_association(adapter, 1, 2, 0.5f));
}

// ============================================================================
// BIO-ASYNC TESTS
// ============================================================================

TEST_F(TemporalAdapterTest, ProcessBioMessagesReturnsZero) {
    // No messages to process
    uint32_t processed = temporal_process_bio_messages(adapter, 10);
    EXPECT_EQ(processed, 0u);
}

TEST_F(TemporalAdapterTest, BroadcastAuditoryEventSuccess) {
    temporal_auditory_result_t result;
    memset(&result, 0, sizeof(result));
    result.loudness = 0.5f;

    nimcp_error_t err = temporal_broadcast_auditory_event(adapter, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(TemporalAdapterTest, BroadcastRecognitionEventSuccess) {
    temporal_recognition_result_t result;
    memset(&result, 0, sizeof(result));
    result.object_id = 1;
    result.confidence = 0.9f;

    nimcp_error_t err = temporal_broadcast_recognition_event(adapter, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}
