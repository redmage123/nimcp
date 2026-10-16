/**
 * @file nimcp_language_production_bridge.c
 * @brief Implementation of Language Production Bridge
 *
 * WHAT: Connects Broca's region with Speech Cortex and NLP systems
 * WHY:  Enable end-to-end language production pipeline
 * HOW:  Orchestrate data flow and transformations between systems
 *
 * @version Phase B3: Language Production Integration
 * @date 2025-11-23
 */

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

// Logging integration
#include "utils/logging/nimcp_logging.h"

// Unified memory integration
#include "utils/memory/nimcp_unified_memory.h"

// Second messenger integration
#include "plasticity/nimcp_second_messengers.h"

// Positional encoding integration
#include "utils/encoding/nimcp_positional_encoding.h"

#include "core/brain/regions/broca/nimcp_language_production_bridge.h"
#include "core/brain/regions/broca/nimcp_broca_adapter.h"
#include "core/brain/regions/wernicke/nimcp_lexical_access.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/memory/nimcp_memory_guards.h"  // For nimcp_calloc/nimcp_free
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "BROCA_LANG_PROD"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_dimension_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(language_production_bridge, MESH_ADAPTER_CATEGORY_SYSTEM)


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Internal bridge state
 */
struct language_production_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    lpb_config_t config;

    /* Core component */
    broca_adapter_t* broca;

    /* Connected systems (opaque references) */
    speech_cortex_t* speech_cortex;
    nlp_network_t* nlp;
    working_memory_t* wm;
    lexical_access_t* lexical;          /**< Wernicke's lexicon for word lookup */

    /* Neuromodulation */
    second_messenger_system_t* second_messengers;

    /* Positional encoding */
    nimcp_pos_encoder_t* motor_seq_encoder;   /**< Sinusoidal PE for motor sequences */
    nimcp_pos_encoder_t* gesture_encoder;     /**< RoPE for articulatory gestures */

    /* State */
    lpb_status_t status;
    lpb_error_t last_error;

    /* Current production context */
    lpb_semantic_intent_t current_intent;
    float* priming_vector;
    uint32_t priming_dim;
    float priming_strength;

    /* Output buffers */
    lpb_token_t* token_buffer;
    uint32_t token_count;
    uint8_t* phoneme_buffer;
    uint32_t phoneme_count;

    /* Callbacks */
    lpb_motor_output_callback_t motor_callback;
    void* motor_callback_data;
    lpb_event_callback_t event_callback;
    void* event_callback_data;

    /* Statistics */
    lpb_stats_t stats;
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Set error and return false
 */
static bool lpb_set_error(language_production_bridge_t* bridge, lpb_error_t error) {
    if (bridge) {
        bridge->last_error = error;
        bridge->status = LPB_STATUS_ERROR;
    }
    return false;
}

/**
 * @brief Emit event to callback
 */
static void lpb_emit_event(language_production_bridge_t* bridge,
                           uint32_t event_type,
                           const void* event_data) {
    if (bridge && bridge->event_callback) {
        bridge->event_callback(event_type, event_data, bridge->event_callback_data);
    }
}

/**
 * @brief Clear current production context
 */
static void lpb_clear_context(language_production_bridge_t* bridge) {
    if (!bridge) return;

    /* Clear intent */
    if (bridge->current_intent.semantic_vector) {
        nimcp_free(bridge->current_intent.semantic_vector);
        bridge->current_intent.semantic_vector = NULL;
    }
    memset(&bridge->current_intent, 0, sizeof(lpb_semantic_intent_t));

    /* Clear tokens */
    bridge->token_count = 0;

    /* Clear phonemes */
    bridge->phoneme_count = 0;

    bridge->status = LPB_STATUS_IDLE;
    bridge->last_error = LPB_ERROR_NONE;
}

/**
 * @brief Simple hash for lexicon lookup
 */
static uint32_t lpb_hash_string(const char* str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + (uint32_t)c;
    }
    return hash;
}

/**
 * @brief Map semantic vector to tokens using Wernicke's lexicon
 *
 * BIOLOGICAL BASIS: Semantic vectors activate lemmas in mental lexicon
 * via spreading activation. When a lexicon is connected, we compute
 * cosine similarity between the semantic vector and each word's embedding
 * to find the best-matching words. Falls back to feature-based heuristic
 * when no lexicon is available.
 */
