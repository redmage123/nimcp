//=============================================================================
// nimcp_pr_speech_bridge.c - Prime Resonant Speech Bridge Implementation
//=============================================================================
/**
 * @file nimcp_pr_speech_bridge.c
 * @brief Integration bridge between Speech Cortex and Prime Resonant Memory
 * @version 1.0.0
 * @date 2026-01-09
 *
 * Implementation of the bidirectional bridge linking speech processing
 * to Prime Resonant memory encoding, enabling phonemic patterns to be
 * stored as prime-resonant memories with prosody-informed quaternion states.
 */

#include "cognitive/memory/core/nimcp_pr_speech_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pr_speech_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_pr_speech_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_pr_speech_bridge_mesh_registry = NULL;

nimcp_error_t pr_speech_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_pr_speech_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "pr_speech_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_MEMORY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "pr_speech_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_pr_speech_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_pr_speech_bridge_mesh_registry = registry;
    return err;
}

void pr_speech_bridge_mesh_unregister(void) {
    if (g_pr_speech_bridge_mesh_registry && g_pr_speech_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_pr_speech_bridge_mesh_registry, g_pr_speech_bridge_mesh_id);
        g_pr_speech_bridge_mesh_id = 0;
        g_pr_speech_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from pr_speech_bridge module (instance-level) */
static inline void pr_speech_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_pr_speech_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_pr_speech_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_pr_speech_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "PR_SPEECH_BRIDGE"


//=============================================================================
// First 64 Prime Numbers for Phoneme Mapping
//=============================================================================

/**
 * First 64 primes used for phoneme-to-prime mapping.
 * Each English phoneme (44 total) maps to a unique prime.
 */
static const uint64_t PHONEME_PRIMES[64] = {
    2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53,
    59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131,
    137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223,
    227, 229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293, 307, 311
};

/**
 * Phoneme frequency estimates in English (approximate values from corpus studies)
 */
static const float PHONEME_FREQUENCIES[44] = {
    0.035f, 0.040f, 0.025f, 0.035f, 0.038f, 0.030f,  // Vowels: IY, IH, EY, EH, AE, AA
    0.025f, 0.028f, 0.022f, 0.020f, 0.050f, 0.032f,  // Vowels: AO, OW, UH, UW, AH, ER
    0.018f, 0.022f, 0.045f, 0.035f, 0.025f, 0.015f,  // Stops: P, B, T, D, K, G
    0.015f, 0.018f, 0.012f, 0.020f, 0.048f, 0.022f,  // Fricatives: F, V, TH, DH, S, Z
    0.015f, 0.008f, 0.018f,                           // Fricatives: SH, ZH, H
    0.030f, 0.055f, 0.020f,                           // Nasals: M, N, NG
    0.042f, 0.040f, 0.022f, 0.012f,                   // Approximants: L, R, W, Y
    0.010f, 0.012f,                                   // Affricates: CH, JH
    0.100f, 0.005f                                     // Special: SILENCE, UNKNOWN
};

/**
 * Vowel classification for phonemes
 */
static const bool PHONEME_IS_VOWEL[44] = {
    true, true, true, true, true, true, true, true, true, true, true, true,  // Vowels (0-11)
    false, false, false, false, false, false,                                 // Stops (12-17)
    false, false, false, false, false, false, false, false, false,           // Fricatives (18-26)
    false, false, false,                                                      // Nasals (27-29)
    false, false, false, false,                                               // Approximants (30-33)
    false, false,                                                             // Affricates (34-35)
    false, false                                                              // Special (36-37)
};

//=============================================================================
// Internal Helper Functions - Forward Declarations
//=============================================================================

static uint64_t pr_speech_get_current_time_ms(void);
static float pr_speech_clamp(float value, float min, float max);
static float pr_speech_normalize_phase(float phase);
static void pr_speech_reset_prosody_accumulator(pr_speech_bridge_t* bridge);
static pr_speech_error_t pr_speech_finalize_word(pr_speech_bridge_t* bridge);

//=============================================================================
// Configuration Functions
//=============================================================================

pr_speech_bridge_config_t pr_speech_bridge_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_config_default", 0.0f);


    pr_speech_bridge_config_t config = {
        // Signature generation
        .position_encoding_bits = 4,
        .position_decay_factor = 0.85f,
        .enable_syllable_weighting = true,

        // Quaternion mapping
        .consolidation_base = 0.5f,
        .emotion_sensitivity = 0.5f,
        .salience_content_weight = 0.8f,
        .accessibility_base = 0.7f,

        // Memory encoding
        .default_tier = PR_MEMORY_TIER_Z1,
        .auto_create_entanglements = true,
        .entanglement_threshold = 0.6f,

        // Theta-gamma integration
        .enable_theta_gamma = true,
        .theta_frequency_hz = PR_SPEECH_THETA_FREQ_HZ,
        .gamma_frequency_hz = PR_SPEECH_GAMMA_FREQ_HZ,

        // FEP integration
        .enable_fep_updates = true,
        .word_boundary_pe_threshold = PR_SPEECH_WORD_BOUNDARY_PE,

        // Retrieval
        .max_retrieval_results = 10,
        .retrieval_threshold = 0.3f
    };
    return config;
}

