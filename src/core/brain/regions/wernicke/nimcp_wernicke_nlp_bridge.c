/**
 * @file nimcp_wernicke_nlp_bridge.c
 * @brief Wernicke's Area - Comprehensive NLP Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-05
 *
 * WHAT: Unified integration bridge connecting Wernicke's area to all NLP modules
 * WHY:  Language comprehension requires coordination with speech perception,
 *       semantic memory, NLP processing, and multimodal integration
 * HOW:  Bridge pattern connecting Wernicke adapter to all NLP subsystems
 *
 * @version Phase W7: Wernicke NLP Integration
 * @author NIMCP Development Team
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_nlp_bridge.h"
#include "core/brain/nimcp_brain_kg.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

#define LOG_MODULE "WERNICKE_NLP"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(wernicke_nlp_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct wernicke_nlp_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    wernicke_nlp_config_t config;

    /* Connected modules */
    wernicke_adapter_t* wernicke;
    speech_cortex_t* speech_cortex;
    nlp_network_t nlp_network;           /* Already a pointer type */
    semantic_memory_system_t* semantic_memory;
    multimodal_nlp_bridge_t* multimodal;
    brain_kg_t* knowledge_graph;
    working_memory_t* working_memory;

    /* Connection status */
    bool speech_cortex_connected;
    bool nlp_network_connected;
    bool semantic_memory_connected;
    bool multimodal_connected;
    bool kg_connected;
    bool wm_connected;
    bool bio_async_connected;

    /* State */
    wernicke_nlp_state_t state;
    wernicke_nlp_mode_t mode;

    /* Processing buffers */
    wernicke_phoneme_input_t* phoneme_buffer;
    uint32_t phoneme_buffer_count;
    uint32_t phoneme_buffer_capacity;

    wernicke_concept_activation_t* concept_buffer;
    uint32_t concept_buffer_count;
    uint32_t concept_buffer_capacity;

    /* Statistics */
    wernicke_nlp_stats_t stats;

    /* Timing */
    uint64_t last_update_ms;
    uint64_t processing_start_ms;
};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Initialize processing buffers
 */
static bool init_buffers(wernicke_nlp_bridge_t* bridge) {
    bridge->phoneme_buffer_capacity = bridge->config.max_sequence_length;
    bridge->phoneme_buffer = nimcp_calloc(
        bridge->phoneme_buffer_capacity,
        sizeof(wernicke_phoneme_input_t)
    );
    if (!bridge->phoneme_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_buffers: bridge->phoneme_buffer is NULL");
        return false;
    }

    bridge->concept_buffer_capacity = WERNICKE_NLP_MAX_CONCEPTS;
    bridge->concept_buffer = nimcp_calloc(
        bridge->concept_buffer_capacity,
        sizeof(wernicke_concept_activation_t)
    );
    if (!bridge->concept_buffer) {
        nimcp_free(bridge->phoneme_buffer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_buffers: bridge->concept_buffer is NULL");
        return false;
    }

    return true;
}

/**
 * @brief Free processing buffers
 */
static void free_buffers(wernicke_nlp_bridge_t* bridge) {
    if (bridge->phoneme_buffer) {
        nimcp_free(bridge->phoneme_buffer);
        bridge->phoneme_buffer = NULL;
    }
    if (bridge->concept_buffer) {
        nimcp_free(bridge->concept_buffer);
        bridge->concept_buffer = NULL;
    }
}

/**
 * @brief Perform lexical access on phoneme sequence
 */
static int perform_lexical_access(
    wernicke_nlp_bridge_t* bridge,
    const wernicke_phoneme_input_t* phonemes,
    uint32_t count,
    uint32_t* word_ids,
    uint32_t* word_count,
    float* confidence)
{
    if (!bridge || !phonemes || !word_ids || !word_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perform_lexical_access: required parameter is NULL (bridge, phonemes, word_ids, word_count)");
        return -1;
    }

    uint64_t start = nimcp_time_get_ms();

    /* Simulate lexical access - would call wernicke_adapter functions */
    *word_count = 0;
    *confidence = 0.0f;

    /* Process phoneme boundaries to segment words */
    uint32_t word_start = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (phonemes[i].is_boundary || i == count - 1) {
            /* End of word - perform cohort recognition */
            if (i > word_start && *word_count < WERNICKE_NLP_MAX_TOKENS) {
                /* Simulate word ID lookup */
                word_ids[*word_count] = (uint32_t)(word_start * 1000 + i);
                (*word_count)++;
            }
            word_start = i + 1;
        }
    }

    /* Compute average confidence */
    float total_conf = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        total_conf += phonemes[i].confidence;
    }
    *confidence = count > 0 ? total_conf / count : 0.0f;

    /* Update statistics */
    uint64_t elapsed = nimcp_time_get_ms() - start;
    bridge->stats.words_recognized += *word_count;
    float new_lex_time = (bridge->stats.avg_lexical_time_ms * 0.9f) + (elapsed * 0.1f);
    if (isfinite(new_lex_time)) bridge->stats.avg_lexical_time_ms = new_lex_time;

    return 0;
}

