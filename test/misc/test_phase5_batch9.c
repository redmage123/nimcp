/**
 * @file test_phase5_batch9.c
 * @brief Phase 5 Batch 9: Speech Cortex, Auditory Nerve, Cochlea
 *
 * Tests for: speech_cortex_recognize_word, speech_cortex_request_frequency_boost,
 * speech_cortex_get_second_messenger_state, anf_bank_get_phase_histogram,
 * anf_bank_generate_neurogram, anf_bank_get_cap, anf_bank_get_precise_spikes,
 * cochlea_process derived features.
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "perception/nimcp_speech_cortex.h"
#include "perception/nimcp_auditory_nerve.h"
#include "perception/nimcp_cochlea.h"
#include "utils/memory/nimcp_memory.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  %-60s", name); } while(0)
#define PASS() do { printf("[PASS]\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); tests_failed++; } while(0)
#define ASSERT_TRUE(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* =========================================================================
 * Speech Cortex Tests
 * ========================================================================= */

static void test_speech_recognize_word_null(void) {
    TEST("Speech: recognize_word NULL safety");
    char buf[64];
    float conf = 0.0f;
    bool ok = speech_cortex_recognize_word(NULL, NULL, 0, buf, 64, &conf);
    ASSERT_TRUE(!ok, "NULL should return false");
    PASS();
}

static void test_speech_recognize_word_empty(void) {
    TEST("Speech: recognize_word with 0 phonemes");
    speech_cortex_config_t cfg = speech_cortex_default_config();
    speech_cortex_t* cortex = speech_cortex_create(&cfg);
    ASSERT_TRUE(cortex != NULL, "cortex NULL");

    char buf[64];
    float conf = -1.0f;
    bool ok = speech_cortex_recognize_word(cortex, (phoneme_t[]){PHONEME_IY}, 0, buf, 64, &conf);
    ASSERT_TRUE(!ok, "empty should fail");
    ASSERT_TRUE(conf == 0.0f, "confidence should be 0");

    speech_cortex_destroy(cortex);
    PASS();
}

static void test_speech_recognize_word_basic(void) {
    TEST("Speech: recognize_word with phoneme sequence");
    speech_cortex_config_t cfg = speech_cortex_default_config();
    speech_cortex_t* cortex = speech_cortex_create(&cfg);
    ASSERT_TRUE(cortex != NULL, "cortex NULL");

    phoneme_t phons[] = {PHONEME_IY, PHONEME_T, PHONEME_IH};
    char buf[128];
    float conf = -1.0f;
    /* Set some activation to allow recognition */
    speech_cortex_recognize_word(cortex, phons, 3, buf, 128, &conf);
    /* Buffer should contain phoneme names joined by dashes */
    ASSERT_TRUE(strlen(buf) > 0, "word buffer should be non-empty");
    ASSERT_TRUE(conf >= 0.0f, "confidence should be >= 0");

    speech_cortex_destroy(cortex);
    PASS();
}

static void test_speech_freq_boost_null(void) {
    TEST("Speech: request_frequency_boost NULL safety");
    float freq, bw;
    bool ok = speech_cortex_request_frequency_boost(NULL, &freq, &bw);
    ASSERT_TRUE(!ok, "NULL should return false");
    PASS();
}

static void test_speech_freq_boost_no_activation(void) {
    TEST("Speech: freq_boost with no phoneme activation");
    speech_cortex_config_t cfg = speech_cortex_default_config();
    speech_cortex_t* cortex = speech_cortex_create(&cfg);
    ASSERT_TRUE(cortex != NULL, "cortex NULL");

    float freq = -1.0f, bw = -1.0f;
    bool ok = speech_cortex_request_frequency_boost(cortex, &freq, &bw);
    ASSERT_TRUE(!ok, "no activation = no boost");
    ASSERT_TRUE(freq == 0.0f, "freq should be 0");
    ASSERT_TRUE(bw == 0.0f, "bw should be 0");

    speech_cortex_destroy(cortex);
    PASS();
}

static void test_speech_second_messenger_null(void) {
    TEST("Speech: second_messenger_state NULL safety");
    float state[4];
    bool ok = speech_cortex_get_second_messenger_state(NULL, 0, state);
    ASSERT_TRUE(!ok, "NULL should return false");
    PASS();
}

static void test_speech_second_messenger_basic(void) {
    TEST("Speech: second_messenger_state returns baseline");
    speech_cortex_config_t cfg = speech_cortex_default_config();
    speech_cortex_t* cortex = speech_cortex_create(&cfg);
    ASSERT_TRUE(cortex != NULL, "cortex NULL");

    float state[4] = {0};
    bool ok = speech_cortex_get_second_messenger_state(cortex, 0, state);
    ASSERT_TRUE(ok, "should succeed");
    /* All pathways should be at 0.5 baseline */
    for (int i = 0; i < 4; i++) {
        ASSERT_TRUE(fabsf(state[i] - 0.5f) < 0.001f, "state should be 0.5 baseline");
    }

    speech_cortex_destroy(cortex);
    PASS();
}

/* =========================================================================
 * Auditory Nerve Tests
 * ========================================================================= */

static void test_anf_phase_histogram_null(void) {
    TEST("ANF: phase_histogram NULL safety");
    float hist[16];
    nimcp_error_t err = anf_bank_get_phase_histogram(NULL, 0, hist, 16);
    ASSERT_TRUE(err != NIMCP_SUCCESS, "NULL should fail");
    PASS();
}