static uint32_t lpb_semantic_to_tokens(language_production_bridge_t* bridge,
                                       const float* semantic_vector,
                                       uint32_t dim,
                                       lpb_token_t* tokens,
                                       uint32_t max_tokens) {
    if (!bridge || !semantic_vector || !tokens || max_tokens == 0) {
        return 0;
    }

    /* Compute semantic magnitude for activation scaling */
    float activation_sum = 0.0F;
    for (uint32_t i = 0; i < dim && i < 256; i++) {
        activation_sum += semantic_vector[i] * semantic_vector[i];
    }
    float magnitude = sqrtf(activation_sum);
    float base_activation = magnitude > 0.0F ? magnitude / sqrtf((float)dim) : 0.5F;

    /* Apply priming boost if available */
    float prime_boost = 1.0F;
    if (bridge->priming_vector && bridge->priming_dim > 0) {
        float dot = 0.0F;
        uint32_t min_dim = (dim < bridge->priming_dim) ? dim : bridge->priming_dim;
        for (uint32_t i = 0; i < min_dim; i++) {
            dot += semantic_vector[i] * bridge->priming_vector[i];
        }
        prime_boost = 1.0F + (bridge->priming_strength * fmaxf(0.0F, dot));
    }

    /*
     * LEXICON-BASED SELECTION: When a real lexicon is connected, iterate
     * through entries and score each by cosine similarity with the semantic
     * vector. Select top-K words sorted by activation score.
     */
    if (bridge->lexical) {
        uint32_t lexicon_size = lexical_get_size(bridge->lexical);
        if (lexicon_size > 0) {
            /* Score all lexicon entries — collect candidates */
            enum { MAX_CANDIDATES = 128 };
            struct {
                uint32_t word_id;
                char word[32];
                uint8_t pos;
                float score;
                float freq;
            } candidates[MAX_CANDIDATES];
            uint32_t num_candidates = 0;

            for (uint32_t wid = 1; wid <= lexicon_size && num_candidates < MAX_CANDIDATES; wid++) {
                lexical_entry_t entry;
                if (!lexical_get_entry(bridge->lexical, wid, &entry)) {
                    continue;
                }

                /* Compute activation: frequency-weighted dot product with
                 * semantic vector features mapped to concept_id space.
                 * Words with embeddings use cosine similarity; without
                 * embeddings, use concept_id-based positional activation. */
                float word_activation = 0.0F;

                if (entry.embedding && dim > 0) {
                    /* Cosine similarity with word embedding */
                    float dot = 0.0F;
                    float emb_norm = 0.0F;
                    uint32_t match_dim = dim < 256 ? dim : 256;
                    for (uint32_t d = 0; d < match_dim; d++) {
                        dot += semantic_vector[d] * entry.embedding[d];
                        emb_norm += entry.embedding[d] * entry.embedding[d];
                    }
                    emb_norm = sqrtf(emb_norm);
                    if (emb_norm > 1e-7F && magnitude > 1e-7F) {
                        word_activation = dot / (emb_norm * magnitude);
                    }
                } else {
                    /* Concept-based positional activation:
                     * Map concept_id to a feature region and check activation */
                    uint32_t feature_idx = (entry.concept_id * 5) % dim;
                    uint32_t window = 8;
                    float region_sum = 0.0F;
                    for (uint32_t d = feature_idx; d < feature_idx + window && d < dim; d++) {
                        region_sum += fabsf(semantic_vector[d]);
                    }
                    word_activation = region_sum / (float)window;
                }

                /* Frequency weighting (Zipf scale: higher = more common) */
                float freq_weight = 0.5F + 0.5F * (entry.frequency / 7.0F);
                float final_score = word_activation * freq_weight * base_activation * prime_boost;

                if (final_score > 0.05F) {
                    candidates[num_candidates].word_id = wid;
                    strncpy(candidates[num_candidates].word, entry.orthography,
                            sizeof(candidates[num_candidates].word) - 1);
                    candidates[num_candidates].word[sizeof(candidates[num_candidates].word) - 1] = '\0';
                    candidates[num_candidates].pos = (uint8_t)entry.pos;
                    candidates[num_candidates].score = final_score;
                    candidates[num_candidates].freq = entry.frequency / 7.0F;
                    num_candidates++;
                }
            }

            /* Sort candidates by score (simple insertion sort, small N) */
            for (uint32_t i = 1; i < num_candidates; i++) {
                for (uint32_t j = i; j > 0 && candidates[j].score > candidates[j - 1].score; j--) {
                    /* swap */
                    __typeof__(candidates[0]) tmp = candidates[j];
                    candidates[j] = candidates[j - 1];
                    candidates[j - 1] = tmp;
                }
            }

            /* Take top-K */
            uint32_t count = 0;
            for (uint32_t i = 0; i < num_candidates && count < max_tokens; i++) {
                tokens[count].token_id = candidates[i].word_id;
                strncpy(tokens[count].token_str, candidates[i].word,
                        sizeof(tokens[count].token_str) - 1);
                tokens[count].token_str[sizeof(tokens[count].token_str) - 1] = '\0';
                tokens[count].pos = candidates[i].pos;
                tokens[count].activation = candidates[i].score;
                tokens[count].frequency = candidates[i].freq;
                count++;
            }
            if (count > 0) {
                return count;
            }
            /* Fall through to heuristic if lexicon yielded nothing */
        }
    }

    /*
     * FALLBACK: Feature-based heuristic when no lexicon is available
     * Feature indices (simplified model):
     *   0-31: entity features → "it"
     *   32-63: action features → "does"
     *   64-95: property features → "good"
     */
    uint32_t count = 0;

    float entity_activation = 0.0F;
    for (uint32_t i = 0; i < 32 && i < dim; i++) {
        entity_activation += fabsf(semantic_vector[i]);
    }
    entity_activation /= 32.0F;

    if (entity_activation > 0.1F && count < max_tokens) {
        tokens[count].token_id = lpb_hash_string("entity") % 10000;
        strncpy(tokens[count].token_str, "it", sizeof(tokens[count].token_str) - 1);
        tokens[count].token_str[sizeof(tokens[count].token_str) - 1] = '\0';
        tokens[count].pos = 0;
        tokens[count].activation = entity_activation * base_activation * prime_boost;
        tokens[count].frequency = 0.9F;
        count++;
    }

    float action_activation = 0.0F;
    for (uint32_t i = 32; i < 64 && i < dim; i++) {
        action_activation += fabsf(semantic_vector[i]);
    }
    action_activation /= 32.0F;

    if (action_activation > 0.1F && count < max_tokens) {
        tokens[count].token_id = lpb_hash_string("action") % 10000;
        strncpy(tokens[count].token_str, "does", sizeof(tokens[count].token_str) - 1);
        tokens[count].token_str[sizeof(tokens[count].token_str) - 1] = '\0';
        tokens[count].pos = 1;
        tokens[count].activation = action_activation * base_activation * prime_boost;
        tokens[count].frequency = 0.85F;
        count++;
    }

    float property_activation = 0.0F;
    for (uint32_t i = 64; i < 96 && i < dim; i++) {
        property_activation += fabsf(semantic_vector[i]);
    }
    property_activation /= 32.0F;

    if (property_activation > 0.15F && count < max_tokens) {
        tokens[count].token_id = lpb_hash_string("property") % 10000;
        strncpy(tokens[count].token_str, "good", sizeof(tokens[count].token_str) - 1);
        tokens[count].token_str[sizeof(tokens[count].token_str) - 1] = '\0';
        tokens[count].pos = 2;
        tokens[count].activation = property_activation * base_activation * prime_boost;
        tokens[count].frequency = 0.8F;
        count++;
    }

    return count;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

lpb_config_t lpb_default_config(void) {
    lpb_config_t config = {
        .max_tokens = LPB_DEFAULT_MAX_TOKENS,
        .semantic_dim = LPB_DEFAULT_SEMANTIC_DIM,
        .comprehension_threshold = LPB_DEFAULT_COMPREHENSION_THRESHOLD,
        .production_delay_ms = LPB_DEFAULT_PRODUCTION_DELAY_MS,
        .enable_wernicke_connection = true,
        .enable_nlp_connection = true,
        .enable_working_memory = true,
        .enable_semantic_priming = true,
        .enable_repetition = true,
        .enable_paraphrase = false,
        .enable_self_monitoring = true,
        .enable_error_correction = true,
        .enable_second_messengers = true,
        .enable_positional_encoding = true,
        .pe_config = {
            .motor_seq_pe_type = NIMCP_POS_SINUSOIDAL,
            .gesture_pe_type = NIMCP_POS_ROTARY,
            .motor_seq_max_length = 256,
            .motor_seq_embedding_dim = NIMCP_MEDIUM_EMBEDDING_DIM,
            .motor_seq_pe_base = 10000.0F,
            .gesture_max_length = 512,
            .gesture_embedding_dim = NIMCP_DEFAULT_EMBEDDING_DIM,
            .gesture_rope_base = 10000.0F,
            .enable_motor_pe_cache = true,
            .enable_gesture_pe_cache = true
        }
    };
    return config;
}

language_production_bridge_t* lpb_create(const lpb_config_t* config,
                                          broca_adapter_t* broca) {
    if (!broca) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca is NULL");

        return NULL;
    }

    language_production_bridge_t* bridge = nimcp_calloc(1, sizeof(language_production_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "language_production") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lpb_create: bridge_base_init failed");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = lpb_default_config();
    }

    /* Store Broca reference */
    bridge->broca = broca;

    /* Allocate buffers */
    bridge->token_buffer = nimcp_calloc(bridge->config.max_tokens, sizeof(lpb_token_t));
    if (!bridge->token_buffer) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lpb_create: bridge->token_buffer is NULL");
        return NULL;
    }

    /* Phoneme buffer: estimate 4 phonemes per token average */
    uint32_t phoneme_capacity = bridge->config.max_tokens * 4;
    bridge->phoneme_buffer = nimcp_calloc(phoneme_capacity, sizeof(uint8_t));
    if (!bridge->phoneme_buffer) {
        nimcp_free(bridge->token_buffer);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lpb_create: bridge->phoneme_buffer is NULL");
        return NULL;
    }

    /* Initialize second messenger system */
    if (bridge->config.enable_second_messengers) {
        second_messenger_config_t sm_config = second_messenger_default_config();
        sm_config.enable_bio_async = true;
        sm_config.enable_security = true;

        /* Estimate 100 neurons for Broca's region model */
        bridge->second_messengers = second_messenger_create(100, &sm_config);
        if (!bridge->second_messengers) {
            LOG_MODULE_WARN(LOG_MODULE, "%s", "Failed to create second messenger system, continuing without it");
            bridge->config.enable_second_messengers = false;
        } else {
            LOG_MODULE_INFO(LOG_MODULE, "%s", "Second messenger cascades enabled for language production");
        }
    }

    /* Initialize state */
    bridge->status = LPB_STATUS_IDLE;
    bridge->last_error = LPB_ERROR_NONE;

    return bridge;
}