/**
 * @brief Perform semantic integration on words
 */
static int perform_semantic_integration(
    wernicke_nlp_bridge_t* bridge,
    const uint32_t* word_ids,
    uint32_t word_count,
    wernicke_concept_activation_t* concepts,
    uint32_t* concept_count,
    float* coherence)
{
    if (!bridge || !word_ids || !concepts || !concept_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perform_semantic_integration: required parameter is NULL (bridge, word_ids, concepts, concept_count)");
        return -1;
    }

    uint64_t start = nimcp_time_get_ms();

    *concept_count = 0;
    *coherence = 0.0f;

    /* Activate concepts for each word */
    for (uint32_t i = 0; i < word_count && *concept_count < WERNICKE_NLP_MAX_CONCEPTS; i++) {
        /* Simulate concept activation */
        wernicke_concept_activation_t* c = &concepts[*concept_count];
        c->concept_id = word_ids[i];
        snprintf(c->concept_name, sizeof(c->concept_name), "concept_%u", word_ids[i]);
        c->activation = 0.8f + (float)(i % 3) * 0.1f;
        c->relevance = 1.0f - (float)i / (word_count + 1);
        c->source = i;
        c->is_target = (i == word_count - 1);
        (*concept_count)++;
    }

    /* Compute semantic coherence (simplified) */
    if (*concept_count > 1) {
        *coherence = 0.7f + 0.3f * (1.0f - 1.0f / *concept_count);
    } else if (*concept_count == 1) {
        *coherence = 0.9f;
    }

    /* Spread activation if semantic memory connected */
    if (bridge->semantic_memory_connected && bridge->semantic_memory) {
        /* Would call semantic_memory_spread_activation() */
        bridge->stats.semantic_queries++;
    }

    /* Update statistics */
    uint64_t elapsed = nimcp_time_get_ms() - start;
    bridge->stats.concepts_activated += *concept_count;
    float new_sem_time = (bridge->stats.avg_semantic_time_ms * 0.9f) + (elapsed * 0.1f);
    if (isfinite(new_sem_time)) bridge->stats.avg_semantic_time_ms = new_sem_time;
    float new_sem_coh = (bridge->stats.avg_semantic_coherence * 0.9f) + (*coherence * 0.1f);
    if (isfinite(new_sem_coh)) bridge->stats.avg_semantic_coherence = new_sem_coh;

    return 0;
}

/**
 * @brief Perform syntactic parsing
 */