bool pr_speech_bridge_config_validate(const pr_speech_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_speech_bridge_config_validate: config is NULL");
        return false;
    }

    // Validate position encoding bits (1-8)
    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_config_validate", 0.0f);


    if (config->position_encoding_bits < 1 || config->position_encoding_bits > 8) {
        return false;
    }

    // Validate decay factor (0-1)
    if (config->position_decay_factor < 0.0f || config->position_decay_factor > 1.0f) {
        return false;
    }

    // Validate quaternion parameters
    if (config->consolidation_base < 0.0f || config->consolidation_base > 1.0f) {
        return false;
    }
    if (config->emotion_sensitivity < 0.0f || config->emotion_sensitivity > 2.0f) {
        return false;
    }
    if (config->salience_content_weight < 0.0f || config->salience_content_weight > 1.0f) {
        return false;
    }
    if (config->accessibility_base < 0.0f || config->accessibility_base > 1.0f) {
        return false;
    }

    // Validate entanglement threshold
    if (config->entanglement_threshold < 0.0f || config->entanglement_threshold > 1.0f) {
        return false;
    }

    // Validate theta-gamma frequencies
    if (config->enable_theta_gamma) {
        if (config->theta_frequency_hz < 1.0f || config->theta_frequency_hz > 20.0f) {
            return false;
        }
        if (config->gamma_frequency_hz < 10.0f || config->gamma_frequency_hz > 150.0f) {
            return false;
        }
    }

    // Validate FEP threshold
    if (config->word_boundary_pe_threshold < 0.0f || config->word_boundary_pe_threshold > 1.0f) {
        return false;
    }

    // Validate retrieval parameters
    if (config->max_retrieval_results == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_speech_bridge_config_validate: config->max_retrieval_results is zero");
        return false;
    }
    if (config->retrieval_threshold < 0.0f || config->retrieval_threshold > 1.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

pr_speech_bridge_t* pr_speech_bridge_create(const pr_speech_bridge_config_t* config) {
    // Use default config if none provided
    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_create", 0.0f);


    pr_speech_bridge_config_t cfg;
    if (config) {
        if (!pr_speech_bridge_config_validate(config)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_speech_bridge_create: pr_speech_bridge_config_validate is NULL");
            return NULL;
        }
        cfg = *config;
    } else {
        cfg = pr_speech_bridge_config_default();
    }

    // Allocate bridge structure
    pr_speech_bridge_t* bridge = (pr_speech_bridge_t*)nimcp_calloc(1, sizeof(pr_speech_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    // Initialize configuration
    bridge->config = cfg;

    // Initialize phoneme-to-prime mapping table
    pr_speech_error_t err = pr_speech_bridge_init_phoneme_primes(bridge);
    if (err != PR_SPEECH_SUCCESS) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_speech_bridge_create: validation failed");
        return NULL;
    }

    // Initialize prime signature config
    bridge->sig_config = prime_sig_default_config();

    // Allocate word signature cache
    bridge->word_capacity = PR_SPEECH_MAX_WORD_SIGNATURES;
    bridge->word_signatures = (pr_word_signature_t*)nimcp_calloc(
        bridge->word_capacity, sizeof(pr_word_signature_t));
    if (!bridge->word_signatures) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_speech_bridge_create: bridge->word_signatures is NULL");
        return NULL;
    }
    bridge->word_count = 0;

    // Initialize state
    memset(&bridge->state, 0, sizeof(pr_speech_bridge_state_t));
    bridge->state.current_speech_quat = quat_identity();

    // Initialize statistics
    memset(&bridge->stats, 0, sizeof(pr_speech_bridge_stats_t));

    // Mark as initialized (but not connected)
    bridge->initialized = true;
    bridge->speech_cortex = NULL;
    bridge->fep_bridge = NULL;
    bridge->current_speech_memory = NULL;

    NIMCP_LOGGING_INFO("Created %s bridge", "pr_speech");
    return bridge;
}

void pr_speech_bridge_destroy(pr_speech_bridge_t* bridge) {
    if (!bridge) {
        return;
        NIMCP_LOGGING_DEBUG("Destroying %s bridge", "pr_speech");
    }

    // Free word signatures cache
    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_destroy", 0.0f);


    if (bridge->word_signatures) {
        nimcp_free(bridge->word_signatures);
        bridge->word_signatures = NULL;
    }

    // Note: We don't own speech_cortex, fep_bridge, node_manager, etc.
    // They should be destroyed by their respective owners

    bridge->initialized = false;
    nimcp_free(bridge);
    bridge = NULL;
}

pr_speech_error_t pr_speech_bridge_reset(pr_speech_bridge_t* bridge) {
    if (!bridge) {
        return PR_SPEECH_ERROR_NULL_POINTER;
    }

    // Clear phoneme buffer
    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_reset", 0.0f);


    memset(bridge->state.phoneme_buffer, 0, sizeof(bridge->state.phoneme_buffer));
    bridge->state.buffer_pos = 0;

    // Clear current word
    memset(bridge->state.current_word_phonemes, 0, sizeof(bridge->state.current_word_phonemes));
    bridge->state.current_word_length = 0;

    // Reset prosody accumulator
    pr_speech_reset_prosody_accumulator(bridge);

    // Reset quaternion state
    bridge->state.current_speech_quat = quat_identity();

    // Reset phases
    bridge->state.theta_phase = 0.0f;
    bridge->state.gamma_phase = 0.0f;

    // Reset FEP state
    bridge->state.current_pe = 0.0f;
    bridge->state.avg_pe = 0.0f;

    // Reset timing
    bridge->state.last_phoneme_time_ms = 0;
    bridge->state.word_start_time_ms = 0;

    // Reset flags
    bridge->state.word_in_progress = false;
    bridge->state.waiting_for_boundary = false;

    // Reset current memory
    bridge->current_speech_memory = NULL;

    return PR_SPEECH_SUCCESS;
}

//=============================================================================
// Connection Functions
//=============================================================================

pr_speech_error_t pr_speech_bridge_connect_speech_cortex(
    pr_speech_bridge_t* bridge,
    speech_cortex_t* cortex
) {
    if (!bridge) {
        return PR_SPEECH_ERROR_NULL_POINTER;
    }
    // cortex can be NULL to disconnect
    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_connect_speech_corte", 0.0f);


    bridge->speech_cortex = cortex;
    return PR_SPEECH_SUCCESS;
}

pr_speech_error_t pr_speech_bridge_connect_fep_bridge(
    pr_speech_bridge_t* bridge,
    speech_cortex_fep_bridge_t* fep_bridge
) {
    if (!bridge) {
        return PR_SPEECH_ERROR_NULL_POINTER;
    }
    // fep_bridge can be NULL to disconnect
    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_connect_fep_bridge", 0.0f);


    bridge->fep_bridge = fep_bridge;
    return PR_SPEECH_SUCCESS;
}

pr_speech_error_t pr_speech_bridge_connect_node_manager(
    pr_speech_bridge_t* bridge,
    pr_node_manager_t manager
) {
    if (!bridge) {
        return PR_SPEECH_ERROR_NULL_POINTER;
    }
    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_connect_node_manager", 0.0f);


    bridge->node_manager = manager;
    return PR_SPEECH_SUCCESS;
}

pr_speech_error_t pr_speech_bridge_connect_entanglement(
    pr_speech_bridge_t* bridge,
    entangle_graph_t graph
) {
    if (!bridge) {
        return PR_SPEECH_ERROR_NULL_POINTER;
    }
    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_connect_entanglement", 0.0f);


    bridge->entanglement_graph = graph;
    return PR_SPEECH_SUCCESS;
}

pr_speech_error_t pr_speech_bridge_connect_theta_gamma(
    pr_speech_bridge_t* bridge,
    theta_gamma_manager_t theta_gamma
) {
    if (!bridge) {
        return PR_SPEECH_ERROR_NULL_POINTER;
    }
    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_connect_theta_gamma", 0.0f);


    bridge->theta_gamma = theta_gamma;
    return PR_SPEECH_SUCCESS;
}

//=============================================================================
// Phoneme Processing Functions
//=============================================================================

pr_speech_error_t pr_speech_bridge_process_phoneme(
    pr_speech_bridge_t* bridge,
    const phoneme_event_t* event
) {
    if (!bridge || !event) {
        return PR_SPEECH_ERROR_NULL_POINTER;
    }

    if (!bridge->initialized) {
        return PR_SPEECH_ERROR_INVALID_CONFIG;
    }

    // Validate phoneme
    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_process_phoneme", 0.0f);


    if (event->phoneme < 0 || event->phoneme >= PHONEME_COUNT) {
        return PR_SPEECH_ERROR_INVALID_PHONEME;
    }

    // Check buffer space
    if (bridge->state.buffer_pos >= PR_SPEECH_MAX_PHONEME_BUFFER) {
        return PR_SPEECH_ERROR_BUFFER_FULL;
    }

    uint64_t current_time = pr_speech_get_current_time_ms();

    // Handle silence phoneme as potential word boundary
    if (event->phoneme == PHONEME_SILENCE) {
        if (bridge->state.word_in_progress && bridge->state.current_word_length > 0) {
            // Finalize current word
            pr_speech_error_t err = pr_speech_finalize_word(bridge);
            if (err != PR_SPEECH_SUCCESS) {
                return err;
            }
        }
        bridge->state.last_phoneme_time_ms = current_time;
        return PR_SPEECH_SUCCESS;
    }

    // Start new word if needed
    if (!bridge->state.word_in_progress) {
        bridge->state.word_in_progress = true;
        bridge->state.word_start_time_ms = current_time;
        bridge->state.current_word_length = 0;
        pr_speech_reset_prosody_accumulator(bridge);
    }

    // Add phoneme to current word buffer
    if (bridge->state.current_word_length < PR_SPEECH_MAX_WORD_PHONEMES) {
        bridge->state.current_word_phonemes[bridge->state.current_word_length] = event->phoneme;
        bridge->state.current_word_length++;
    }

    // Store event in phoneme buffer
    bridge->state.phoneme_buffer[bridge->state.buffer_pos] = *event;
    bridge->state.buffer_pos++;

    // Update prosody accumulator from phoneme features
    if (event->features.pitch_hz > 0.0f) {
        pr_speech_bridge_update_prosody(
            bridge,
            event->features.pitch_hz,
            event->features.intensity_db,
            event->features.stress_level
        );
    }

    // Update timing
    bridge->state.last_phoneme_time_ms = current_time;

    // Update statistics
    bridge->stats.phonemes_processed++;

    return PR_SPEECH_SUCCESS;
}

size_t pr_speech_bridge_process_phonemes(
    pr_speech_bridge_t* bridge,
    const phoneme_event_t* events,
    size_t count
) {
    if (!bridge || !events || count == 0) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_process_phonemes", 0.0f);


    size_t processed = 0;
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_speech_bridge_heartbeat("pr_speech_br_loop",
                             (float)(i + 1) / (float)count);
        }

        pr_speech_error_t err = pr_speech_bridge_process_phoneme(bridge, &events[i]);
        if (err != PR_SPEECH_SUCCESS) {
            break;
        }
        processed++;
    }
    return processed;
}