void lpb_destroy(language_production_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "language_production");

    /* Clear context (frees intent vector) */
    lpb_clear_context(bridge);

    /* Free priming vector */
    if (bridge->priming_vector) {
        nimcp_free(bridge->priming_vector);
    }

    /* Destroy second messenger system */
    if (bridge->second_messengers) {
        second_messenger_destroy(bridge->second_messengers);
        bridge->second_messengers = NULL;
    }

    /* Destroy positional encoders */
    if (bridge->motor_seq_encoder) {
        nimcp_pos_encoder_destroy(bridge->motor_seq_encoder);
        bridge->motor_seq_encoder = NULL;
    }
    if (bridge->gesture_encoder) {
        nimcp_pos_encoder_destroy(bridge->gesture_encoder);
        bridge->gesture_encoder = NULL;
    }

    /* Free buffers */
    if (bridge->token_buffer) {
        nimcp_free(bridge->token_buffer);
    }
    if (bridge->phoneme_buffer) {
        nimcp_free(bridge->phoneme_buffer);
    }

    /* Note: We don't destroy broca, speech_cortex, nlp, or wm
     * as they are owned externally */

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

bool lpb_reset(language_production_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lpb_reset: bridge is NULL");
        return false;
    }

    lpb_clear_context(bridge);

    /* Clear priming */
    if (bridge->priming_vector) {
        nimcp_free(bridge->priming_vector);
        bridge->priming_vector = NULL;
    }
    bridge->priming_dim = 0;
    bridge->priming_strength = 0.0F;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(lpb_stats_t));

    /* Reset Broca adapter */
    if (bridge->broca) {
        broca_reset(bridge->broca);
    }

    return true;
}

/*=============================================================================
 * SYSTEM CONNECTIONS
 *===========================================================================*/

bool lpb_connect_speech_cortex(language_production_bridge_t* bridge,
                                speech_cortex_t* speech_cortex) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lpb_reset: bridge is NULL");
        return false;
    }

    if (!bridge->config.enable_wernicke_connection) {
        return lpb_set_error(bridge, LPB_ERROR_NO_SPEECH_CORTEX);
    }

    bridge->speech_cortex = speech_cortex;
    return true;
}

bool lpb_connect_nlp(language_production_bridge_t* bridge,
                     nlp_network_t* nlp) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lpb_reset: bridge is NULL");
        return false;
    }

    if (!bridge->config.enable_nlp_connection) {
        return lpb_set_error(bridge, LPB_ERROR_NO_NLP);
    }

    bridge->nlp = nlp;
    return true;
}

bool lpb_connect_working_memory(language_production_bridge_t* bridge,
                                 working_memory_t* wm) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lpb_reset: bridge is NULL");
        return false;
    }

    if (!bridge->config.enable_working_memory) {
        return lpb_set_error(bridge, LPB_ERROR_INVALID_INPUT);
    }

    bridge->wm = wm;
    return true;
}

bool lpb_connect_lexical_access(language_production_bridge_t* bridge,
                                 lexical_access_t* lexical) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lpb_connect_lexical_access: bridge is NULL");
        return false;
    }

    bridge->lexical = lexical;
    return true;
}

/*=============================================================================
 * PRODUCTION PIPELINE
 *===========================================================================*/