static int perform_syntactic_parsing(
    wernicke_nlp_bridge_t* bridge,
    const uint32_t* word_ids,
    uint32_t word_count,
    uint32_t* constituents,
    uint32_t* constituent_count,
    float* wellformedness)
{
    if (!bridge || !word_ids || !constituents || !constituent_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perform_syntactic_parsing: required parameter is NULL (bridge, word_ids, constituents, constituent_count)");
        return -1;
    }

    *constituent_count = 0;
    *wellformedness = 0.0f;

    /* Simplified syntactic parsing - identify major constituents */
    if (word_count >= 1) {
        /* Subject NP */
        constituents[*constituent_count] = 1;  /* NP marker */
        (*constituent_count)++;
    }
    if (word_count >= 2) {
        /* Verb */
        constituents[*constituent_count] = 2;  /* V marker */
        (*constituent_count)++;
    }
    if (word_count >= 3) {
        /* Object NP */
        constituents[*constituent_count] = 3;  /* NP marker */
        (*constituent_count)++;
    }

    /* Compute wellformedness (simplified) */
    if (word_count >= 2) {
        *wellformedness = 0.8f + 0.2f * fminf(word_count / 5.0f, 1.0f);
    } else if (word_count == 1) {
        *wellformedness = 0.5f;  /* Single word - fragment */
    }

    bridge->stats.sentences_parsed++;
    float new_syn_score = (bridge->stats.avg_syntactic_score * 0.9f) + (*wellformedness * 0.1f);
    if (isfinite(new_syn_score)) bridge->stats.avg_syntactic_score = new_syn_score;

    return 0;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int wernicke_nlp_default_config(wernicke_nlp_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    memset(config, 0, sizeof(wernicke_nlp_config_t));

    /* Module enables */
    config->enable_speech_cortex = true;
    config->enable_nlp_network = true;
    config->enable_semantic_memory = true;
    config->enable_multimodal = true;
    config->enable_knowledge_graph = true;
    config->enable_working_memory = true;

    /* Processing parameters */
    config->default_mode = WERNICKE_NLP_MODE_COMPREHENSION;
    config->max_sequence_length = WERNICKE_NLP_MAX_PHONEMES;
    config->embedding_dim = WERNICKE_NLP_EMBEDDING_DIM;
    config->attention_dropout = 0.0f;

    /* Semantic parameters */
    config->spreading_activation_decay = 0.8f;
    config->max_spreading_depth = 3;
    config->concept_threshold = 0.1f;

    /* Cross-modal parameters */
    config->crossmodal_mode = WERNICKE_CROSSMODAL_AUDIO_ONLY;
    config->mcgurk_weight = 0.3f;

    /* Bio-async */
    config->enable_bio_async = true;
    config->inbox_capacity = 64;

    /* Logging */
    config->enable_logging = true;

    return 0;
}

wernicke_nlp_bridge_t* wernicke_nlp_bridge_create(
    wernicke_adapter_t* wernicke,
    const wernicke_nlp_config_t* config)
{
    /* Wernicke adapter is required */
    if (!wernicke) {
        LOG_ERROR(LOG_MODULE, "Cannot create NLP bridge: wernicke adapter is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke is NULL");

        return NULL;
    }

    wernicke_nlp_bridge_t* bridge = nimcp_calloc(1, sizeof(wernicke_nlp_bridge_t));
    if (!bridge) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate NLP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        wernicke_nlp_default_config(&bridge->config);
    }

    /* Store Wernicke adapter */
    bridge->wernicke = wernicke;

    /* Initialize buffers */
    if (!init_buffers(bridge)) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate buffers");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wernicke_nlp_bridge_create: init_buffers is NULL");
        return NULL;
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "wernicke_nlp") != 0) {
        free_buffers(bridge);
        nimcp_free(bridge);
        return NULL;
    }

    /* Initialize state */
    bridge->state = WERNICKE_NLP_STATE_IDLE;
    bridge->mode = bridge->config.default_mode;
    bridge->last_update_ms = nimcp_time_get_ms();

    LOG_INFO(LOG_MODULE, "Created Wernicke NLP bridge");
    return bridge;
}

void wernicke_nlp_bridge_destroy(wernicke_nlp_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "wernicke_nlp");

    bridge_base_cleanup(&bridge->base);
    free_buffers(bridge);
    nimcp_free(bridge);

    LOG_DEBUG(LOG_MODULE, "Destroyed Wernicke NLP bridge");
}

/* ============================================================================
 * Module Connection API Implementation
 * ============================================================================ */

int wernicke_nlp_connect_speech_cortex(
    wernicke_nlp_bridge_t* bridge,
    speech_cortex_t* speech_cortex)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_speech_cortex) return 0;

    bridge->speech_cortex = speech_cortex;
    bridge->speech_cortex_connected = (speech_cortex != NULL);

    if (bridge->speech_cortex_connected) {
        LOG_DEBUG(LOG_MODULE, "Connected to speech cortex");
    }

    return 0;
}

int wernicke_nlp_connect_nlp_network(
    wernicke_nlp_bridge_t* bridge,
    nlp_network_t nlp_network)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_nlp_network) return 0;

    bridge->nlp_network = nlp_network;
    bridge->nlp_network_connected = (nlp_network != NULL);

    if (bridge->nlp_network_connected) {
        LOG_DEBUG(LOG_MODULE, "Connected to NLP network");
    }

    return 0;
}