static void test_anf_phase_histogram_basic(void) {
    TEST("ANF: phase_histogram returns normalized distribution");
    anf_config_t cfg = anf_config_default(4, BM_MODE_HUMAN);
    anf_bank_t* bank = anf_bank_create(&cfg);
    ASSERT_TRUE(bank != NULL, "bank NULL");

    float hist[32];
    nimcp_error_t err = anf_bank_get_phase_histogram(bank, 0, hist, 32);
    ASSERT_TRUE(err == NIMCP_SUCCESS, "should succeed");

    /* Check normalization: sum should be ~1.0 */
    float sum = 0.0f;
    for (int i = 0; i < 32; i++) sum += hist[i];
    ASSERT_TRUE(fabsf(sum - 1.0f) < 0.01f, "histogram should sum to ~1.0");

    anf_bank_destroy(bank);
    PASS();
}

static void test_anf_neurogram_null(void) {
    TEST("ANF: generate_neurogram NULL safety");
    nimcp_error_t err = anf_bank_generate_neurogram(NULL, 100.0f, 1.0f, NULL, NULL);
    ASSERT_TRUE(err != NIMCP_SUCCESS, "NULL should fail");
    PASS();
}

static void test_anf_neurogram_basic(void) {
    TEST("ANF: generate_neurogram basic");
    anf_config_t cfg = anf_config_default(4, BM_MODE_HUMAN);
    anf_bank_t* bank = anf_bank_create(&cfg);
    ASSERT_TRUE(bank != NULL, "bank NULL");

    uint32_t num_bins = 0;
    float ch0_data[100], ch1_data[100], ch2_data[100], ch3_data[100];
    float* neurogram[4] = {ch0_data, ch1_data, ch2_data, ch3_data};

    nimcp_error_t err = anf_bank_generate_neurogram(bank, 10.0f, 1.0f, neurogram, &num_bins);
    ASSERT_TRUE(err == NIMCP_SUCCESS, "should succeed");
    ASSERT_TRUE(num_bins > 0, "should have bins");

    anf_bank_destroy(bank);
    PASS();
}

static void test_anf_cap_null(void) {
    TEST("ANF: get_cap NULL safety");
    float waveform[64];
    nimcp_error_t err = anf_bank_get_cap(NULL, waveform, 64);
    ASSERT_TRUE(err != NIMCP_SUCCESS, "NULL should fail");
    PASS();
}

static void test_anf_cap_basic(void) {
    TEST("ANF: get_cap generates waveform");
    anf_config_t cfg = anf_config_default(4, BM_MODE_HUMAN);
    anf_bank_t* bank = anf_bank_create(&cfg);
    ASSERT_TRUE(bank != NULL, "bank NULL");

    float waveform[64];
    nimcp_error_t err = anf_bank_get_cap(bank, waveform, 64);
    ASSERT_TRUE(err == NIMCP_SUCCESS, "should succeed");

    anf_bank_destroy(bank);
    PASS();
}

static void test_anf_precise_spikes_null(void) {
    TEST("ANF: get_precise_spikes NULL safety");
    float times[32];
    uint32_t count;
    nimcp_error_t err = anf_bank_get_precise_spikes(NULL, 0, times, 32, &count);
    ASSERT_TRUE(err != NIMCP_SUCCESS, "NULL should fail");
    PASS();
}

static void test_anf_precise_spikes_no_bat(void) {
    TEST("ANF: precise_spikes in non-bat mode");
    anf_config_t cfg = anf_config_default(4, BM_MODE_HUMAN);
    anf_bank_t* bank = anf_bank_create(&cfg);
    ASSERT_TRUE(bank != NULL, "bank NULL");

    float times[32];
    uint32_t count = 99;
    nimcp_error_t err = anf_bank_get_precise_spikes(bank, 0, times, 32, &count);
    ASSERT_TRUE(err == NIMCP_SUCCESS, "should succeed");
    /* Non-bat mode now generates approximate spikes (was returning 0) */
    ASSERT_TRUE(count <= 32, "count should be <= max_spikes");

    anf_bank_destroy(bank);
    PASS();
}

/* =========================================================================
 * Cochlea Tests
 * ========================================================================= */

static void test_cochlea_create_destroy(void) {
    TEST("Cochlea: create and destroy");
    cochlea_config_t cfg = cochlea_config_default(BM_MODE_HUMAN, 16000);
    cochlea_t* c = cochlea_create(&cfg);
    ASSERT_TRUE(c != NULL, "cochlea NULL");
    cochlea_destroy(c);
    PASS();
}

static void test_cochlea_process_null(void) {
    TEST("Cochlea: process NULL safety");
    nimcp_error_t err = cochlea_process(NULL, NULL, 0, NULL);
    ASSERT_TRUE(err != NIMCP_SUCCESS, "NULL should fail");
    PASS();
}

/* =========================================================================
 * Main
 * ========================================================================= */

int main(void) {
    printf("\n=== Phase 5 Batch 9: Speech Cortex, Auditory Nerve, Cochlea ===\n\n");

    printf("--- Speech Cortex ---\n");
    test_speech_recognize_word_null();
    test_speech_recognize_word_empty();
    test_speech_recognize_word_basic();
    test_speech_freq_boost_null();
    test_speech_freq_boost_no_activation();
    test_speech_second_messenger_null();
    test_speech_second_messenger_basic();

    printf("\n--- Auditory Nerve ---\n");
    test_anf_phase_histogram_null();
    test_anf_phase_histogram_basic();
    test_anf_neurogram_null();
    test_anf_neurogram_basic();
    test_anf_cap_null();
    test_anf_cap_basic();
    test_anf_precise_spikes_null();
    test_anf_precise_spikes_no_bat();

    printf("\n--- Cochlea ---\n");
    test_cochlea_create_destroy();
    test_cochlea_process_null();

    printf("\n=== Results: %d passed, %d failed ===\n\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