pr_memory_node_t* pr_speech_bridge_process_word(
    pr_speech_bridge_t* bridge,
    const char* word_text,
    const phoneme_t* phonemes,
    uint32_t phoneme_count,
    pr_word_type_t word_type
) {
    if (!bridge || !phonemes || phoneme_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_speech_bridge_process_word: required parameter is NULL (bridge, phonemes)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_process_word", 0.0f);


    if (phoneme_count > PR_SPEECH_MAX_WORD_PHONEMES) {
        phoneme_count = PR_SPEECH_MAX_WORD_PHONEMES;
    }

    // Compute word signature
    pr_word_signature_t word_sig;
    memset(&word_sig, 0, sizeof(word_sig));

    pr_speech_error_t err = pr_speech_bridge_compute_word_prime_sig(
        bridge, word_text, phonemes, phoneme_count, word_type, &word_sig);
    if (err != PR_SPEECH_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_speech_bridge_process_word: validation failed");
        return NULL;
    }

    // Compute quaternion from current prosody
    pr_prosody_features_t prosody;
    pr_speech_bridge_get_current_prosody(bridge, &prosody);

    nimcp_quaternion_t quat = pr_speech_bridge_compute_speech_quaternion(
        bridge, &prosody, word_type, word_sig.frequency, word_sig.neighborhood_density);

    // Encode to memory
    pr_memory_node_t* node = pr_speech_bridge_encode_word_memory(bridge, &word_sig, quat);

    // Update statistics
    if (node) {
        bridge->stats.words_encoded++;

        // Update average word length
        float total = bridge->stats.avg_word_length * (bridge->stats.words_encoded - 1);
        bridge->stats.avg_word_length = (total + phoneme_count) / bridge->stats.words_encoded;
    }

    return node;
}

//=============================================================================
// Signature Computation Functions
//=============================================================================

pr_speech_error_t pr_speech_bridge_compute_phoneme_prime_sig(
    pr_speech_bridge_t* bridge,
    const phoneme_t* phonemes,
    size_t count,
    prime_signature_t* signature
) {
    if (!bridge || !phonemes || !signature) {
        return PR_SPEECH_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_compute_phoneme_prim", 0.0f);


    if (count == 0) {
        return PR_SPEECH_ERROR_INVALID_CONFIG;
    }

    // Initialize signature
    memset(signature, 0, sizeof(prime_signature_t));

    // Copy prime values
    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            pr_speech_bridge_heartbeat("pr_speech_br_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        signature->primes[i] = PHONEME_PRIMES[i];
    }

    // Process each phoneme with position encoding
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_speech_bridge_heartbeat("pr_speech_br_loop",
                             (float)(i + 1) / (float)count);
        }

        phoneme_t p = phonemes[i];

        // Skip invalid phonemes
        if (p < 0 || p >= PR_SPEECH_NUM_PHONEMES) {
            continue;
        }

        // Get prime index for this phoneme
        size_t prime_idx = (size_t)p % PRIME_SIG_DIM;

        // Compute position weight
        float pos_weight = pr_speech_bridge_position_weight(
            (uint32_t)i, (uint32_t)count, bridge->config.position_decay_factor);

        // Add weighted exponent (quantize to uint8_t)
        uint8_t weight_increment = (uint8_t)(pos_weight * 16.0f);  // Scale to 0-16
        if (weight_increment == 0) weight_increment = 1;

        // Saturating add
        uint16_t new_val = signature->exponents[prime_idx] + weight_increment;
        signature->exponents[prime_idx] = (new_val > 255) ? 255 : (uint8_t)new_val;
    }

    // Recount factors and compute hash
    signature->num_factors = 0;
    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            pr_speech_bridge_heartbeat("pr_speech_br_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        if (signature->exponents[i] > 0) {
            signature->num_factors++;
        }
    }

    // Simple FNV-1a hash of exponents
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            pr_speech_bridge_heartbeat("pr_speech_br_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        hash ^= signature->exponents[i];
        hash *= 1099511628211ULL;
    }
    signature->hash = hash;

    // Update statistics
    bridge->stats.signatures_computed++;

    return PR_SPEECH_SUCCESS;
}