bool lpb_produce_from_intent(language_production_bridge_t* bridge,
                              const lpb_semantic_intent_t* intent,
                              lpb_production_result_t* result) {
    if (!bridge || !intent) {
        return lpb_set_error(bridge, LPB_ERROR_INVALID_INPUT);
    }

    if (!bridge->broca) {
        return lpb_set_error(bridge, LPB_ERROR_NO_BROCA);
    }

    bridge->stats.productions_attempted++;

    /* Stage 1: Receive intent */
    bridge->status = LPB_STATUS_RECEIVING_INTENT;
    lpb_emit_event(bridge, LPB_STATUS_RECEIVING_INTENT, intent);

    /* Copy intent */
    if (bridge->current_intent.semantic_vector) {
        nimcp_free(bridge->current_intent.semantic_vector);
    }
    bridge->current_intent = *intent;
    if (intent->semantic_vector && intent->semantic_dim > 0) {
        bridge->current_intent.semantic_vector = nimcp_malloc(intent->semantic_dim * sizeof(float));
        if (bridge->current_intent.semantic_vector) {
            memcpy(bridge->current_intent.semantic_vector,
                   intent->semantic_vector,
                   intent->semantic_dim * sizeof(float));
        }
    }

    /* Stage 2: Lexical selection */
    bridge->status = LPB_STATUS_LEXICAL_SELECTION;
    lpb_emit_event(bridge, LPB_STATUS_LEXICAL_SELECTION, NULL);

    bridge->token_count = lpb_semantic_to_tokens(
        bridge,
        intent->semantic_vector,
        intent->semantic_dim,
        bridge->token_buffer,
        bridge->config.max_tokens
    );

    if (bridge->token_count == 0) {
        return lpb_set_error(bridge, LPB_ERROR_LEXICAL_FAILURE);
    }

    bridge->stats.lexical_selections++;

    /* Stage 3: Syntactic encoding via Broca */
    bridge->status = LPB_STATUS_SYNTACTIC_ENCODING;
    lpb_emit_event(bridge, LPB_STATUS_SYNTACTIC_ENCODING, NULL);

    if (!broca_begin_utterance(bridge->broca)) {
        return lpb_set_error(bridge, LPB_ERROR_PRODUCTION_FAILURE);
    }

    /* Add tokens to Broca */
    for (uint32_t i = 0; i < bridge->token_count; i++) {
        broca_input_word_t word = {
            .word_id = bridge->token_buffer[i].token_id,
            .pos = bridge->token_buffer[i].pos,
            .number = 0, /* Singular */
            .person = 3, /* Third person */
            .tense = 0   /* Present */
        };
        strncpy(word.word, bridge->token_buffer[i].token_str, sizeof(word.word) - 1);
        word.word[sizeof(word.word) - 1] = '\0';

        if (!broca_add_word(bridge->broca, &word)) {
            /* Continue with partial utterance */
        }
    }

    /* Stage 4: Phonological encoding */
    bridge->status = LPB_STATUS_PHONOLOGICAL_ENCODING;
    lpb_emit_event(bridge, LPB_STATUS_PHONOLOGICAL_ENCODING, NULL);

    /* Stage 5: Articulation planning */
    bridge->status = LPB_STATUS_ARTICULATION;
    lpb_emit_event(bridge, LPB_STATUS_ARTICULATION, NULL);

    /* Query second messenger state for neuromodulation effects */
    float pka_activity = 0.0F;
    float production_delay_modulation = 1.0F;
    float fluency_modulation = 0.0F;

    if (bridge->second_messengers && bridge->config.enable_second_messengers) {
        second_messenger_state_t sm_state;
        /* Query neuron 0 as representative of Broca's region */
        if (second_messenger_get_state(bridge->second_messengers, 0, &sm_state) == NIMCP_SUCCESS) {
            pka_activity = sm_state.camp.pka_activity;

            /* PKA activity modulates production speed (dopamine effect) */
            /* Higher PKA (high dopamine) = faster production (reduced delay) */
            /* Lower PKA (low dopamine) = slower production (increased delay) */
            production_delay_modulation = 1.0F - (0.3F * pka_activity);

            /* PKA activity modulates fluency */
            /* Higher PKA = more fluent speech */
            fluency_modulation = 0.2F * pka_activity;

            LOG_MODULE_DEBUG(LOG_MODULE, "Second messenger modulation: PKA=%.3f, delay_mod=%.3f, fluency_mod=%.3f",
                           (double)pka_activity, (double)production_delay_modulation, (double)fluency_modulation);
        }
    }

    /* Process through Broca's pipeline */
    broca_utterance_result_t broca_result;
    if (!broca_process_utterance(bridge->broca, &broca_result)) {
        return lpb_set_error(bridge, LPB_ERROR_PRODUCTION_FAILURE);
    }

    /* Stage 6: Self-monitoring */
    if (bridge->config.enable_self_monitoring) {
        bridge->status = LPB_STATUS_SELF_MONITORING;
        lpb_emit_event(bridge, LPB_STATUS_SELF_MONITORING, NULL);

        float match_score;
        if (lpb_check_production(bridge, &match_score)) {
            if (result) {
                result->self_monitoring_passed = true;
                result->semantic_match = match_score;
            }
        } else if (bridge->config.enable_error_correction) {
            /* Attempt correction - simplified: just note the issue */
            bridge->stats.self_corrections++;
            if (result) {
                result->self_monitoring_passed = false;
            }
        }
    }

    /* Extract motor commands */
    if (bridge->motor_callback) {
        broca_output_command_t cmd;
        while (broca_get_next_command(bridge->broca, &cmd)) {
            bridge->motor_callback(&cmd, bridge->motor_callback_data);
        }
    }

    /* Fill result */
    if (result) {
        result->tokens = bridge->token_buffer;
        result->token_count = bridge->token_count;
        result->phonemes = bridge->phoneme_buffer;
        result->phoneme_count = broca_result.phoneme_count;
        result->motor_command_count = broca_result.command_count;

        /* Apply second messenger modulation to duration */
        float modulated_duration = broca_result.total_duration_ms * production_delay_modulation;
        result->estimated_duration_ms = modulated_duration;

        /* Compute fluency score from syntax/agreement validation + PKA modulation */
        float base_fluency = (broca_result.syntax_valid ? 0.5F : 0.0F) +
                             (broca_result.agreement_valid ? 0.5F : 0.0F);
        result->fluency_score = fminf(1.0F, base_fluency + fluency_modulation);

        if (!result->self_monitoring_passed) {
            result->semantic_match = 0.5F; /* Default if monitoring disabled */
        }
    }

    bridge->status = LPB_STATUS_READY;
    bridge->stats.productions_successful++;

    /* Track modulated production delay */
    float actual_delay = bridge->config.production_delay_ms * production_delay_modulation;
    float new_avg_latency =
        (bridge->stats.avg_production_latency_ms * (bridge->stats.productions_successful - 1) +
         actual_delay) / bridge->stats.productions_successful;
    if (isfinite(new_avg_latency)) bridge->stats.avg_production_latency_ms = new_avg_latency;

    if (result) {
        float new_avg_fluency =
            (bridge->stats.avg_fluency_score * (bridge->stats.productions_successful - 1) +
             result->fluency_score) / bridge->stats.productions_successful;
        if (isfinite(new_avg_fluency)) bridge->stats.avg_fluency_score = new_avg_fluency;
    }

    return true;
}

bool lpb_produce_from_tokens(language_production_bridge_t* bridge,
                              const lpb_token_t* tokens,
                              uint32_t num_tokens,
                              lpb_production_result_t* result) {
    if (!bridge || !tokens || num_tokens == 0) {
        return lpb_set_error(bridge, LPB_ERROR_INVALID_INPUT);
    }

    if (!bridge->broca) {
        return lpb_set_error(bridge, LPB_ERROR_NO_BROCA);
    }

    bridge->stats.productions_attempted++;

    /* Copy tokens to internal buffer */
    uint32_t copy_count = (num_tokens < bridge->config.max_tokens) ?
                          num_tokens : bridge->config.max_tokens;
    memcpy(bridge->token_buffer, tokens, copy_count * sizeof(lpb_token_t));
    bridge->token_count = copy_count;

    /* Skip lexical selection, go directly to syntax */
    bridge->status = LPB_STATUS_SYNTACTIC_ENCODING;

    if (!broca_begin_utterance(bridge->broca)) {
        return lpb_set_error(bridge, LPB_ERROR_PRODUCTION_FAILURE);
    }

    for (uint32_t i = 0; i < bridge->token_count; i++) {
        broca_input_word_t word = {
            .word_id = bridge->token_buffer[i].token_id,
            .pos = bridge->token_buffer[i].pos,
            .number = 0,
            .person = 3,
            .tense = 0
        };
        strncpy(word.word, bridge->token_buffer[i].token_str, sizeof(word.word) - 1);
        word.word[sizeof(word.word) - 1] = '\0';

        broca_add_word(bridge->broca, &word);
    }

    /* Process */
    bridge->status = LPB_STATUS_PHONOLOGICAL_ENCODING;
    bridge->status = LPB_STATUS_ARTICULATION;

    broca_utterance_result_t broca_result;
    if (!broca_process_utterance(bridge->broca, &broca_result)) {
        return lpb_set_error(bridge, LPB_ERROR_PRODUCTION_FAILURE);
    }

    /* Motor output */
    if (bridge->motor_callback) {
        broca_output_command_t cmd;
        while (broca_get_next_command(bridge->broca, &cmd)) {
            bridge->motor_callback(&cmd, bridge->motor_callback_data);
        }
    }

    if (result) {
        result->tokens = bridge->token_buffer;
        result->token_count = bridge->token_count;
        result->phonemes = bridge->phoneme_buffer;
        result->phoneme_count = broca_result.phoneme_count;
        result->motor_command_count = broca_result.command_count;
        result->estimated_duration_ms = broca_result.total_duration_ms;
        /* Compute fluency score from syntax/agreement validation */
        result->fluency_score = (broca_result.syntax_valid ? 0.5F : 0.0F) +
                                (broca_result.agreement_valid ? 0.5F : 0.0F);
        result->semantic_match = 1.0F; /* Direct tokens = exact match */
        result->self_monitoring_passed = true;
    }

    bridge->status = LPB_STATUS_READY;
    bridge->stats.productions_successful++;

    return true;
}

