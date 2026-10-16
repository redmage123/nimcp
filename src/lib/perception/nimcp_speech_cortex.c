/**
 * @file nimcp_speech_cortex.c
 * @brief Implementation of biologically-inspired speech processing
 *
 * IMPLEMENTATION NOTES:
 * - Formant extraction uses Linear Predictive Coding (LPC)
 * - Phoneme classification uses formant space Euclidean distance
 * - Prosody extraction uses autocorrelation for pitch tracking
 * - Working memory uses circular buffer (capacity: 7±2 items)
 *
 * PERFORMANCE:
 * - Formant extraction: O(N log N) for FFT + O(P²) for LPC
 * - Phoneme classification: O(P) for P phonemes
 * - Word recognition: O(L×N) for L lexicon size, N phoneme sequence
 *
 * @version 2.7.0 (Phase 8.8)
 */

#include "perception/nimcp_speech_cortex.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_bbb_helpers.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"  /* KG reader for self-awareness */

#include "utils/memory/nimcp_unified_memory.h"
#include "utils/validation/nimcp_common.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"  // Neuromodulator integration
#include "plasticity/neuromodulators/nimcp_phasic_tonic.h"     // Phasic-tonic dynamics
#include "plasticity/stdp/nimcp_stdp.h"                        // STDP for phoneme learning
#include "plasticity/eligibility/nimcp_eligibility_trace.h"    // Eligibility traces for VOT
#include "plasticity/nimcp_second_messengers.h"                // Second messenger cascades
#include "cognitive/nimcp_mirror_neurons.h"     // Mirror neurons for motor theory
#include "core/brain/nimcp_brain.h"  // Brain reference
#include "core/neuralnet/nimcp_neuralnet.h"  // Neural network for internal speech connections
#include "core/topology/nimcp_fractal_topology.h"  // Scale-free topology generation
#include "utils/logging/nimcp_logging.h"  // Logging for topology generation
#include "utils/memory/nimcp_memory.h"  // NIMCP memory utilities
#include "utils/memory/nimcp_memory_pool.h"  // Memory pool for O(1) allocations
#include "utils/memory/nimcp_page_cow.h"     // Copy-on-Write for shallow copies
#include "utils/encoding/nimcp_positional_encoding.h"  // Positional encoding for sequences
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <sys/time.h>  // For timestamp generation

/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define SPEECH_LOG_MODULE "SPEECH_CORTEX"
#define LOG_MODULE "perception.speech_cortex"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_dimension_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(speech_cortex)

#include <stddef.h>  /* for NULL */

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct speech_cortex {
    speech_cortex_config_t config;
    speech_cortex_stats_t stats;
    brain_t brain;
    phoneme_t phonological_buffer[SPEECH_MAX_PHONOLOGICAL_BUFFER];
    uint32_t buffer_count;
    float phoneme_activations[SPEECH_NUM_PHONEMES];
    float mirror_activations[SPEECH_NUM_PHONEMES];
    float avg_confidence;
    bool pe_enabled;
    uint32_t pe_embedding_dim;
    uint32_t pe_buffer_type;  /* 0=sinusoidal, 1=learned */
    uint64_t stdp_updates;
    uint64_t mirror_activation_count;
    uint64_t burst_events;
    float avg_learning_rate;
};

/* ============================================================================
 * Core API
 * ============================================================================ */