pr_speech_error_t pr_speech_bridge_compute_word_prime_sig(
    pr_speech_bridge_t* bridge,
    const char* word_text,
    const phoneme_t* phonemes,
    size_t count,
    pr_word_type_t word_type,
    pr_word_signature_t* word_sig
) {
    if (!bridge || !phonemes || !word_sig) {
        return PR_SPEECH_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_compute_word_prime_s", 0.0f);


    memset(word_sig, 0, sizeof(pr_word_signature_t));

    // Compute base signature
    pr_speech_error_t err = pr_speech_bridge_compute_phoneme_prime_sig(
        bridge, phonemes, count, &word_sig->signature);
    if (err != PR_SPEECH_SUCCESS) {
        return err;
    }

    // Generate word ID from signature hash
    word_sig->word_id = word_sig->signature.hash;

    // Copy word text if provided
    if (word_text) {
        strncpy(word_sig->word_text, word_text, sizeof(word_sig->word_text) - 1);
        word_sig->word_text[sizeof(word_sig->word_text) - 1] = '\0';
    }

    // Copy phoneme sequence
    word_sig->phoneme_count = (count > PR_SPEECH_MAX_WORD_PHONEMES)
                              ? PR_SPEECH_MAX_WORD_PHONEMES
                              : (uint32_t)count;
    for (uint32_t i = 0; i < word_sig->phoneme_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && word_sig->phoneme_count > 256) {
            pr_speech_bridge_heartbeat("pr_speech_br_loop",
                             (float)(i + 1) / (float)word_sig->phoneme_count);
        }

        word_sig->phonemes[i] = phonemes[i];
    }

    // Set word type
    word_sig->word_type = word_type;

    // Estimate word frequency (simplified - could use lexicon lookup)
    // Average frequency of constituent phonemes as approximation
    float freq_sum = 0.0f;
    for (size_t i = 0; i < count && i < PR_SPEECH_NUM_PHONEMES; i++) {
        if (phonemes[i] >= 0 && phonemes[i] < PR_SPEECH_NUM_PHONEMES) {
            freq_sum += PHONEME_FREQUENCIES[phonemes[i]];
        }
    }
    word_sig->frequency = freq_sum / (float)count;

    // Estimate neighborhood density based on phoneme count
    // Shorter words tend to have more neighbors
    word_sig->neighborhood_density = 1.0f - (count / (float)PR_SPEECH_MAX_WORD_PHONEMES);
    word_sig->neighborhood_density = pr_speech_clamp(word_sig->neighborhood_density, 0.1f, 0.9f);

    // Estimate syllable count (rough: one per vowel)
    word_sig->syllable_count = 0;
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_speech_bridge_heartbeat("pr_speech_br_loop",
                             (float)(i + 1) / (float)count);
        }

        if (phonemes[i] >= 0 && phonemes[i] < PR_SPEECH_NUM_PHONEMES &&
            PHONEME_IS_VOWEL[phonemes[i]]) {
            word_sig->syllable_count++;
        }
    }
    if (word_sig->syllable_count == 0) {
        word_sig->syllable_count = 1;  // Minimum 1 syllable
    }

    // Set creation time
    word_sig->created_time_ms = pr_speech_get_current_time_ms();

    return PR_SPEECH_SUCCESS;
}

uint64_t pr_speech_bridge_get_phoneme_prime(
    const pr_speech_bridge_t* bridge,
    phoneme_t phoneme
) {
    if (!bridge || phoneme < 0 || phoneme >= PR_SPEECH_NUM_PHONEMES) {
        return 0;
    }
    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_get_phoneme_prime", 0.0f);


    return bridge->phoneme_to_prime[phoneme].prime;
}

//=============================================================================
// Quaternion Computation Functions
//=============================================================================

nimcp_quaternion_t pr_speech_bridge_compute_speech_quaternion(
    pr_speech_bridge_t* bridge,
    const pr_prosody_features_t* prosody,
    pr_word_type_t word_type,
    float word_frequency,
    float neighborhood_density
) {
    if (!bridge) {
        return quat_identity();
    }

    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_compute_speech_quate", 0.0f);


    nimcp_quaternion_t q;

    // w = Consolidation: based on word frequency and base value
    // Higher frequency words have more established representations
    q.w = bridge->config.consolidation_base;
    if (word_frequency > 0.0f) {
        q.w += word_frequency * 0.3f;  // Frequency boost
    }
    q.w = pr_speech_clamp(q.w, 0.0f, 1.0f);

    // x = Emotion: derived from prosody
    // Rising pitch = positive, falling = negative
    // Higher stress = more intense (away from zero)
    q.x = 0.0f;
    if (prosody) {
        // Pitch contour contributes to valence
        q.x += prosody->pitch_contour_type * bridge->config.emotion_sensitivity;

        // Intensity/stress adds emotional intensity
        float intensity_factor = (prosody->stress_level - 0.5f) * 0.5f;
        if (q.x > 0) {
            q.x += intensity_factor;
        } else if (q.x < 0) {
            q.x -= intensity_factor;
        }

        // Speaking rate: faster = more excited/positive, slower = more negative
        if (prosody->speaking_rate_sps > 4.0f) {
            q.x += 0.1f;  // Fast speech tends positive
        } else if (prosody->speaking_rate_sps < 2.0f) {
            q.x -= 0.1f;  // Slow speech tends negative
        }
    }
    q.x = pr_speech_clamp(q.x, -1.0f, 1.0f);

    // y = Salience: based on word type and novelty
    switch (word_type) {
        case PR_WORD_TYPE_CONTENT:
            q.y = bridge->config.salience_content_weight;
            break;
        case PR_WORD_TYPE_PROPER:
            q.y = 0.9f;  // Proper nouns are highly salient
            break;
        case PR_WORD_TYPE_TECHNICAL:
            q.y = 0.85f;  // Technical terms are salient
            break;
        case PR_WORD_TYPE_FUNCTION:
            q.y = PR_SPEECH_FUNCTION_WORD_SALIENCE;
            break;
        default:
            q.y = 0.5f;
            break;
    }

    // Novelty boost: low frequency = more novel
    if (word_frequency < 0.01f) {
        q.y += 0.15f;  // Rare words more salient
    }

    // Stress boost
    if (prosody && prosody->stress_level > 0.7f) {
        q.y += 0.1f;
    }
    q.y = pr_speech_clamp(q.y, 0.0f, 1.0f);

    // z = Accessibility: based on familiarity and neighborhood density
    q.z = bridge->config.accessibility_base;

    // Higher frequency = more accessible
    q.z += word_frequency * 0.2f;

    // More neighbors = faster retrieval (higher accessibility)
    q.z += neighborhood_density * 0.15f;

    q.z = pr_speech_clamp(q.z, 0.0f, 1.0f);

    // Store current quaternion in bridge state
    bridge->state.current_speech_quat = q;

    return q;
}