int wernicke_nlp_connect_semantic_memory(
    wernicke_nlp_bridge_t* bridge,
    semantic_memory_system_t* semantic_memory)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_semantic_memory) return 0;

    bridge->semantic_memory = semantic_memory;
    bridge->semantic_memory_connected = (semantic_memory != NULL);

    if (bridge->semantic_memory_connected) {
        LOG_DEBUG(LOG_MODULE, "Connected to semantic memory");
    }

    return 0;
}

int wernicke_nlp_connect_multimodal(
    wernicke_nlp_bridge_t* bridge,
    multimodal_nlp_bridge_t* multimodal)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_multimodal) return 0;

    bridge->multimodal = multimodal;
    bridge->multimodal_connected = (multimodal != NULL);

    if (bridge->multimodal_connected) {
        LOG_DEBUG(LOG_MODULE, "Connected to multimodal NLP bridge");
    }

    return 0;
}

int wernicke_nlp_connect_knowledge_graph(
    wernicke_nlp_bridge_t* bridge,
    brain_kg_t* kg)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_knowledge_graph) return 0;

    bridge->knowledge_graph = kg;
    bridge->kg_connected = (kg != NULL);

    if (bridge->kg_connected) {
        LOG_DEBUG(LOG_MODULE, "Connected to knowledge graph");
    }

    return 0;
}

int wernicke_nlp_connect_working_memory(
    wernicke_nlp_bridge_t* bridge,
    working_memory_t* wm)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_working_memory) return 0;

    bridge->working_memory = wm;
    bridge->wm_connected = (wm != NULL);

    if (bridge->wm_connected) {
        LOG_DEBUG(LOG_MODULE, "Connected to working memory");
    }

    return 0;
}

int wernicke_nlp_connect_bio_async(wernicke_nlp_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_bio_async) return 0;

    /* Would register with bio_router here */
    bridge->bio_async_connected = true;

    LOG_DEBUG(LOG_MODULE, "Connected to bio-async");
    return 0;
}

/* ============================================================================
 * Processing API Implementation
 * ============================================================================ */

int wernicke_nlp_process_phonemes(
    wernicke_nlp_bridge_t* bridge,
    const wernicke_phoneme_input_t* phonemes,
    uint32_t count,
    wernicke_comprehension_result_t* result)
{
    if (!bridge || !phonemes || !result || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_nlp_process_phonemes: required parameter is NULL (bridge, phonemes, result)");
        return -1;
    }

    uint64_t start = nimcp_time_get_ms();
    bridge->state = WERNICKE_NLP_STATE_RECEIVING;
    bridge->processing_start_ms = start;

    /* Update statistics */
    bridge->stats.phonemes_processed += count;
    if (bridge->speech_cortex_connected) {
        bridge->stats.speech_cortex_inputs++;
    }

    /* Allocate result buffers */
    result->word_ids = nimcp_calloc(WERNICKE_NLP_MAX_TOKENS, sizeof(uint32_t));
    result->concepts = nimcp_calloc(WERNICKE_NLP_MAX_CONCEPTS,
                                     sizeof(wernicke_concept_activation_t));
    result->constituent_ids = nimcp_calloc(32, sizeof(uint32_t));

    if (!result->word_ids || !result->concepts || !result->constituent_ids) {
        wernicke_nlp_free_result(result);
        bridge->state = WERNICKE_NLP_STATE_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wernicke_nlp_process_phonemes: required parameter is NULL (result->word_ids, result->concepts, result->constituent_ids)");
        return -1;
    }

    /* Phase 1: Lexical Access */
    bridge->state = WERNICKE_NLP_STATE_LEXICAL;
    if (perform_lexical_access(bridge, phonemes, count,
                                result->word_ids, &result->word_count,
                                &result->lexical_confidence) != 0) {
        bridge->state = WERNICKE_NLP_STATE_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wernicke_nlp_process_phonemes: operation failed");
        return -1;
    }

    /* Phase 2: Semantic Integration */
    bridge->state = WERNICKE_NLP_STATE_SEMANTIC;
    if (perform_semantic_integration(bridge,
                                      result->word_ids, result->word_count,
                                      result->concepts, &result->concept_count,
                                      &result->semantic_coherence) != 0) {
        bridge->state = WERNICKE_NLP_STATE_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wernicke_nlp_process_phonemes: operation failed");
        return -1;
    }

    /* Phase 3: Syntactic Parsing */
    bridge->state = WERNICKE_NLP_STATE_SYNTACTIC;
    if (perform_syntactic_parsing(bridge,
                                   result->word_ids, result->word_count,
                                   result->constituent_ids, &result->constituent_count,
                                   &result->syntactic_wellformedness) != 0) {
        bridge->state = WERNICKE_NLP_STATE_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wernicke_nlp_process_phonemes: operation failed");
        return -1;
    }

    /* Analyze prosody */
    result->is_question = false;
    result->is_command = false;
    result->emotional_valence = 0.0f;

    for (uint32_t i = 0; i < count; i++) {
        if (phonemes[i].pitch > 200.0f && i == count - 1) {
            result->is_question = true;
        }
        result->emotional_valence += phonemes[i].intensity * 0.01f;
    }
    result->emotional_valence = fminf(fmaxf(result->emotional_valence, -1.0f), 1.0f);

    /* Compute overall confidence */
    result->confidence = (result->lexical_confidence * 0.4f +
                          result->semantic_coherence * 0.4f +
                          result->syntactic_wellformedness * 0.2f);

    /* Compute processing time */
    result->processing_time_ms = (float)(nimcp_time_get_ms() - start);

    /* Update statistics */
    float new_total_time = (bridge->stats.avg_total_time_ms * 0.9f) + (result->processing_time_ms * 0.1f);
    if (isfinite(new_total_time)) bridge->stats.avg_total_time_ms = new_total_time;
    float new_lex_conf = (bridge->stats.avg_lexical_confidence * 0.9f) + (result->lexical_confidence * 0.1f);
    if (isfinite(new_lex_conf)) bridge->stats.avg_lexical_confidence = new_lex_conf;

    bridge->state = WERNICKE_NLP_STATE_COMPLETE;

    if (bridge->config.enable_logging) {
        LOG_DEBUG(LOG_MODULE, "Processed %u phonemes -> %u words, %u concepts (%.1fms)",
                  count, result->word_count, result->concept_count,
                  result->processing_time_ms);
    }

    return 0;
}