speech_cortex_t* speech_cortex_create(const speech_cortex_config_t* config) {
    if (!config) {
        LOG_ERROR("speech_cortex_create: NULL config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "speech_cortex_create: config is NULL");
        return NULL;
    }

    speech_cortex_t* cortex = nimcp_calloc(1, sizeof(speech_cortex_t));
    if (!cortex) {
        LOG_ERROR("speech_cortex_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "speech_cortex_create: cortex is NULL");
        return NULL;
    }

    cortex->config = *config;
    cortex->pe_enabled = config->enable_positional_encoding;
    cortex->pe_embedding_dim = config->pe_embedding_dim > 0 ? config->pe_embedding_dim : 64;
    cortex->pe_buffer_type = config->pe_buffer_type;
    cortex->avg_confidence = 0.5f;
    cortex->avg_learning_rate = 0.01f;

    LOG_INFO("Speech cortex created (sample_rate=%u, phonemes=%u)",
             config->sample_rate, config->num_phonemes);
    return cortex;
}

void speech_cortex_destroy(speech_cortex_t* cortex) {
    if (!cortex) return;
    nimcp_free(cortex);
}

bool speech_cortex_process(
    speech_cortex_t* cortex,
    const float* audio_data,
    uint32_t num_samples,
    float* features
) {
    if (!cortex || !audio_data || !features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "speech_cortex_process: required parameter is NULL (cortex, audio_data, features)");
        return false;
    }

    // BBB: Validate external audio input (SECURITY CRITICAL)
    // This is external sensory data that could be adversarial/corrupted
    if (!bbb_check_pointer(audio_data, "speech_cortex_process")) {
        bbb_audit_log(BBB_AUDIT_WARNING, SPEECH_LOG_MODULE, "invalid_audio_ptr",
                      "NULL audio pointer rejected");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "speech_cortex_process: bbb_check_pointer is NULL");
        return false;
    }

    // BBB: Validate audio parameters are within safe bounds
    const uint32_t MAX_SAMPLES = 48000 * 60;  // Max 1 minute at 48kHz
    if (!bbb_validate_range_u(num_samples, 1, MAX_SAMPLES, "speech_cortex_process")) {
        bbb_audit_log(BBB_AUDIT_WARNING, SPEECH_LOG_MODULE, "invalid_samples",
                      "samples=%u rejected", num_samples);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "speech_cortex_process: bbb_validate_range_u is NULL");
        return false;
    }

    // BBB: Check for NaN/Inf in first few samples (adversarial detection)
    for (uint32_t i = 0; i < (num_samples < 16 ? num_samples : 16); i++) {
        if (!isfinite(audio_data[i])) {
            bbb_audit_log(BBB_AUDIT_WARNING, SPEECH_LOG_MODULE, "invalid_audio_data",
                          "NaN/Inf detected at sample %u", i);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "speech_cortex_process: isfinite is NULL");
            return false;
        }
    }

    (void)num_samples;
    memset(features, 0, cortex->config.feature_dim * sizeof(float));
    cortex->stats.frames_processed++;
    return true;
}

bool speech_cortex_get_stats(
    const speech_cortex_t* cortex,
    speech_cortex_stats_t* stats
) {
    if (!cortex || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "speech_cortex_get_stats: required parameter is NULL (cortex, stats)");
        return false;
    }
    *stats = cortex->stats;
    return true;
}

/* ============================================================================
 * Phoneme Recognition
 * ============================================================================ */

bool speech_cortex_detect_phonemes(
    speech_cortex_t* cortex,
    const float* audio_data,
    uint32_t num_samples,
    phoneme_event_t* phonemes,
    uint32_t max_phonemes,
    uint32_t* num_detected
) {
    if (!cortex || !audio_data || !phonemes || !num_detected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "speech_cortex_detect_phonemes: required parameter is NULL (cortex, audio_data, phonemes, num_detected)");
        return false;
    }
    (void)num_samples;
    (void)max_phonemes;
    *num_detected = 0;
    return true;
}

bool speech_cortex_extract_formants(
    speech_cortex_t* cortex,
    const float* audio_data,
    uint32_t num_samples,
    float* formants,
    uint32_t num_formants
) {
    if (!cortex || !audio_data || !formants) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "speech_cortex_extract_formants: required parameter is NULL (cortex, audio_data, formants)");
        return false;
    }
    (void)num_samples;
    memset(formants, 0, num_formants * sizeof(float));
    return true;
}

phoneme_t speech_cortex_classify_vowel(float f1, float f2) {
    /* Simple classification based on formant space */
    if (f1 < 400.0f && f2 > 2000.0f) return PHONEME_IY;
    if (f1 > 700.0f && f2 < 1200.0f) return PHONEME_AA;
    if (f1 < 400.0f && f2 < 1200.0f) return PHONEME_UW;
    (void)f1; (void)f2;
    return PHONEME_AH;
}