bool lpb_repeat_last_heard(language_production_bridge_t* bridge,
                            lpb_production_result_t* result) {
    if (!bridge) {
        return lpb_set_error(bridge, LPB_ERROR_INVALID_INPUT);
    }

    if (!bridge->config.enable_repetition) {
        return lpb_set_error(bridge, LPB_ERROR_INVALID_INPUT);
    }

    if (!bridge->speech_cortex) {
        return lpb_set_error(bridge, LPB_ERROR_NO_SPEECH_CORTEX);
    }

    bridge->stats.wernicke_inputs++;

    /*
     * In a full implementation, we would:
     * 1. Query speech_cortex for last comprehended utterance
     * 2. Get phoneme sequence directly (echoic pathway)
     * 3. Feed to Broca for motor planning only
     *
     * For now, return error as speech cortex interface is opaque
     */
    return lpb_set_error(bridge, LPB_ERROR_NO_SPEECH_CORTEX);
}

bool lpb_generate_response(language_production_bridge_t* bridge,
                            lpb_production_result_t* result) {
    if (!bridge) {
        return lpb_set_error(bridge, LPB_ERROR_INVALID_INPUT);
    }

    if (!bridge->nlp) {
        return lpb_set_error(bridge, LPB_ERROR_NO_NLP);
    }

    bridge->stats.nlp_inputs++;

    /*
     * In a full implementation, we would:
     * 1. Query NLP for response to last input
     * 2. Get semantic intent from NLP
     * 3. Call lpb_produce_from_intent
     *
     * For now, return error as NLP interface is opaque
     */
    return lpb_set_error(bridge, LPB_ERROR_NO_NLP);
}

/*=============================================================================
 * LEXICAL ACCESS
 *===========================================================================*/

bool lpb_select_lexical_items(language_production_bridge_t* bridge,
                               const float* semantic_vector,
                               uint32_t dim,
                               lpb_token_t* tokens,
                               uint32_t max_tokens,
                               uint32_t* num_selected) {
    if (!bridge || !semantic_vector || !tokens || !num_selected) {
        return lpb_set_error(bridge, LPB_ERROR_INVALID_INPUT);
    }

    *num_selected = lpb_semantic_to_tokens(bridge, semantic_vector, dim, tokens, max_tokens);

    if (*num_selected == 0) {
        return lpb_set_error(bridge, LPB_ERROR_LEXICAL_FAILURE);
    }

    bridge->stats.lexical_selections++;
    return true;
}

bool lpb_prime_lexical_access(language_production_bridge_t* bridge,
                               const float* context_vector,
                               uint32_t dim,
                               float prime_strength) {
    if (!bridge || !context_vector || dim == 0) {
        return lpb_set_error(bridge, LPB_ERROR_INVALID_INPUT);
    }

    if (!bridge->config.enable_semantic_priming) {
        return true; /* Silently succeed if priming disabled */
    }

    /* Allocate/reallocate priming vector */
    if (bridge->priming_vector == NULL || bridge->priming_dim != dim) {
        float* new_vec = nimcp_realloc(bridge->priming_vector, dim * sizeof(float));
        if (!new_vec) {
            return lpb_set_error(bridge, LPB_ERROR_INTERNAL);
        }
        bridge->priming_vector = new_vec;
        bridge->priming_dim = dim;
    }

    /* Copy and normalize */
    float norm = 0.0F;
    for (uint32_t i = 0; i < dim; i++) {
        norm += context_vector[i] * context_vector[i];
    }
    norm = sqrtf(norm);

    if (norm > 0.0F) {
        for (uint32_t i = 0; i < dim; i++) {
            bridge->priming_vector[i] = context_vector[i] / norm;
        }
    } else {
        memcpy(bridge->priming_vector, context_vector, dim * sizeof(float));
    }

    bridge->priming_strength = fmaxf(0.0F, fminf(1.0F, prime_strength));

    return true;
}

/*=============================================================================
 * SELF-MONITORING
 *===========================================================================*/

bool lpb_set_self_monitoring(language_production_bridge_t* bridge, bool enable) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lpb_set_self_monitoring: bridge is NULL");
        return false;
    }

    bridge->config.enable_self_monitoring = enable;
    return true;
}

bool lpb_check_production(language_production_bridge_t* bridge, float* match_score) {
    if (!bridge) {
        return false;
    }

    if (!bridge->config.enable_self_monitoring) {
        if (match_score) *match_score = 1.0F;
        return true;
    }

    /*
     * Self-monitoring: Compare planned output semantics to input intent
     *
     * BIOLOGICAL BASIS: Perceptual loop monitors inner speech before
     * articulation, comparing planned phonemes/words to intended meaning
     */

    if (!bridge->current_intent.semantic_vector || bridge->token_count == 0) {
        if (match_score) *match_score = 0.0F;
        return false;
    }

    /* Simplified check: compute expected semantic match from token activations */
    float total_activation = 0.0F;
    float max_activation = 0.0F;
    for (uint32_t i = 0; i < bridge->token_count; i++) {
        total_activation += bridge->token_buffer[i].activation;
        if (bridge->token_buffer[i].activation > max_activation) {
            max_activation = bridge->token_buffer[i].activation;
        }
    }

    /* Average activation as proxy for semantic match */
    float avg_activation = (bridge->token_count > 0) ?
                           total_activation / bridge->token_count : 0.0F;

    /* Scale to [0,1] range */
    float score = fminf(1.0F, avg_activation * 2.0F);

    if (match_score) {
        *match_score = score;
    }

    /* Pass if score exceeds comprehension threshold */
    return (score >= bridge->config.comprehension_threshold);
}