pr_speech_error_t pr_speech_bridge_update_prosody(
    pr_speech_bridge_t* bridge,
    float pitch_hz,
    float intensity_db,
    float stress
) {
    if (!bridge) {
        return PR_SPEECH_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_update_prosody", 0.0f);


    pr_prosody_features_t* acc = &bridge->state.current_prosody;
    uint32_t n = bridge->state.prosody_sample_count;

    // Running average update
    if (n == 0) {
        acc->pitch_mean_hz = pitch_hz;
        acc->intensity_mean_db = intensity_db;
        acc->stress_level = stress;
        acc->pitch_range_hz = 0.0f;

        // Track initial values for slope calculation
        acc->pitch_slope = pitch_hz;  // Temporarily store first pitch
    } else {
        // Update means
        acc->pitch_mean_hz = (acc->pitch_mean_hz * n + pitch_hz) / (n + 1);
        acc->intensity_mean_db = (acc->intensity_mean_db * n + intensity_db) / (n + 1);
        acc->stress_level = (acc->stress_level * n + stress) / (n + 1);

        // Update range (track max deviation from mean)
        float deviation = fabsf(pitch_hz - acc->pitch_mean_hz);
        if (deviation > acc->pitch_range_hz) {
            acc->pitch_range_hz = deviation * 2.0f;  // Range = 2 * max deviation
        }

        // Update pitch slope (difference from first to last)
        float first_pitch = acc->pitch_slope;  // Was stored in first iteration
        if (n == 1) {
            // Second sample: compute initial slope
            acc->pitch_slope = pitch_hz - first_pitch;
        } else {
            // Exponential moving average of slope
            float instant_slope = pitch_hz - acc->pitch_mean_hz;
            acc->pitch_slope = acc->pitch_slope * 0.8f + instant_slope * 0.2f;
        }
    }

    bridge->state.prosody_sample_count = n + 1;

    // Compute pitch contour type from slope
    if (acc->pitch_slope > 10.0f) {
        acc->pitch_contour_type = 1.0f;   // Rising
    } else if (acc->pitch_slope < -10.0f) {
        acc->pitch_contour_type = -1.0f;  // Falling
    } else {
        acc->pitch_contour_type = 0.0f;   // Flat
    }

    return PR_SPEECH_SUCCESS;
}

pr_speech_error_t pr_speech_bridge_get_current_prosody(
    const pr_speech_bridge_t* bridge,
    pr_prosody_features_t* prosody
) {
    if (!bridge || !prosody) {
        return PR_SPEECH_ERROR_NULL_POINTER;
    }

    *prosody = bridge->state.current_prosody;
    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_get_current_prosody", 0.0f);


    return PR_SPEECH_SUCCESS;
}

//=============================================================================
// Memory Encoding Functions
//=============================================================================

pr_memory_node_t* pr_speech_bridge_encode_word_memory(
    pr_speech_bridge_t* bridge,
    const pr_word_signature_t* word_sig,
    nimcp_quaternion_t quaternion
) {
    if (!bridge || !word_sig) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_speech_bridge_encode_word_memory: required parameter is NULL (bridge, word_sig)");
        return NULL;
    }

    // Check if node manager is connected
    if (!bridge->node_manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_speech_bridge_encode_word_memory: bridge->node_manager is NULL");
        return NULL;
    }

    // Check theta-gamma phase if enabled
    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_encode_word_memory", 0.0f);


    if (bridge->config.enable_theta_gamma && !pr_speech_bridge_can_encode(bridge)) {
        // Not in encoding window - could queue or return NULL
        // For now, we'll proceed anyway but note it in stats
    }

    // Create node configuration
    pr_node_config_t node_cfg = {
        .initial_tier = bridge->config.default_tier,
        .initial_strength = quaternion.w,  // Consolidation as strength
        .emotional_valence = quaternion.x,
        .salience = quaternion.y,
        .accessibility = quaternion.z,
        .compute_signature = false,  // We provide our own signature
        .enable_cow = true
    };

    // Create memory node with word text as data
    pr_memory_node_t* node = pr_memory_node_create_with_signature(
        bridge->node_manager,
        word_sig->word_text,
        strlen(word_sig->word_text) + 1,
        &word_sig->signature,
        &node_cfg
    );

    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");


        return NULL;
    }

    // Set quaternion state
    pr_memory_node_update_state(node, quaternion);

    // Cache word signature if space available
    if (bridge->word_count < bridge->word_capacity) {
        bridge->word_signatures[bridge->word_count] = *word_sig;
        bridge->word_count++;
    }

    // Auto-create entanglements with similar words if enabled
    if (bridge->config.auto_create_entanglements && bridge->entanglement_graph) {
        for (size_t i = 0; i < bridge->word_count - 1; i++) {
            float sim = prime_sig_jaccard(&word_sig->signature,
                                          &bridge->word_signatures[i].signature);
            if (sim >= bridge->config.entanglement_threshold) {
                // Create entanglement link (would need entanglement API)
                bridge->stats.entanglements_created++;
            }
        }
    }

    // Store as current speech memory
    bridge->current_speech_memory = node;

    return node;
}

pr_memory_node_t* pr_speech_bridge_flush_word_buffer(pr_speech_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    if (!bridge->state.word_in_progress || bridge->state.current_word_length == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_speech_bridge_flush_word_buffer: bridge->state is NULL");
        return NULL;
    }

    // Process the buffered word
    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_flush_word_buffer", 0.0f);


    pr_memory_node_t* node = pr_speech_bridge_process_word(
        bridge,
        NULL,  // No text
        bridge->state.current_word_phonemes,
        bridge->state.current_word_length,
        PR_WORD_TYPE_UNKNOWN
    );

    // Reset word state
    bridge->state.word_in_progress = false;
    bridge->state.current_word_length = 0;
    pr_speech_reset_prosody_accumulator(bridge);

    return node;
}

//=============================================================================
// Memory Retrieval Functions
//=============================================================================