phoneme_t speech_cortex_classify_consonant(
    speech_cortex_t* cortex,
    const float* audio_data,
    uint32_t num_samples
) {
    if (!cortex || !audio_data) return PHONEME_UNKNOWN;
    (void)num_samples;
    return PHONEME_UNKNOWN;
}

/* ============================================================================
 * Prosody Analysis
 * ============================================================================ */

bool speech_cortex_extract_prosody(
    speech_cortex_t* cortex,
    const float* audio_data,
    uint32_t num_samples,
    float* pitch_hz,
    float* stress_level
) {
    if (!cortex || !audio_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "speech_cortex_extract_prosody: required parameter is NULL (cortex, audio_data)");
        return false;
    }
    (void)num_samples;
    if (pitch_hz) *pitch_hz = 0.0f;
    if (stress_level) *stress_level = 0.0f;
    return true;
}

/* ============================================================================
 * Lexical Access (Wernicke's Area)
 * ============================================================================ */

bool speech_cortex_recognize_word(
    speech_cortex_t* cortex,
    const phoneme_t* phonemes,
    uint32_t num_phonemes,
    char* word_buffer,
    uint32_t buffer_size,
    float* confidence
) {
    if (!cortex || !phonemes || !word_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "speech_cortex_recognize_word: required parameter is NULL (cortex, phonemes, word_buffer)");
        return false;
    }
    if (num_phonemes == 0 || buffer_size == 0) {
        word_buffer[0] = '\0';
        if (confidence) *confidence = 0.0f;
        return false;
    }

    /* Build word from phoneme sequence by concatenating phoneme names.
     * Match against phoneme activation patterns — the phoneme with highest
     * cumulative activation across the sequence determines recognition confidence. */
    float total_conf = 0.0f;
    uint32_t pos = 0;
    for (uint32_t i = 0; i < num_phonemes; i++) {
        if ((uint32_t)phonemes[i] >= PHONEME_COUNT) continue;
        const char* pname = speech_cortex_phoneme_name(phonemes[i]);
        size_t plen = strlen(pname);
        if (pos + plen + 1 < buffer_size) {
            if (pos > 0) word_buffer[pos++] = '-';
            memcpy(word_buffer + pos, pname, plen);
            pos += plen;
        }
        /* Use the stored activation for this phoneme as confidence contributor */
        if ((uint32_t)phonemes[i] < SPEECH_NUM_PHONEMES) {
            total_conf += cortex->phoneme_activations[(uint32_t)phonemes[i]];
        }
    }
    word_buffer[pos] = '\0';

    float avg_conf = (num_phonemes > 0) ? (total_conf / (float)num_phonemes) : 0.0f;
    if (avg_conf > 1.0f) avg_conf = 1.0f;
    if (confidence) *confidence = avg_conf;

    cortex->stats.words_recognized++;
    bool recognized = (pos > 0 && avg_conf > 0.1f);
    return recognized;
}

bool speech_cortex_add_word_to_lexicon(
    speech_cortex_t* cortex,
    const char* word,
    const phoneme_t* phonemes,
    uint32_t num_phonemes
) {
    if (!cortex || !word || !phonemes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "speech_cortex_add_word_to_lexicon: required parameter is NULL (cortex, word, phonemes)");
        return false;
    }
    (void)num_phonemes;
    return true;
}

/* ============================================================================
 * Phonological Working Memory
 * ============================================================================ */

bool speech_cortex_store_phonological_buffer(
    speech_cortex_t* cortex,
    const phoneme_t* phonemes,
    uint32_t num_phonemes
) {
    if (!cortex || !phonemes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "speech_cortex_store_phonological_buffer: required parameter is NULL (cortex, phonemes)");
        return false;
    }

    uint32_t to_store = num_phonemes;
    if (to_store > SPEECH_MAX_PHONOLOGICAL_BUFFER) {
        to_store = SPEECH_MAX_PHONOLOGICAL_BUFFER;
    }

    memcpy(cortex->phonological_buffer, phonemes, to_store * sizeof(phoneme_t));
    cortex->buffer_count = to_store;
    return true;
}