/*=============================================================================
 * CALLBACKS
 *===========================================================================*/

bool lpb_set_motor_callback(language_production_bridge_t* bridge,
                             lpb_motor_output_callback_t callback,
                             void* user_data) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lpb_check_production: bridge is NULL");
        return false;
    }

    bridge->motor_callback = callback;
    bridge->motor_callback_data = user_data;
    return true;
}

bool lpb_set_event_callback(language_production_bridge_t* bridge,
                             lpb_event_callback_t callback,
                             void* user_data) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lpb_check_production: bridge is NULL");
        return false;
    }

    bridge->event_callback = callback;
    bridge->event_callback_data = user_data;
    return true;
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

lpb_status_t lpb_get_status(const language_production_bridge_t* bridge) {
    if (!bridge) return LPB_STATUS_ERROR;
    return bridge->status;
}

lpb_error_t lpb_get_last_error(const language_production_bridge_t* bridge) {
    if (!bridge) return LPB_ERROR_INTERNAL;
    return bridge->last_error;
}

const char* lpb_error_string(lpb_error_t error) {
    switch (error) {
        case LPB_ERROR_NONE:              return "No error";
        case LPB_ERROR_INVALID_INPUT:     return "Invalid input";
        case LPB_ERROR_NO_BROCA:          return "Broca's region not connected";
        case LPB_ERROR_NO_SPEECH_CORTEX:  return "Speech cortex not connected";
        case LPB_ERROR_NO_NLP:            return "NLP network not connected";
        case LPB_ERROR_LEXICAL_FAILURE:   return "Lexical selection failed";
        case LPB_ERROR_PRODUCTION_FAILURE:return "Production pipeline failed";
        case LPB_ERROR_MONITORING_FAILURE:return "Self-monitoring failed";
        case LPB_ERROR_INTERNAL:          return "Internal error";
        default:                          return "Unknown error";
    }
}

const char* lpb_status_string(lpb_status_t status) {
    switch (status) {
        case LPB_STATUS_IDLE:                return "Idle";
        case LPB_STATUS_RECEIVING_INTENT:    return "Receiving intent";
        case LPB_STATUS_LEXICAL_SELECTION:   return "Lexical selection";
        case LPB_STATUS_SYNTACTIC_ENCODING:  return "Syntactic encoding";
        case LPB_STATUS_PHONOLOGICAL_ENCODING:return "Phonological encoding";
        case LPB_STATUS_ARTICULATION:        return "Articulation planning";
        case LPB_STATUS_SELF_MONITORING:     return "Self-monitoring";
        case LPB_STATUS_READY:               return "Ready";
        case LPB_STATUS_ERROR:               return "Error";
        default:                             return "Unknown";
    }
}

bool lpb_get_stats(const language_production_bridge_t* bridge, lpb_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lpb_get_stats: required parameter is NULL (bridge, stats)");
        return false;
    }

    *stats = bridge->stats;
    return true;
}

bool lpb_get_config(const language_production_bridge_t* bridge, lpb_config_t* config) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lpb_get_config: required parameter is NULL (bridge, config)");
        return false;
    }

    *config = bridge->config;
    return true;
}

/*=============================================================================
 * POSITIONAL ENCODING INTEGRATION
 *===========================================================================*/

/**
 * @brief Set positional encoding configuration
 *
 * WHAT: Initialize positional encoders for motor sequences and gestures
 * WHY:  Enable temporal ordering in speech production pipeline
 * HOW:  Create sinusoidal encoder for motor commands, RoPE for gestures
 *
 * BIOLOGICAL BASIS:
 * - Broca's area generates sequential motor commands with precise timing
 * - Motor cortex requires temporally ordered commands for coordinated speech
 * - Position encoding preserves sequential information in neural representations
 */
bool language_production_set_pe_config(
    language_production_bridge_t* bridge,
    const lpb_pe_config_t* pe_config) {
    /* Guard clause: validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lpb_get_config: bridge is NULL");
        return false;
    }

    if (!pe_config) {
        return lpb_set_error(bridge, LPB_ERROR_INVALID_INPUT);
    }

    if (!bridge->config.enable_positional_encoding) {
        LOG_MODULE_WARN(LOG_MODULE, "%s", "Positional encoding not enabled in config");
        return lpb_set_error(bridge, LPB_ERROR_INVALID_INPUT);
    }

    /* Update configuration */
    bridge->config.pe_config = *pe_config;

    /* Destroy existing encoders if present */
    if (bridge->motor_seq_encoder) {
        nimcp_pos_encoder_destroy(bridge->motor_seq_encoder);
        bridge->motor_seq_encoder = NULL;
    }
    if (bridge->gesture_encoder) {
        nimcp_pos_encoder_destroy(bridge->gesture_encoder);
        bridge->gesture_encoder = NULL;
    }

    /* Create motor sequence encoder (sinusoidal) */
    if (pe_config->motor_seq_pe_type == NIMCP_POS_SINUSOIDAL) {
        nimcp_pos_config_t motor_config = {
            .type = NIMCP_POS_SINUSOIDAL,
            .config.sinusoidal = {
                .base = {
                    .max_seq_length = pe_config->motor_seq_max_length,
                    .embedding_dim = pe_config->motor_seq_embedding_dim,
                    .cache_enabled = pe_config->enable_motor_pe_cache,
                    .thread_safe = false
                },
                .frequency_base = pe_config->motor_seq_pe_base,
                .frequency_scale = 1.0F
            }
        };

        bridge->motor_seq_encoder = nimcp_pos_encoder_create(&motor_config);
        if (!bridge->motor_seq_encoder) {
            LOG_MODULE_ERROR(LOG_MODULE, "%s", "Failed to create motor sequence PE encoder");
            return lpb_set_error(bridge, LPB_ERROR_INTERNAL);
        }

        /* Pre-compute cache if enabled */
        if (pe_config->enable_motor_pe_cache) {
            int result = nimcp_pos_cache_precompute(bridge->motor_seq_encoder,
                                                    pe_config->motor_seq_max_length);
            if (result != NIMCP_POS_SUCCESS) {
                LOG_MODULE_WARN(LOG_MODULE, "Failed to pre-compute motor PE cache: %d", result);
            }
        }

        LOG_MODULE_INFO(LOG_MODULE, "Motor sequence PE encoder created: type=sinusoidal, max_len=%u, dim=%u",
                       pe_config->motor_seq_max_length, pe_config->motor_seq_embedding_dim);
    }

    /* Create gesture encoder (RoPE) */
    if (pe_config->gesture_pe_type == NIMCP_POS_ROTARY) {
        nimcp_pos_config_t gesture_config = {
            .type = NIMCP_POS_ROTARY,
            .config.rope = {
                .base = {
                    .max_seq_length = pe_config->gesture_max_length,
                    .embedding_dim = pe_config->gesture_embedding_dim,
                    .cache_enabled = pe_config->enable_gesture_pe_cache,
                    .thread_safe = false
                },
                .rope_base = pe_config->gesture_rope_base,
                .rope_scaling = 1.0F,
                .rope_dim = 0,  /* Apply to all dimensions */
                .use_ntk_scaling = false,
                .ntk_factor = 1.0F
            }
        };

        bridge->gesture_encoder = nimcp_pos_encoder_create(&gesture_config);
        if (!bridge->gesture_encoder) {
            LOG_MODULE_ERROR(LOG_MODULE, "%s", "Failed to create gesture PE encoder");
            /* Clean up motor encoder if created */
            if (bridge->motor_seq_encoder) {
                nimcp_pos_encoder_destroy(bridge->motor_seq_encoder);
                bridge->motor_seq_encoder = NULL;
            }
            return lpb_set_error(bridge, LPB_ERROR_INTERNAL);
        }

        /* Pre-compute cache if enabled */
        if (pe_config->enable_gesture_pe_cache) {
            int result = nimcp_pos_cache_precompute(bridge->gesture_encoder,
                                                    pe_config->gesture_max_length);
            if (result != NIMCP_POS_SUCCESS) {
                LOG_MODULE_WARN(LOG_MODULE, "Failed to pre-compute gesture PE cache: %d", result);
            }
        }

        LOG_MODULE_INFO(LOG_MODULE, "Gesture PE encoder created: type=RoPE, max_len=%u, dim=%u",
                       pe_config->gesture_max_length, pe_config->gesture_embedding_dim);
    }

    return true;
}