int wernicke_nlp_process_tokens(
    wernicke_nlp_bridge_t* bridge,
    const wernicke_token_output_t* tokens,
    uint32_t count,
    wernicke_comprehension_result_t* result)
{
    if (!bridge || !tokens || !result || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_nlp_process_tokens: required parameter is NULL (bridge, tokens, result)");
        return -1;
    }

    bridge->stats.nlp_network_calls++;

    /* Convert tokens to word IDs for semantic processing */
    result->word_ids = nimcp_calloc(count, sizeof(uint32_t));
    result->concepts = nimcp_calloc(WERNICKE_NLP_MAX_CONCEPTS,
                                     sizeof(wernicke_concept_activation_t));
    result->constituent_ids = nimcp_calloc(32, sizeof(uint32_t));

    if (!result->word_ids || !result->concepts || !result->constituent_ids) {
        wernicke_nlp_free_result(result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wernicke_nlp_process_tokens: required parameter is NULL (result->word_ids, result->concepts, result->constituent_ids)");
        return -1;
    }

    result->word_count = count;
    for (uint32_t i = 0; i < count; i++) {
        result->word_ids[i] = tokens[i].token_id;
    }

    /* Perform semantic integration */
    if (perform_semantic_integration(bridge,
                                      result->word_ids, result->word_count,
                                      result->concepts, &result->concept_count,
                                      &result->semantic_coherence) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wernicke_nlp_process_tokens: operation failed");
        return -1;
    }

    /* Perform syntactic parsing */
    if (perform_syntactic_parsing(bridge,
                                   result->word_ids, result->word_count,
                                   result->constituent_ids, &result->constituent_count,
                                   &result->syntactic_wellformedness) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wernicke_nlp_process_tokens: operation failed");
        return -1;
    }

    result->lexical_confidence = 1.0f;  /* Tokens already recognized */
    result->confidence = (result->semantic_coherence * 0.6f +
                          result->syntactic_wellformedness * 0.4f);

    return 0;
}

int wernicke_nlp_process_crossmodal(
    wernicke_nlp_bridge_t* bridge,
    const wernicke_crossmodal_input_t* input,
    wernicke_comprehension_result_t* result)
{
    if (!bridge || !input || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_nlp_process_crossmodal: required parameter is NULL (bridge, input, result)");
        return -1;
    }

    bridge->stats.multimodal_fusions++;

    /* Fuse modalities based on weights */
    float audio_contrib = input->audio_weight;
    float visual_contrib = input->visual_weight;

    /* McGurk effect: visual can override audio */
    if (input->mode == WERNICKE_CROSSMODAL_AUDIOVISUAL) {
        visual_contrib *= (1.0f + bridge->config.mcgurk_weight);
    }

    /* Normalize */
    float total = audio_contrib + visual_contrib;
    if (total > 0) {
        audio_contrib /= total;
        visual_contrib /= total;
    }

    /* Would process fused features here */
    /* For now, return placeholder result */
    memset(result, 0, sizeof(wernicke_comprehension_result_t));
    result->confidence = 0.5f * (audio_contrib + visual_contrib);

    if (bridge->config.enable_logging) {
        LOG_DEBUG(LOG_MODULE, "Cross-modal fusion: audio=%.2f, visual=%.2f",
                  audio_contrib, visual_contrib);
    }

    return 0;
}

int wernicke_nlp_activate_concept(
    wernicke_nlp_bridge_t* bridge,
    uint32_t concept_id,
    float activation)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Activate concept and spread */
    int activated = 1;  /* At least the target */

    if (bridge->semantic_memory_connected && bridge->semantic_memory) {
        /* Would call semantic_memory_spread_activation() */
        /* Simulate spreading to neighbors */
        activated += (int)(bridge->config.max_spreading_depth * 2);
        bridge->stats.semantic_queries++;
    }

    bridge->stats.concepts_activated += activated;

    if (bridge->config.enable_logging) {
        LOG_DEBUG(LOG_MODULE, "Activated concept %u (%.2f) -> %d total",
                  concept_id, activation, activated);
    }

    return activated;
}