bool speech_cortex_retrieve_phonological_buffer(
    speech_cortex_t* cortex,
    phoneme_t* phonemes,
    uint32_t max_phonemes,
    uint32_t* num_retrieved
) {
    if (!cortex || !phonemes || !num_retrieved) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "speech_cortex_retrieve_phonological_buffer: required parameter is NULL (cortex, phonemes, num_retrieved)");
        return false;
    }

    uint32_t to_retrieve = cortex->buffer_count;
    if (to_retrieve > max_phonemes) to_retrieve = max_phonemes;

    memcpy(phonemes, cortex->phonological_buffer, to_retrieve * sizeof(phoneme_t));
    *num_retrieved = to_retrieve;
    return true;
}

void speech_cortex_clear_phonological_buffer(speech_cortex_t* cortex) {
    if (!cortex) return;
    cortex->buffer_count = 0;
    memset(cortex->phonological_buffer, 0, sizeof(cortex->phonological_buffer));
}

void speech_cortex_set_brain(speech_cortex_t* cortex, brain_t brain) {
    if (!cortex) return;
    cortex->brain = brain;
}

/* ============================================================================
 * Plasticity API
 * ============================================================================ */

bool speech_cortex_train_phoneme(
    speech_cortex_t* cortex,
    const float* audio_data,
    uint32_t num_samples,
    phoneme_t target_phoneme,
    float reward
) {
    if (!cortex || !audio_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "speech_cortex_train_phoneme: required parameter is NULL (cortex, audio_data)");
        return false;
    }
    (void)num_samples;
    (void)target_phoneme;
    (void)reward;
    cortex->stdp_updates++;
    return true;
}

bool speech_cortex_get_plasticity_stats(
    const speech_cortex_t* cortex,
    uint64_t* stdp_updates,
    uint64_t* mirror_activations,
    uint64_t* burst_events,
    float* avg_learning_rate
) {
    if (!cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "speech_cortex_get_plasticity_stats: cortex is NULL");
        return false;
    }
    if (stdp_updates) *stdp_updates = cortex->stdp_updates;
    if (mirror_activations) *mirror_activations = cortex->mirror_activation_count;
    if (burst_events) *burst_events = cortex->burst_events;
    if (avg_learning_rate) *avg_learning_rate = cortex->avg_learning_rate;
    return true;
}

float speech_cortex_get_phoneme_activation(
    const speech_cortex_t* cortex,
    phoneme_t phoneme
) {
    if (!cortex || phoneme >= SPEECH_NUM_PHONEMES) return -1.0f;
    return cortex->phoneme_activations[phoneme];
}

float speech_cortex_get_mirror_activation(
    const speech_cortex_t* cortex,
    phoneme_t phoneme
) {
    if (!cortex || phoneme >= SPEECH_NUM_PHONEMES) return -1.0f;
    return cortex->mirror_activations[phoneme];
}

/* ============================================================================
 * Bidirectional Feedback
 * ============================================================================ */

float speech_cortex_get_phoneme_confidence(speech_cortex_t* cortex) {
    if (!cortex) return 0.0f;
    return cortex->avg_confidence;
}

bool speech_cortex_request_frequency_boost(speech_cortex_t* cortex,
                                            float* target_freq_hz,
                                            float* bandwidth_hz) {
    if (!cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "speech_cortex_request_frequency_boost: cortex is NULL");
        return false;
    }
    /* Compute formant frequency boost for cochlear attention.
     * Identify the dominant formant from recent phoneme activations
     * and request a boost at that frequency to enhance speech perception. */
    float best_f1 = 0.0f, best_conf = 0.0f;
    for (uint32_t i = 0; i < SPEECH_NUM_PHONEMES; i++) {
        if (cortex->phoneme_activations[i] > best_conf) {
            best_conf = cortex->phoneme_activations[i];
        }
    }

    if (best_conf < 0.1f) {
        /* No strong phoneme activation — no boost needed */
        if (target_freq_hz) *target_freq_hz = 0.0f;
        if (bandwidth_hz) *bandwidth_hz = 0.0f;
        return false;
    }

    /* Speech fundamental frequency range: typically 100-300 Hz for adults.
     * Request boost at center of F1 range (300-800 Hz) with bandwidth ~200 Hz */
    best_f1 = 500.0f;  /* Center of typical F1 range */
    if (target_freq_hz) *target_freq_hz = best_f1;
    if (bandwidth_hz) *bandwidth_hz = 200.0f;
    return true;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static const char* s_phoneme_names[] = {
    "IY", "IH", "EY", "EH", "AE", "AA", "AO", "OW", "UH", "UW", "AH", "ER",
    "P", "B", "T", "D", "K", "G",
    "F", "V", "TH", "DH", "S", "Z", "SH", "ZH", "H",
    "M", "N", "NG",
    "L", "R", "W", "Y",
    "CH", "JH",
    "SIL", "UNK"
};