pr_speech_error_t pr_speech_bridge_retrieve_similar_words(
    pr_speech_bridge_t* bridge,
    const phoneme_t* phonemes,
    size_t count,
    pr_speech_retrieval_result_t* results,
    size_t max_results,
    size_t* result_count
) {
    if (!bridge || !phonemes || !results || !result_count) {
        return PR_SPEECH_ERROR_NULL_POINTER;
    }

    *result_count = 0;

    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_retrieve_similar_wor", 0.0f);


    if (count == 0 || max_results == 0) {
        return PR_SPEECH_SUCCESS;
    }

    // Check theta-gamma phase if enabled
    if (bridge->config.enable_theta_gamma && !pr_speech_bridge_can_retrieve(bridge)) {
        // Not in retrieval window
    }

    // Compute query signature
    prime_signature_t query_sig;
    pr_speech_error_t err = pr_speech_bridge_compute_phoneme_prime_sig(
        bridge, phonemes, count, &query_sig);
    if (err != PR_SPEECH_SUCCESS) {
        return err;
    }

    // Allocate temporary array for scored results
    typedef struct {
        size_t index;
        float score;
    } scored_entry_t;

    scored_entry_t* scored = (scored_entry_t*)nimcp_calloc(bridge->word_count, sizeof(scored_entry_t));
    if (!scored) {
        return PR_SPEECH_ERROR_NO_MEMORY;
    }

    // Score all cached words
    size_t valid_count = 0;
    for (size_t i = 0; i < bridge->word_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->word_count > 256) {
            pr_speech_bridge_heartbeat("pr_speech_br_loop",
                             (float)(i + 1) / (float)bridge->word_count);
        }

        float sim = prime_sig_jaccard(&query_sig, &bridge->word_signatures[i].signature);
        if (sim >= bridge->config.retrieval_threshold) {
            scored[valid_count].index = i;
            scored[valid_count].score = sim;
            valid_count++;
        }
    }

    // Simple selection sort for top-k (could use partial_sort for large k)
    size_t num_results = (valid_count < max_results) ? valid_count : max_results;
    for (size_t i = 0; i < num_results; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_results > 256) {
            pr_speech_bridge_heartbeat("pr_speech_br_loop",
                             (float)(i + 1) / (float)num_results);
        }

        size_t max_idx = i;
        for (size_t j = i + 1; j < valid_count; j++) {
            if (scored[j].score > scored[max_idx].score) {
                max_idx = j;
            }
        }
        if (max_idx != i) {
            scored_entry_t tmp = scored[i];
            scored[i] = scored[max_idx];
            scored[max_idx] = tmp;
        }

        // Fill result
        results[i].memory_id = bridge->word_signatures[scored[i].index].word_id;
        results[i].word_sig = bridge->word_signatures[scored[i].index];
        results[i].resonance_score = scored[i].score;
        results[i].jaccard_similarity = scored[i].score;
        results[i].quaternion_similarity = 1.0f;  // Would compute if needed
    }

    nimcp_free(scored);
    scored = NULL;

    *result_count = num_results;
    bridge->stats.words_retrieved += num_results;

    return PR_SPEECH_SUCCESS;
}

pr_speech_error_t pr_speech_bridge_retrieve_by_text(
    pr_speech_bridge_t* bridge,
    const char* text_pattern,
    pr_speech_retrieval_result_t* results,
    size_t max_results,
    size_t* result_count
) {
    if (!bridge || !text_pattern || !results || !result_count) {
        return PR_SPEECH_ERROR_NULL_POINTER;
    }

    *result_count = 0;
    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_retrieve_by_text", 0.0f);


    size_t pattern_len = strlen(text_pattern);

    for (size_t i = 0; i < bridge->word_count && *result_count < max_results; i++) {
        // Simple substring search
        if (strstr(bridge->word_signatures[i].word_text, text_pattern) != NULL) {
            results[*result_count].memory_id = bridge->word_signatures[i].word_id;
            results[*result_count].word_sig = bridge->word_signatures[i];
            results[*result_count].resonance_score = 1.0f;
            results[*result_count].jaccard_similarity = 1.0f;
            results[*result_count].quaternion_similarity = 1.0f;
            (*result_count)++;
        }
    }

    return PR_SPEECH_SUCCESS;
}

pr_speech_error_t pr_speech_bridge_find_rhymes(
    pr_speech_bridge_t* bridge,
    const phoneme_t* phonemes,
    size_t count,
    pr_speech_retrieval_result_t* results,
    size_t max_results,
    size_t* result_count
) {
    if (!bridge || !phonemes || !results || !result_count) {
        return PR_SPEECH_ERROR_NULL_POINTER;
    }

    *result_count = 0;

    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_find_rhymes", 0.0f);


    if (count == 0 || max_results == 0) {
        return PR_SPEECH_SUCCESS;
    }

    // Rhyme = matching ending phonemes
    for (size_t i = 0; i < bridge->word_count && *result_count < max_results; i++) {
        pr_word_signature_t* ws = &bridge->word_signatures[i];

        // Need at least 'count' phonemes to match
        if (ws->phoneme_count < count) {
            continue;
        }

        // Compare ending phonemes
        bool match = true;
        for (size_t j = 0; j < count; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && count > 256) {
                pr_speech_bridge_heartbeat("pr_speech_br_loop",
                                 (float)(j + 1) / (float)count);
            }

            size_t ws_idx = ws->phoneme_count - count + j;
            if (ws->phonemes[ws_idx] != phonemes[j]) {
                match = false;
                break;
            }
        }

        if (match) {
            results[*result_count].memory_id = ws->word_id;
            results[*result_count].word_sig = *ws;
            results[*result_count].resonance_score = (float)count / ws->phoneme_count;
            results[*result_count].jaccard_similarity = results[*result_count].resonance_score;
            results[*result_count].quaternion_similarity = 1.0f;
            (*result_count)++;
        }
    }

    return PR_SPEECH_SUCCESS;
}

//=============================================================================
// FEP Integration Functions
//=============================================================================

pr_speech_error_t pr_speech_bridge_update_from_fep(
    pr_speech_bridge_t* bridge,
    float prediction_error
) {
    if (!bridge) {
        return PR_SPEECH_ERROR_NULL_POINTER;
    }

    // Update PE state
    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_update_from_fep", 0.0f);


    bridge->state.current_pe = prediction_error;

    // Update running average (exponential moving average)
    const float alpha = 0.1f;
    bridge->state.avg_pe = bridge->state.avg_pe * (1.0f - alpha) + prediction_error * alpha;

    // Check for word boundary
    if (pr_speech_bridge_detect_word_boundary(bridge)) {
        bridge->stats.word_boundaries_detected++;

        // Finalize current word if one is in progress
        if (bridge->state.word_in_progress && bridge->state.current_word_length > 0) {
            pr_speech_finalize_word(bridge);
        }
    }

    return PR_SPEECH_SUCCESS;
}

bool pr_speech_bridge_detect_word_boundary(pr_speech_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_speech_bridge_detect_word_boundary: bridge is NULL");
        return false;
    }

    if (!bridge->config.enable_fep_updates) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_speech_bridge_detect_word_boundary: bridge->config is NULL");
        return false;
    }

    // Word boundary detected when PE spikes above threshold
    // relative to the running average
    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_detect_word_boundary", 0.0f);


    float threshold = bridge->config.word_boundary_pe_threshold;
    float relative_pe = bridge->state.current_pe - bridge->state.avg_pe;

    return (relative_pe > threshold * bridge->state.avg_pe);
}