int wernicke_nlp_query_semantic(
    wernicke_nlp_bridge_t* bridge,
    const char* query,
    wernicke_concept_activation_t* results,
    uint32_t max_results)
{
    if (!bridge || !query || !results || max_results == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_nlp_query_semantic: required parameter is NULL (bridge, query, results)");
        return -1;
    }

    bridge->stats.semantic_queries++;

    /* Would query semantic memory here */
    /* For now, return placeholder result */
    if (max_results >= 1) {
        results[0].concept_id = 1;
        snprintf(results[0].concept_name, sizeof(results[0].concept_name),
                 "query_result");
        results[0].activation = 0.9f;
        results[0].relevance = 0.8f;
        return 1;
    }

    return 0;
}

uint32_t wernicke_nlp_register_concept(
    wernicke_nlp_bridge_t* bridge,
    const char* concept_name,
    const char* properties)
{
    if (!bridge || !concept_name) return 0;

    if (!bridge->kg_connected || !bridge->knowledge_graph) {
        (void)properties;
        return 0;
    }

    /* Prefix concept names to avoid collisions with structural nodes
     * (see docs/claude/kg-node-naming-registry.md §2). */
    char node_name[BRAIN_KG_MAX_NAME_LEN];
    snprintf(node_name, sizeof(node_name), "cog_language_concept_%s", concept_name);

    /* Idempotent: skip if already registered */
    brain_kg_node_id_t existing = brain_kg_find_node(bridge->knowledge_graph, node_name);
    if (existing != BRAIN_KG_INVALID_NODE) {
        if (bridge->config.enable_logging) {
            LOG_DEBUG(LOG_MODULE, "Concept already registered: %s (id=%u)",
                      concept_name, existing);
        }
        (void)properties;
        return existing;
    }

    char description[BRAIN_KG_MAX_DESC_LEN];
    if (properties && properties[0]) {
        snprintf(description, sizeof(description),
                 "Wernicke-registered concept '%s' (%s)", concept_name, properties);
    } else {
        snprintf(description, sizeof(description),
                 "Wernicke-registered concept '%s'", concept_name);
    }

    brain_kg_node_id_t node_id = brain_kg_add_node(
        bridge->knowledge_graph,
        node_name,
        BRAIN_KG_NODE_COGNITIVE,
        description
    );

    if (node_id == BRAIN_KG_INVALID_NODE) {
        LOG_WARN(LOG_MODULE, "Failed to register concept node: %s", concept_name);
        (void)properties;
        return 0;
    }

    /* Link concept back to wernicke root (created by wernicke_kg_wiring).
     * If wernicke root doesn't exist yet, the edge is skipped silently. */
    brain_kg_node_id_t wernicke_root = brain_kg_find_node(bridge->knowledge_graph, "wernicke");
    if (wernicke_root != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(
            bridge->knowledge_graph,
            wernicke_root,
            node_id,
            BRAIN_KG_EDGE_PROVIDES_TO,
            "wernicke produces concept activation",
            0.7f
        );
    }

    bridge->stats.kg_registrations++;
    if (bridge->config.enable_logging) {
        LOG_DEBUG(LOG_MODULE, "Registered concept: %s (kg_id=%u)", concept_name, node_id);
    }

    (void)properties;
    return node_id;
}