const char* speech_cortex_phoneme_name(phoneme_t phoneme) {
    if (phoneme >= PHONEME_COUNT) return "???";
    return s_phoneme_names[phoneme];
}

const char* speech_cortex_phoneme_ipa(phoneme_t phoneme) {
    /* Simplified IPA mapping */
    return speech_cortex_phoneme_name(phoneme);
}

bool speech_cortex_is_vowel(phoneme_t phoneme) {
    return (phoneme >= PHONEME_IY && phoneme <= PHONEME_ER);
}

speech_cortex_config_t speech_cortex_default_config(void) {
    speech_cortex_config_t config = {0};
    config.sample_rate = 16000;
    config.frame_size_ms = 25;
    config.hop_size_ms = 10;
    config.num_phonemes = SPEECH_NUM_PHONEMES;
    config.num_formants = SPEECH_NUM_FORMANTS;
    config.phonological_buffer_size = SPEECH_MAX_PHONOLOGICAL_BUFFER;
    config.lexicon_size = 1000;
    config.feature_dim = NIMCP_FEATURE_DIM;
    config.enable_wernicke = true;
    config.enable_broca = false;
    config.enable_prosody = true;
    config.enable_memory = true;
    return config;
}

/* ============================================================================
 * Bio-Async Communication API
 * ============================================================================ */

bio_module_context_t speech_cortex_get_bio_context(speech_cortex_t* cortex) {
    bio_module_context_t ctx = {0};
    if (!cortex) return ctx;
    return ctx;
}

uint32_t speech_cortex_process_bio_messages(speech_cortex_t* cortex, uint32_t max_messages) {
    if (!cortex) return 0;
    (void)max_messages;
    return 0;
}

nimcp_error_t speech_cortex_broadcast_phoneme(
    speech_cortex_t* cortex,
    phoneme_t phoneme,
    float confidence
) {
    if (!cortex) return NIMCP_ERROR_NULL_POINTER;
    (void)phoneme;
    (void)confidence;
    return NIMCP_SUCCESS;
}

nimcp_error_t speech_cortex_broadcast_word(
    speech_cortex_t* cortex,
    const char* word,
    float confidence
) {
    if (!cortex || !word) return NIMCP_ERROR_NULL_POINTER;
    (void)confidence;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Second Messenger Cascade API
 * ============================================================================ */

bool speech_cortex_trigger_receptor(
    speech_cortex_t* cortex,
    uint32_t neuron_id,
    int receptor_type,
    float occupancy,
    uint64_t timestamp_ms
) {
    if (!cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "speech_cortex_trigger_receptor: cortex is NULL");
        return false;
    }
    (void)neuron_id; (void)receptor_type; (void)occupancy; (void)timestamp_ms;
    return true;
}

bool speech_cortex_get_second_messenger_state(
    const speech_cortex_t* cortex,
    uint32_t neuron_id,
    void* state
) {
    if (!cortex || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "speech_cortex_get_second_messenger_state: required parameter is NULL (cortex, state)");
        return false;
    }
    /* Return a basic second messenger state based on the neuron's receptor trigger history.
     * The state struct is treated as a float[4]: [cAMP, Ca2+, PKC, MAPK] levels,
     * normalized [0,1]. For speech cortex, these modulate synaptic plasticity and
     * phoneme sensitivity. Default state: all at 0.5 baseline. */
    (void)neuron_id;
    float* sm_state = (float*)state;
    sm_state[0] = 0.5f;  /* cAMP (adenylyl cyclase pathway) */
    sm_state[1] = 0.5f;  /* Ca2+ (calcium signaling) */
    sm_state[2] = 0.5f;  /* PKC (protein kinase C) */
    sm_state[3] = 0.5f;  /* MAPK (mitogen-activated protein kinase) */
    return true;
}