//=============================================================================
// Theta-Gamma Integration Functions
//=============================================================================

pr_speech_error_t pr_speech_bridge_update_theta_gamma(
    pr_speech_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        return PR_SPEECH_ERROR_NULL_POINTER;
    }

    if (!bridge->config.enable_theta_gamma) {
        return PR_SPEECH_SUCCESS;
    }

    // Convert ms to seconds
    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_update_theta_gamma", 0.0f);


    float dt_s = dt_ms / 1000.0f;

    // Update theta phase
    float theta_delta = 2.0f * (float)M_PI * bridge->config.theta_frequency_hz * dt_s;
    bridge->state.theta_phase += theta_delta;
    bridge->state.theta_phase = pr_speech_normalize_phase(bridge->state.theta_phase);

    // Update gamma phase (faster oscillation)
    float gamma_delta = 2.0f * (float)M_PI * bridge->config.gamma_frequency_hz * dt_s;
    bridge->state.gamma_phase += gamma_delta;
    bridge->state.gamma_phase = pr_speech_normalize_phase(bridge->state.gamma_phase);

    // Track theta cycles
    if (bridge->state.theta_phase < theta_delta) {
        // Phase wrapped around - completed a cycle
        bridge->stats.theta_cycles++;
    }

    // Track gamma bursts (rough approximation - gamma amplitude peak)
    if (bridge->state.gamma_phase < gamma_delta) {
        bridge->stats.gamma_bursts++;
    }

    return PR_SPEECH_SUCCESS;
}

bool pr_speech_bridge_can_encode(const pr_speech_bridge_t* bridge) {
    if (!bridge || !bridge->config.enable_theta_gamma) {
        return true;  // No theta-gamma = always allowed
    }

    // Encoding allowed at theta trough (0° to 90°)
    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_can_encode", 0.0f);


    float phase_deg = bridge->state.theta_phase * 180.0f / (float)M_PI;
    return (phase_deg >= 0.0f && phase_deg < 90.0f) ||
           (phase_deg >= 315.0f && phase_deg <= 360.0f);
}

bool pr_speech_bridge_can_retrieve(const pr_speech_bridge_t* bridge) {
    if (!bridge || !bridge->config.enable_theta_gamma) {
        return true;  // No theta-gamma = always allowed
    }

    // Retrieval allowed at theta peak (180° to 270°)
    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_can_retrieve", 0.0f);


    float phase_deg = bridge->state.theta_phase * 180.0f / (float)M_PI;
    return (phase_deg >= 135.0f && phase_deg < 270.0f);
}

float pr_speech_bridge_get_encode_strength(const pr_speech_bridge_t* bridge) {
    if (!bridge || !bridge->config.enable_theta_gamma) {
        return 1.0f;
    }

    // Encoding strength follows cosine profile with peak at 45°
    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_get_encode_strength", 0.0f);


    float optimal_phase = (float)M_PI / 4.0f;  // 45°
    float phase_diff = fabsf(bridge->state.theta_phase - optimal_phase);
    if (phase_diff > (float)M_PI) {
        phase_diff = 2.0f * (float)M_PI - phase_diff;
    }

    // Cosine falloff
    return cosf(phase_diff) * 0.5f + 0.5f;
}

//=============================================================================
// State and Statistics Functions
//=============================================================================

pr_speech_error_t pr_speech_bridge_get_state(
    const pr_speech_bridge_t* bridge,
    pr_speech_bridge_state_t* state
) {
    if (!bridge || !state) {
        return PR_SPEECH_ERROR_NULL_POINTER;
    }

    *state = bridge->state;
    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_get_state", 0.0f);


    return PR_SPEECH_SUCCESS;
}

pr_speech_error_t pr_speech_bridge_get_stats(
    const pr_speech_bridge_t* bridge,
    pr_speech_bridge_stats_t* stats
) {
    if (!bridge || !stats) {
        return PR_SPEECH_ERROR_NULL_POINTER;
    }

    *stats = bridge->stats;
    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_get_stats", 0.0f);


    return PR_SPEECH_SUCCESS;
}