/* ============================================================================
 * State and Statistics API Implementation
 * ============================================================================ */

int wernicke_nlp_bridge_update(
    wernicke_nlp_bridge_t* bridge,
    uint64_t current_time_ms)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->last_update_ms = current_time_ms;

    /* Reset state if idle too long */
    if (bridge->state == WERNICKE_NLP_STATE_COMPLETE ||
        bridge->state == WERNICKE_NLP_STATE_ERROR) {
        if (current_time_ms - bridge->processing_start_ms > 1000) {
            bridge->state = WERNICKE_NLP_STATE_IDLE;
        }
    }

    return 0;
}

wernicke_nlp_state_t wernicke_nlp_get_state(
    const wernicke_nlp_bridge_t* bridge)
{
    return bridge ? bridge->state : WERNICKE_NLP_STATE_IDLE;
}

int wernicke_nlp_get_stats(
    const wernicke_nlp_bridge_t* bridge,
    wernicke_nlp_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_nlp_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

void wernicke_nlp_reset_stats(wernicke_nlp_bridge_t* bridge) {
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(wernicke_nlp_stats_t));
}

void wernicke_nlp_free_result(wernicke_comprehension_result_t* result) {
    if (!result) return;

    if (result->word_ids) {
        nimcp_free(result->word_ids);
        result->word_ids = NULL;
    }
    if (result->concepts) {
        nimcp_free(result->concepts);
        result->concepts = NULL;
    }
    if (result->constituent_ids) {
        nimcp_free(result->constituent_ids);
        result->constituent_ids = NULL;
    }
}

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* wernicke_nlp_mode_to_string(wernicke_nlp_mode_t mode) {
    switch (mode) {
        case WERNICKE_NLP_MODE_COMPREHENSION: return "Comprehension";
        case WERNICKE_NLP_MODE_REPETITION:    return "Repetition";
        case WERNICKE_NLP_MODE_DICTATION:     return "Dictation";
        case WERNICKE_NLP_MODE_TRANSLATION:   return "Translation";
        case WERNICKE_NLP_MODE_INFERENCE:     return "Inference";
        default:                              return "Unknown";
    }
}

const char* wernicke_nlp_state_to_string(wernicke_nlp_state_t state) {
    switch (state) {
        case WERNICKE_NLP_STATE_IDLE:      return "Idle";
        case WERNICKE_NLP_STATE_RECEIVING: return "Receiving";
        case WERNICKE_NLP_STATE_LEXICAL:   return "Lexical";
        case WERNICKE_NLP_STATE_SEMANTIC:  return "Semantic";
        case WERNICKE_NLP_STATE_SYNTACTIC: return "Syntactic";
        case WERNICKE_NLP_STATE_COMPLETE:  return "Complete";
        case WERNICKE_NLP_STATE_ERROR:     return "Error";
        default:                           return "Unknown";
    }
}

const char* wernicke_crossmodal_mode_to_string(wernicke_crossmodal_mode_t mode) {
    switch (mode) {
        case WERNICKE_CROSSMODAL_AUDIO_ONLY:   return "Audio Only";
        case WERNICKE_CROSSMODAL_VISUAL_ONLY:  return "Visual Only";
        case WERNICKE_CROSSMODAL_AUDIOVISUAL:  return "Audiovisual";
        case WERNICKE_CROSSMODAL_TACTILE:      return "Tactile";
        default:                               return "Unknown";
    }
}