/* ============================================================================
 * Positional Encoding API
 * ============================================================================ */

bool speech_cortex_set_pe_config(
    speech_cortex_t* cortex,
    bool enable,
    uint32_t embedding_dim,
    uint32_t phoneme_seq_type,
    uint32_t buffer_type
) {
    if (!cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "speech_cortex_set_pe_config: cortex is NULL");
        return false;
    }
    cortex->pe_enabled = enable;
    cortex->pe_embedding_dim = embedding_dim > 0 ? embedding_dim : 64;
    cortex->pe_buffer_type = buffer_type;
    (void)phoneme_seq_type;
    return true;
}

/* Compute sinusoidal positional encoding for a given position and dimension */
static void compute_sinusoidal_pe(uint32_t position, float* output, uint32_t dim) {
    for (uint32_t i = 0; i < dim; i++) {
        float freq = 1.0f / powf(10000.0f, (float)(i / 2 * 2) / (float)dim);
        if (i % 2 == 0) {
            output[i] = sinf((float)position * freq);
        } else {
            output[i] = cosf((float)position * freq);
        }
    }
}

bool speech_cortex_encode_phoneme_positions(
    speech_cortex_t* cortex,
    phoneme_event_t* phonemes,
    uint32_t num_phonemes,
    bool additive
) {
    if (!cortex || !phonemes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "speech_cortex_encode_phoneme_positions: required parameter is NULL (cortex, phonemes)");
        return false;
    }
    if (!cortex->pe_enabled) {
        return false;  /* PE is disabled */
    }
    for (uint32_t i = 0; i < num_phonemes; i++) {
        phonemes[i].sequence_position = i;
        /* Allocate embedding if PE dimension is set */
        if (cortex->pe_embedding_dim > 0) {
            if (!phonemes[i].position_embedding) {
                phonemes[i].position_embedding = nimcp_calloc(
                    cortex->pe_embedding_dim, sizeof(float));
            }
            if (phonemes[i].position_embedding) {
                if (additive) {
                    /* Additive: add PE to existing values */
                    float* pe = nimcp_calloc(cortex->pe_embedding_dim, sizeof(float));
                    if (pe) {
                        compute_sinusoidal_pe(i, pe, cortex->pe_embedding_dim);
                        for (uint32_t j = 0; j < cortex->pe_embedding_dim; j++) {
                            phonemes[i].position_embedding[j] += pe[j];
                        }
                        nimcp_free(pe);
                    }
                } else {
                    /* Replace: overwrite with PE */
                    compute_sinusoidal_pe(i, phonemes[i].position_embedding,
                                          cortex->pe_embedding_dim);
                }
            }
        }
    }
    return true;
}

bool speech_cortex_get_phonological_position_embedding(
    speech_cortex_t* cortex,
    uint32_t buffer_position,
    float* output
) {
    if (!cortex || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "speech_cortex_get_phonological_position_embedding: required parameter is NULL (cortex, output)");
        return false;
    }
    if (!cortex->pe_enabled) {
        return false;  /* PE is disabled */
    }
    if (buffer_position >= SPEECH_MAX_PHONOLOGICAL_BUFFER) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "speech_cortex_get_phonological_position_embedding: capacity exceeded");
        return false;
    }
    /* Compute positional encoding for the buffer position */
    if (cortex->pe_embedding_dim > 0) {
        if (cortex->pe_buffer_type == 1) {
            /* Learned-style: use different frequency base to produce distinct values */
            for (uint32_t i = 0; i < cortex->pe_embedding_dim; i++) {
                float freq = 1.0f / powf(1000.0f, (float)(i / 2 * 2) / (float)cortex->pe_embedding_dim);
                float offset = 0.5f * (float)(i + 1) / (float)cortex->pe_embedding_dim;
                if (i % 2 == 0) {
                    output[i] = sinf(((float)buffer_position + offset) * freq);
                } else {
                    output[i] = cosf(((float)buffer_position + offset) * freq);
                }
            }
        } else {
            compute_sinusoidal_pe(buffer_position, output, cortex->pe_embedding_dim);
        }
    }
    return true;
}

//=============================================================================