pr_speech_error_t pr_speech_bridge_reset_stats(pr_speech_bridge_t* bridge) {
    if (!bridge) {
        return PR_SPEECH_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_reset_stats", 0.0f);


    memset(&bridge->stats, 0, sizeof(pr_speech_bridge_stats_t));
    return PR_SPEECH_SUCCESS;
}

nimcp_quaternion_t pr_speech_bridge_get_current_quaternion(
    const pr_speech_bridge_t* bridge
) {
    if (!bridge) {
        return quat_identity();
    }
    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_get_current_quaterni", 0.0f);


    return bridge->state.current_speech_quat;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* pr_speech_error_string(pr_speech_error_t error) {
    switch (error) {
        case PR_SPEECH_SUCCESS:             return "Success";
        case PR_SPEECH_ERROR_NULL_POINTER:  return "Null pointer";
        case PR_SPEECH_ERROR_INVALID_CONFIG: return "Invalid configuration";
        case PR_SPEECH_ERROR_NO_MEMORY:     return "Memory allocation failed";
        case PR_SPEECH_ERROR_NOT_CONNECTED: return "Required system not connected";
        case PR_SPEECH_ERROR_BUFFER_FULL:   return "Phoneme buffer full";
        case PR_SPEECH_ERROR_INVALID_PHONEME: return "Invalid phoneme";
        case PR_SPEECH_ERROR_ENCODING_FAILED: return "Memory encoding failed";
        case PR_SPEECH_ERROR_RETRIEVAL_FAILED: return "Memory retrieval failed";
        case PR_SPEECH_ERROR_FEP_FAILED:    return "FEP update failed";
        case PR_SPEECH_ERROR_THETA_GAMMA:   return "Theta-gamma error";
        default:                            return "Unknown error";
    }
}

const char* pr_speech_word_type_name(pr_word_type_t word_type) {
    switch (word_type) {
        case PR_WORD_TYPE_UNKNOWN:    return "Unknown";
        case PR_WORD_TYPE_CONTENT:    return "Content";
        case PR_WORD_TYPE_FUNCTION:   return "Function";
        case PR_WORD_TYPE_PROPER:     return "Proper";
        case PR_WORD_TYPE_TECHNICAL:  return "Technical";
        default:                      return "Invalid";
    }
}

pr_speech_error_t pr_speech_bridge_init_phoneme_primes(pr_speech_bridge_t* bridge) {
    if (!bridge) {
        return PR_SPEECH_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_init_phoneme_primes", 0.0f);


    for (int i = 0; i < PR_SPEECH_NUM_PHONEMES && i < 64; i++) {
        bridge->phoneme_to_prime[i].phoneme = (phoneme_t)i;
        bridge->phoneme_to_prime[i].prime = PHONEME_PRIMES[i];
        bridge->phoneme_to_prime[i].frequency = (i < 44) ? PHONEME_FREQUENCIES[i] : 0.01f;
        bridge->phoneme_to_prime[i].is_vowel = (i < 44) ? PHONEME_IS_VOWEL[i] : false;
    }

    return PR_SPEECH_SUCCESS;
}

float pr_speech_bridge_position_weight(
    uint32_t position,
    uint32_t word_length,
    float decay_factor
) {
    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_position_weight", 0.0f);


    if (word_length == 0) {
        return 1.0f;
    }

    // Position weight decays from start of word
    // First phoneme has weight 1.0, subsequent phonemes decay
    return powf(decay_factor, (float)position);
}

void pr_speech_bridge_print_state(const pr_speech_bridge_t* bridge) {
    if (!bridge) {
        printf("PR Speech Bridge: NULL\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_print_state", 0.0f);


    printf("=== PR Speech Bridge State ===\n");
    printf("Initialized: %s\n", bridge->initialized ? "yes" : "no");
    printf("Speech Cortex: %s\n", bridge->speech_cortex ? "connected" : "disconnected");
    printf("FEP Bridge: %s\n", bridge->fep_bridge ? "connected" : "disconnected");
    printf("Node Manager: %s\n", bridge->node_manager ? "connected" : "disconnected");
    printf("\nState:\n");
    printf("  Buffer position: %zu\n", bridge->state.buffer_pos);
    printf("  Current word length: %u\n", bridge->state.current_word_length);
    printf("  Word in progress: %s\n", bridge->state.word_in_progress ? "yes" : "no");
    printf("  Theta phase: %.2f rad (%.1f deg)\n",
           bridge->state.theta_phase,
           bridge->state.theta_phase * 180.0f / (float)M_PI);
    printf("  Current PE: %.4f (avg: %.4f)\n",
           bridge->state.current_pe, bridge->state.avg_pe);
    printf("\nStatistics:\n");
    printf("  Phonemes processed: %lu\n", (unsigned long)bridge->stats.phonemes_processed);
    printf("  Words encoded: %lu\n", (unsigned long)bridge->stats.words_encoded);
    printf("  Words retrieved: %lu\n", (unsigned long)bridge->stats.words_retrieved);
    printf("  Signatures computed: %lu\n", (unsigned long)bridge->stats.signatures_computed);
    printf("  Word boundaries: %lu\n", (unsigned long)bridge->stats.word_boundaries_detected);
    printf("  Avg word length: %.2f\n", bridge->stats.avg_word_length);
    printf("  Theta cycles: %lu\n", (unsigned long)bridge->stats.theta_cycles);
    printf("=============================\n");
}

void pr_speech_word_signature_print(const pr_word_signature_t* word_sig) {
    if (!word_sig) {
        printf("Word Signature: NULL\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_speech_bridge_heartbeat("pr_speech_br_pr_speech_word_signa", 0.0f);


    printf("=== Word Signature ===\n");
    printf("ID: 0x%016lx\n", (unsigned long)word_sig->word_id);
    printf("Text: \"%s\"\n", word_sig->word_text);
    printf("Phoneme count: %u\n", word_sig->phoneme_count);
    printf("Phonemes: [");
    for (uint32_t i = 0; i < word_sig->phoneme_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && word_sig->phoneme_count > 256) {
            pr_speech_bridge_heartbeat("pr_speech_br_loop",
                             (float)(i + 1) / (float)word_sig->phoneme_count);
        }

        printf("%d%s", word_sig->phonemes[i], (i < word_sig->phoneme_count - 1) ? ", " : "");
    }
    printf("]\n");
    printf("Word type: %s\n", pr_speech_word_type_name(word_sig->word_type));
    printf("Frequency: %.4f\n", word_sig->frequency);
    printf("Neighborhood density: %.4f\n", word_sig->neighborhood_density);
    printf("Syllable count: %u\n", word_sig->syllable_count);
    printf("Signature hash: 0x%016lx\n", (unsigned long)word_sig->signature.hash);
    printf("Signature factors: %u\n", word_sig->signature.num_factors);
    printf("======================\n");
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

static uint64_t pr_speech_get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static float pr_speech_clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static float pr_speech_normalize_phase(float phase) {
    while (phase >= 2.0f * (float)M_PI) {
        phase -= 2.0f * (float)M_PI;
    }
    while (phase < 0.0f) {
        phase += 2.0f * (float)M_PI;
    }
    return phase;
}

static void pr_speech_reset_prosody_accumulator(pr_speech_bridge_t* bridge) {
    if (!bridge) return;

    memset(&bridge->state.current_prosody, 0, sizeof(pr_prosody_features_t));
    bridge->state.prosody_sample_count = 0;
}

static pr_speech_error_t pr_speech_finalize_word(pr_speech_bridge_t* bridge) {
    if (!bridge) {
        return PR_SPEECH_ERROR_NULL_POINTER;
    }

    if (bridge->state.current_word_length == 0) {
        bridge->state.word_in_progress = false;
        return PR_SPEECH_SUCCESS;
    }

    // Process the word
    pr_memory_node_t* node = pr_speech_bridge_process_word(
        bridge,
        NULL,  // No text available
        bridge->state.current_word_phonemes,
        bridge->state.current_word_length,
        PR_WORD_TYPE_UNKNOWN  // Would need lexicon lookup for type
    );

    // Reset word state
    bridge->state.word_in_progress = false;
    bridge->state.current_word_length = 0;
    pr_speech_reset_prosody_accumulator(bridge);

    return (node != NULL) ? PR_SPEECH_SUCCESS : PR_SPEECH_ERROR_ENCODING_FAILED;
}

//=============================================================================
// Instance Health Agent Setter (B25 Upgrade)
//=============================================================================

void pr_speech_bridge_set_instance_health_agent(
    pr_speech_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B25 Upgrade)
//=============================================================================

int pr_speech_bridge_training_begin(pr_speech_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_speech_bridge_training_begin: NULL argument");
        return -1;
    }
    pr_speech_bridge_heartbeat_instance(bridge->health_agent, "pr_speech_bridge_training_begin", 0.0f);
    return 0;
}

int pr_speech_bridge_training_end(pr_speech_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_speech_bridge_training_end: NULL argument");
        return -1;
    }
    pr_speech_bridge_heartbeat_instance(bridge->health_agent, "pr_speech_bridge_training_end", 1.0f);
    return 0;
}

int pr_speech_bridge_training_step(pr_speech_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_speech_bridge_training_step: NULL argument");
        return -1;
    }
    pr_speech_bridge_heartbeat_instance(bridge->health_agent, "pr_speech_bridge_training_step", progress);
    return 0;
}