/**
 * @brief Apply positional encoding to motor command sequence
 *
 * WHAT: Add sinusoidal position encodings to motor command embeddings
 * WHY:  Preserve temporal order of motor commands for fluent articulation
 * HOW:  Use nimcp_pos_encode_sequence to compute PE and add to embeddings
 *
 * BIOLOGICAL BASIS:
 * - Primary motor cortex (M1) receives sequential commands from Broca's area
 * - Tongue, lips, jaw movements must be precisely timed for speech
 * - Position encoding maintains temporal dependencies in command sequences
 */
bool language_production_encode_motor_sequence(
    language_production_bridge_t* bridge,
    const float* motor_embeddings,
    uint32_t seq_length,
    float* output) {
    /* Guard clause: validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lpb_get_config: bridge is NULL");
        return false;
    }

    if (!motor_embeddings || !output) {
        return lpb_set_error(bridge, LPB_ERROR_INVALID_INPUT);
    }

    if (seq_length == 0) {
        return lpb_set_error(bridge, LPB_ERROR_INVALID_INPUT);
    }

    if (!bridge->config.enable_positional_encoding) {
        /* If PE disabled, just copy input to output */
        if (output != motor_embeddings) {
            uint32_t embedding_dim = bridge->config.pe_config.motor_seq_embedding_dim;
            memcpy(output, motor_embeddings, seq_length * embedding_dim * sizeof(float));
        }
        return true;
    }

    if (!bridge->motor_seq_encoder) {
        LOG_MODULE_ERROR(LOG_MODULE, "%s", "Motor sequence encoder not initialized");
        return lpb_set_error(bridge, LPB_ERROR_INTERNAL);
    }

    /* Validate sequence length */
    if (seq_length > bridge->config.pe_config.motor_seq_max_length) {
        LOG_MODULE_ERROR(LOG_MODULE, "Sequence length %u exceeds max %u",
                        seq_length, bridge->config.pe_config.motor_seq_max_length);
        return lpb_set_error(bridge, LPB_ERROR_INVALID_INPUT);
    }

    /* Apply positional encoding (additive mode) */
    int result = nimcp_pos_apply_encoding(
        bridge->motor_seq_encoder,
        motor_embeddings,
        seq_length,
        output,
        true  /* additive: output = input + PE */
    );

    if (result != NIMCP_POS_SUCCESS) {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to apply motor sequence PE: error=%d", result);
        return lpb_set_error(bridge, LPB_ERROR_INTERNAL);
    }

    LOG_MODULE_DEBUG(LOG_MODULE, "Applied motor sequence PE: seq_len=%u", seq_length);
    return true;
}

/**
 * @brief Apply RoPE encoding to articulatory gesture sequence
 *
 * WHAT: Apply rotary position embedding to gesture query/key pairs
 * WHY:  Capture relative temporal relationships between gestures
 * HOW:  Use nimcp_pos_rope_apply for each position in sequence
 *
 * BIOLOGICAL BASIS:
 * - Articulatory gestures overlap during coarticulation
 * - Relative timing between gestures affects acoustic output
 * - RoPE naturally encodes relative position information
 */
bool language_production_encode_gesture(
    language_production_bridge_t* bridge,
    const float* gesture_query,
    const float* gesture_key,
    uint32_t seq_length,
    float* query_out,
    float* key_out) {
    /* Guard clause: validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge is NULL");
        return false;
    }

    if (!gesture_query || !gesture_key || !query_out || !key_out) {
        return lpb_set_error(bridge, LPB_ERROR_INVALID_INPUT);
    }

    if (seq_length == 0) {
        return lpb_set_error(bridge, LPB_ERROR_INVALID_INPUT);
    }

    if (!bridge->config.enable_positional_encoding) {
        /* If PE disabled, just copy inputs to outputs */
        uint32_t embedding_dim = bridge->config.pe_config.gesture_embedding_dim;
        if (query_out != gesture_query) {
            memcpy(query_out, gesture_query, seq_length * embedding_dim * sizeof(float));
        }
        if (key_out != gesture_key) {
            memcpy(key_out, gesture_key, seq_length * embedding_dim * sizeof(float));
        }
        return true;
    }

    if (!bridge->gesture_encoder) {
        LOG_MODULE_ERROR(LOG_MODULE, "%s", "Gesture encoder not initialized");
        return lpb_set_error(bridge, LPB_ERROR_INTERNAL);
    }

    /* Validate sequence length */
    if (seq_length > bridge->config.pe_config.gesture_max_length) {
        LOG_MODULE_ERROR(LOG_MODULE, "Sequence length %u exceeds max %u",
                        seq_length, bridge->config.pe_config.gesture_max_length);
        return lpb_set_error(bridge, LPB_ERROR_INVALID_INPUT);
    }

    uint32_t embedding_dim = bridge->config.pe_config.gesture_embedding_dim;

    /* Apply RoPE to each position in sequence */
    for (uint32_t pos = 0; pos < seq_length; pos++) {
        const float* q_in = gesture_query + (pos * embedding_dim);
        const float* k_in = gesture_key + (pos * embedding_dim);
        float* q_out = query_out + (pos * embedding_dim);
        float* k_out = key_out + (pos * embedding_dim);

        int result = nimcp_pos_rope_apply(
            bridge->gesture_encoder,
            q_in,
            k_in,
            pos,
            q_out,
            k_out
        );

        if (result != NIMCP_POS_SUCCESS) {
            LOG_MODULE_ERROR(LOG_MODULE, "Failed to apply RoPE at position %u: error=%d",
                           pos, result);
            return lpb_set_error(bridge, LPB_ERROR_INTERNAL);
        }
    }

    LOG_MODULE_DEBUG(LOG_MODULE, "Applied gesture RoPE: seq_len=%u", seq_length);
    return true;
}

/*=============================================================================
 * SECOND MESSENGER INTEGRATION
 *===========================================================================*/

/**
 * @brief Trigger receptor activation in second messenger system
 *
 * WHAT: Activate neuromodulator receptors in Broca's region
 * WHY:  Dopamine modulates speech production fluency via D1 -> cAMP -> PKA
 * HOW:  Forward receptor activation to second messenger cascade system
 *
 * BIOLOGICAL BASIS:
 * - D1 receptors (Gs-coupled) activate adenylyl cyclase -> cAMP -> PKA
 * - High PKA = faster lexical selection, increased speech rate, higher fluency
 * - Low PKA (Parkinson's) = hypophonic speech, reduced prosody, word-finding difficulty
 * - High PKA (mania) = rapid, pressured speech with reduced self-monitoring
 */
bool lpb_trigger_receptor(language_production_bridge_t* bridge,
                          uint32_t neuron_id,
                          uint8_t receptor_type,
                          float occupancy,
                          uint64_t timestamp_ms) {
    /* Guard clause: validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge is NULL");
        return false;
    }

    if (!bridge->second_messengers || !bridge->config.enable_second_messengers) {
        LOG_MODULE_WARN(LOG_MODULE, "%s", "Second messenger system not enabled");
        return false;
    }

    if (occupancy < 0.0F || occupancy > 1.0F) {
        LOG_MODULE_ERROR(LOG_MODULE, "Invalid receptor occupancy: %.3f (must be [0,1])", (double)occupancy);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return false;
    }

    /* Map receptor types to cascade activation
     * For simplicity, assume receptor_type maps to neuromodulator receptor enum
     * D1 (dopamine receptor 1) -> Gs-coupled -> activate cAMP cascade
     * D2 (dopamine receptor 2) -> Gi-coupled -> inhibit cAMP cascade
     */

    nimcp_result_t result;

    /* Simplified receptor mapping:
     * Assume receptor_type 0 = D1 (Gs), 1 = D2 (Gi), 2 = Gq-coupled
     * In production, use proper receptor_type_t enum from neuromodulators
     */
    if (receptor_type == 0) {
        /* D1 receptor: activate Gs pathway */
        result = second_messenger_activate_gs(bridge->second_messengers,
                                             neuron_id,
                                             occupancy,
                                             timestamp_ms);
        if (result == NIMCP_SUCCESS) {
            LOG_MODULE_DEBUG(LOG_MODULE, "D1 receptor activated: neuron=%u, occupancy=%.3f",
                           neuron_id, (double)occupancy);
        }
    } else if (receptor_type == 1) {
        /* D2 receptor: activate Gi pathway (inhibit cAMP) */
        result = second_messenger_activate_gi(bridge->second_messengers,
                                             neuron_id,
                                             occupancy,
                                             timestamp_ms);
        if (result == NIMCP_SUCCESS) {
            LOG_MODULE_DEBUG(LOG_MODULE, "D2 receptor activated: neuron=%u, occupancy=%.3f",
                           neuron_id, (double)occupancy);
        }
    } else if (receptor_type == 2) {
        /* Gq-coupled receptor: activate PLC pathway */
        result = second_messenger_activate_gq(bridge->second_messengers,
                                             neuron_id,
                                             occupancy,
                                             timestamp_ms);
        if (result == NIMCP_SUCCESS) {
            LOG_MODULE_DEBUG(LOG_MODULE, "Gq receptor activated: neuron=%u, occupancy=%.3f",
                           neuron_id, (double)occupancy);
        }
    } else {
        LOG_MODULE_ERROR(LOG_MODULE, "Unknown receptor type: %u", receptor_type);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return false;
    }

    if (result != NIMCP_SUCCESS) {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to activate receptor cascade: type=%u, neuron=%u",
                       receptor_type, neuron_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return false;
    }

    /* Update cascade dynamics */
    second_messenger_update(bridge->second_messengers, 1.0F, timestamp_ms);

    return true;
}

/**
 * @brief Get second messenger cascade state
 *
 * WHAT: Query kinase activity levels in Broca's region
 * WHY:  Monitor neuromodulator effects on speech production
 * HOW:  Query PKA, PKC, CaMKII from cascade state
 *
 * USAGE:
 * - PKA activity indicates dopamine (D1) effect strength
 * - PKC activity indicates serotonin (5-HT2A) or mGluR effects
 * - CaMKII activity indicates calcium signaling strength
 */
bool lpb_get_second_messenger_state(const language_production_bridge_t* bridge,
                                    uint32_t neuron_id,
                                    float* pka_activity,
                                    float* pkc_activity,
                                    float* camkii_activity) {
    /* Guard clause: validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge is NULL");
        return false;
    }

    if (!pka_activity || !pkc_activity || !camkii_activity) {
        LOG_MODULE_ERROR(LOG_MODULE, "%s", "NULL output pointers provided");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (pka_activity, pkc_activity, camkii_activity)");
        return false;
    }

    /* Initialize outputs to zero */
    *pka_activity = 0.0F;
    *pkc_activity = 0.0F;
    *camkii_activity = 0.0F;

    if (!bridge->second_messengers || !bridge->config.enable_second_messengers) {
        /* Return baseline values if second messengers disabled */
        return true;
    }

    /* Query cascade state */
    second_messenger_state_t state;
    nimcp_result_t result = second_messenger_get_state(bridge->second_messengers,
                                                      neuron_id,
                                                      &state);

    if (result != NIMCP_SUCCESS) {
        LOG_MODULE_WARN(LOG_MODULE, "Failed to query second messenger state for neuron %u", neuron_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return false;
    }

    /* Extract kinase activities */
    *pka_activity = state.camp.pka_activity;
    *pkc_activity = state.ip3_dag.pkc_activity;
    *camkii_activity = state.calcium.camkii_activity;

    LOG_MODULE_DEBUG(LOG_MODULE, "Second messenger state: neuron=%u, PKA=%.3f, PKC=%.3f, CaMKII=%.3f",
                   neuron_id, (double)*pka_activity, (double)*pkc_activity, (double)*camkii_activity);

    return true;
}

/*=============================================================================
 * DIRECT ACCESS
 *===========================================================================*/

broca_adapter_t* lpb_get_broca_adapter(language_production_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }
    return bridge->broca;
}
