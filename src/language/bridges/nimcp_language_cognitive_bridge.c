#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_language_cognitive_bridge.c - Language-Cognitive Bridge Implementation
//=============================================================================
/**
 * @file nimcp_language_cognitive_bridge.c
 * @brief Implementation of bidirectional Language-Cognitive bridge
 *
 * WHAT: Bridge connecting language processing with cognitive systems
 * WHY:  Enable working memory, attention, semantic memory, and reasoning
 *       to participate in language comprehension and production
 * HOW:  WM holds phonological loop, attention guides processing,
 *       semantic memory provides concepts, reasoning aids interpretation
 *
 * @version 1.0.0 - Phase L2: Language Layer Bridges
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#include "language/bridges/nimcp_language_cognitive_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#define LOG_MODULE "LANG_COGNITIVE_BRIDGE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(language_cognitive_bridge)

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Initialize working memory data
 */
static int init_wm_data(language_cognitive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_wm_data: bridge is NULL");
        return -1;
    }

    language_working_memory_data_t* data = &bridge->wm_data;

    data->phonological_capacity = bridge->config.phonological_buffer_size > 0 ?
        bridge->config.phonological_buffer_size : LANGUAGE_COGNITIVE_DEFAULT_WM_SLOTS;

    data->phonological_buffer = (phonological_item_t*)nimcp_calloc(
        data->phonological_capacity, sizeof(phonological_item_t));
    if (!data->phonological_buffer) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate phonological buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_wm_data: data->phonological_buffer is NULL");
        return -1;
    }

    data->phonological_count = 0;
    data->articulatory_rehearsal_active = true;
    data->rehearsal_rate = 3.0f;  /* ~3 items per second */
    data->current_rehearsal_idx = 0;

    data->binding_dim = 128;
    data->episodic_binding = (float*)nimcp_calloc(data->binding_dim, sizeof(float));
    if (!data->episodic_binding) {
        nimcp_free(data->phonological_buffer);
        data->phonological_buffer = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_wm_data: data->episodic_binding is NULL");
        return -1;
    }

    return 0;
}

/**
 * @brief Initialize attention data
 */
static int init_attention_data(language_cognitive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_attention_data: bridge is NULL");
        return -1;
    }

    language_attention_data_t* data = &bridge->attention_data;

    data->num_features = 64;
    data->feature_weights = (float*)nimcp_calloc(data->num_features, sizeof(float));
    if (!data->feature_weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_attention_data: data->feature_weights is NULL");
        return -1;
    }

    /* Initialize uniform attention */
    for (uint32_t i = 0; i < data->num_features; i++) {
        data->feature_weights[i] = 1.0f / (float)data->num_features;
    }

    data->num_words = 32;
    data->word_attention = (float*)nimcp_calloc(data->num_words, sizeof(float));
    if (!data->word_attention) {
        nimcp_free(data->feature_weights);
        data->feature_weights = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_attention_data: data->word_attention is NULL");
        return -1;
    }

    data->focus_word_idx = 0;
    data->linguistic_attention = LANGUAGE_COGNITIVE_ATTENTION_BASE;
    data->current_target = ATTENTION_TARGET_WORD;
    data->attention_gain = 1.0f;
    data->suppression_active = false;

    return 0;
}

/**
 * @brief Initialize semantic memory data
 */
static int init_semantic_data(language_cognitive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_semantic_data: bridge is NULL");
        return -1;
    }

    language_semantic_memory_data_t* data = &bridge->semantic_data;

    data->max_concepts = LANGUAGE_COGNITIVE_MAX_ACTIVE_CONCEPTS;
    data->active_concepts = (active_concept_t*)nimcp_calloc(
        data->max_concepts, sizeof(active_concept_t));
    if (!data->active_concepts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_semantic_data: data->active_concepts is NULL");
        return -1;
    }

    data->num_active = 0;
    data->spreading_mode = SPREADING_MODE_WEIGHTED;
    data->spreading_decay = LANGUAGE_COGNITIVE_DEFAULT_SPREADING_DECAY;
    data->max_spreading_depth = LANGUAGE_COGNITIVE_DEFAULT_SPREADING_DEPTH;
    data->spreading_in_progress = false;

    data->context_dim = 256;
    data->context_vector = (float*)nimcp_calloc(data->context_dim, sizeof(float));
    if (!data->context_vector) {
        nimcp_free(data->active_concepts);
        data->active_concepts = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_semantic_data: data->context_vector is NULL");
        return -1;
    }
    data->context_coherence = 0.0f;

    return 0;
}

/**
 * @brief Initialize reasoning data
 */
static int init_reasoning_data(language_cognitive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_reasoning_data: bridge is NULL");
        return -1;
    }

    language_reasoning_data_t* data = &bridge->reasoning_data;

    data->max_inferences = 16;
    data->inferences = (language_inference_t*)nimcp_calloc(
        data->max_inferences, sizeof(language_inference_t));
    if (!data->inferences) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_reasoning_data: data->inferences is NULL");
        return -1;
    }
    data->num_inferences = 0;

    data->num_candidates = 8;
    data->reference_candidates = (uint32_t*)nimcp_calloc(data->num_candidates, sizeof(uint32_t));
    data->reference_scores = (float*)nimcp_calloc(data->num_candidates, sizeof(float));
    if (!data->reference_candidates || !data->reference_scores) {
        nimcp_free(data->inferences);
        nimcp_free(data->reference_candidates);
        nimcp_free(data->reference_scores);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_reasoning_data: required parameter is NULL (data->reference_candidates, data->reference_scores)");
        return -1;
    }

    data->implicature_strength = 0.5f;
    data->inference_enabled = true;

    return 0;
}

/**
 * @brief Cleanup all data
 */
static void cleanup_all_data(language_cognitive_bridge_t* bridge) {
    if (!bridge) return;

    /* Working memory */
    if (bridge->wm_data.phonological_buffer) {
        nimcp_free(bridge->wm_data.phonological_buffer);
    }
    if (bridge->wm_data.episodic_binding) {
        nimcp_free(bridge->wm_data.episodic_binding);
    }

    /* Attention */
    if (bridge->attention_data.feature_weights) {
        nimcp_free(bridge->attention_data.feature_weights);
    }
    if (bridge->attention_data.word_attention) {
        nimcp_free(bridge->attention_data.word_attention);
    }

    /* Semantic */
    if (bridge->semantic_data.active_concepts) {
        /* Free semantic vectors in concepts */
        for (uint32_t i = 0; i < bridge->semantic_data.num_active; i++) {
            if (bridge->semantic_data.active_concepts[i].semantic_vector) {
                nimcp_free(bridge->semantic_data.active_concepts[i].semantic_vector);
            }
        }
        nimcp_free(bridge->semantic_data.active_concepts);
    }
    if (bridge->semantic_data.context_vector) {
        nimcp_free(bridge->semantic_data.context_vector);
    }

    /* Reasoning */
    if (bridge->reasoning_data.inferences) {
        nimcp_free(bridge->reasoning_data.inferences);
    }
    if (bridge->reasoning_data.reference_candidates) {
        nimcp_free(bridge->reasoning_data.reference_candidates);
    }
    if (bridge->reasoning_data.reference_scores) {
        nimcp_free(bridge->reasoning_data.reference_scores);
    }
}

/**
 * @brief Apply decay to phonological buffer items
 */
static void apply_phonological_decay(language_cognitive_bridge_t* bridge, uint64_t current_time_ms) {
    if (!bridge) return;

    language_working_memory_data_t* wm = &bridge->wm_data;

    for (uint32_t i = 0; i < wm->phonological_count; i++) {
        phonological_item_t* item = &wm->phonological_buffer[i];

        /* Calculate time since last rehearsal */
        uint64_t elapsed = current_time_ms - item->last_rehearsal_ms;

        /* Apply exponential decay (~2 second half-life without rehearsal) */
        float decay = powf(item->decay_rate, (float)elapsed / 1000.0f);
        item->activation *= decay;

        /* Mark for rehearsal if activation dropping */
        if (item->activation < 0.5f) {
            item->needs_rehearsal = true;
        }
    }

    /* Remove items with activation below threshold */
    uint32_t write_idx = 0;
    for (uint32_t i = 0; i < wm->phonological_count; i++) {
        if (wm->phonological_buffer[i].activation >= LANGUAGE_COGNITIVE_ACTIVATION_THRESHOLD) {
            if (write_idx != i) {
                memcpy(&wm->phonological_buffer[write_idx],
                       &wm->phonological_buffer[i],
                       sizeof(phonological_item_t));
            }
            write_idx++;
        } else {
            bridge->stats.items_forgotten++;
        }
    }
    wm->phonological_count = write_idx;
}

/**
 * @brief Perform subvocal rehearsal
 */
static void perform_rehearsal(language_cognitive_bridge_t* bridge, uint64_t current_time_ms) {
    if (!bridge || !bridge->wm_data.articulatory_rehearsal_active) return;

    language_working_memory_data_t* wm = &bridge->wm_data;

    if (wm->phonological_count == 0) return;

    /* Rehearse one item per call (round-robin) */
    phonological_item_t* item = &wm->phonological_buffer[wm->current_rehearsal_idx];

    item->activation = 1.0f;  /* Refresh activation */
    item->last_rehearsal_ms = current_time_ms;
    item->needs_rehearsal = false;

    bridge->stats.items_rehearsed++;

    /* Move to next item */
    wm->current_rehearsal_idx = (wm->current_rehearsal_idx + 1) % wm->phonological_count;
}

/**
 * @brief Apply spreading activation decay
 */
static void apply_semantic_decay(language_cognitive_bridge_t* bridge, uint64_t current_time_ms) {
    if (!bridge) return;

    language_semantic_memory_data_t* sem = &bridge->semantic_data;

    /* Apply decay to all active concepts */
    uint32_t write_idx = 0;
    for (uint32_t i = 0; i < sem->num_active; i++) {
        active_concept_t* concept = &sem->active_concepts[i];

        /* Time-based decay */
        uint64_t elapsed = current_time_ms - concept->activation_time_ms;
        float decay = powf(0.95f, (float)elapsed / 1000.0f);
        concept->activation *= decay;

        /* Keep if above threshold */
        if (concept->activation >= LANGUAGE_COGNITIVE_ACTIVATION_THRESHOLD) {
            if (write_idx != i) {
                memcpy(&sem->active_concepts[write_idx],
                       &sem->active_concepts[i],
                       sizeof(active_concept_t));
            }
            write_idx++;
        } else {
            /* Free semantic vector before removal */
            if (concept->semantic_vector) {
                nimcp_free(concept->semantic_vector);
            }
        }
    }
    sem->num_active = write_idx;
}

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

language_cognitive_bridge_t* language_cognitive_bridge_create(
    const language_cognitive_config_t* config)
{
    language_cognitive_bridge_t* bridge = (language_cognitive_bridge_t*)
        nimcp_calloc(1, sizeof(language_cognitive_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_cognitive_bridge_create: allocation failed");
        LOG_ERROR(LOG_MODULE, "Failed to allocate bridge");
        return NULL;
    }

    /* Copy configuration */
    if (config) {
        memcpy(&bridge->config, config, sizeof(language_cognitive_config_t));
    } else {
        language_cognitive_default_config(&bridge->config);
    }

    /* Initialize data structures */
    if (init_wm_data(bridge) != 0 ||
        init_attention_data(bridge) != 0 ||
        init_semantic_data(bridge) != 0 ||
        init_reasoning_data(bridge) != 0) {
        cleanup_all_data(bridge);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "language_cognitive_bridge_create: validation failed");
        return NULL;
    }

    bridge->initialized = true;
    bridge->active = false;

    LOG_INFO(LOG_MODULE, "Cognitive bridge created");
    return bridge;
}

void language_cognitive_bridge_destroy(language_cognitive_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "language_cognitive");

    if (bridge->bio_async_registered) {
        language_cognitive_bridge_bio_async_unregister(bridge);
    }

    cleanup_all_data(bridge);
    nimcp_free(bridge);

    LOG_INFO(LOG_MODULE, "Cognitive bridge destroyed");
}

int language_cognitive_bridge_init(language_cognitive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_init: bridge is NULL");
        return -1;
    }

    memset(&bridge->stats, 0, sizeof(language_cognitive_stats_t));
    bridge->initialized = true;

    LOG_DEBUG(LOG_MODULE, "Cognitive bridge initialized");
    return 0;
}

int language_cognitive_bridge_start(language_cognitive_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_start: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    bridge->active = true;
    LOG_INFO(LOG_MODULE, "Cognitive bridge started");
    return 0;
}

int language_cognitive_bridge_stop(language_cognitive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_stop: bridge is NULL");
        return -1;
    }

    bridge->active = false;
    LOG_INFO(LOG_MODULE, "Cognitive bridge stopped");
    return 0;
}

//=============================================================================
// Connection API Implementation
//=============================================================================

int language_cognitive_bridge_connect_orchestrator(
    language_cognitive_bridge_t* bridge,
    language_orchestrator_t* orchestrator)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_connect_orchestrator: bridge is NULL");
        return -1;
    }
    bridge->orchestrator = orchestrator;
    return 0;
}

int language_cognitive_bridge_connect_working_memory(
    language_cognitive_bridge_t* bridge,
    working_memory_system_t* working_memory)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_connect_working_memory: bridge is NULL");
        return -1;
    }
    bridge->working_memory = working_memory;
    return 0;
}

int language_cognitive_bridge_connect_attention(
    language_cognitive_bridge_t* bridge,
    attention_system_t* attention)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_connect_attention: bridge is NULL");
        return -1;
    }
    bridge->attention = attention;
    return 0;
}

int language_cognitive_bridge_connect_semantic_memory(
    language_cognitive_bridge_t* bridge,
    semantic_memory_system_t* semantic_memory)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_connect_semantic_memory: bridge is NULL");
        return -1;
    }
    bridge->semantic_memory = semantic_memory;
    return 0;
}

int language_cognitive_bridge_connect_reasoning(
    language_cognitive_bridge_t* bridge,
    reasoning_engine_t* reasoning)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_connect_reasoning: bridge is NULL");
        return -1;
    }
    bridge->reasoning = reasoning;
    return 0;
}

int language_cognitive_bridge_connect_executive(
    language_cognitive_bridge_t* bridge,
    executive_controller_t* executive)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_connect_executive: bridge is NULL");
        return -1;
    }
    bridge->executive = executive;
    return 0;
}

//=============================================================================
// Working Memory API Implementation
//=============================================================================

int language_cognitive_bridge_wm_add_word(
    language_cognitive_bridge_t* bridge,
    const language_word_t* word)
{
    if (!bridge || !word) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_wm_add_word: required parameter is NULL (bridge, word)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_wm_add_word: bridge->active is NULL");
        return -1;
    }

    language_working_memory_data_t* wm = &bridge->wm_data;

    if (wm->phonological_count >= wm->phonological_capacity) {
        LOG_WARN(LOG_MODULE, "Phonological buffer full");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "language_cognitive_bridge_wm_add_word: capacity exceeded");
        return -1;
    }

    phonological_item_t* item = &wm->phonological_buffer[wm->phonological_count];
    item->word_id = word->id;
    strncpy(item->phonological_form, word->form, sizeof(item->phonological_form) - 1);
    item->phonological_form[sizeof(item->phonological_form) - 1] = '\0';
    item->activation = 1.0f;
    item->decay_rate = LANGUAGE_COGNITIVE_DEFAULT_REHEARSAL_DECAY;
    item->entry_time_ms = bridge->stats.last_update_time_ms;
    item->last_rehearsal_ms = item->entry_time_ms;
    item->needs_rehearsal = false;

    wm->phonological_count++;

    return 0;
}

int language_cognitive_bridge_wm_rehearse(language_cognitive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_wm_rehearse: bridge is NULL");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_wm_rehearse: bridge->active is NULL");
        return -1;
    }

    perform_rehearsal(bridge, bridge->stats.last_update_time_ms);
    return 0;
}

int language_cognitive_bridge_wm_get_items(
    const language_cognitive_bridge_t* bridge,
    phonological_item_t* items,
    uint32_t max_items)
{
    if (!bridge || !items) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_wm_get_items: required parameter is NULL (bridge, items)");
        return -1;
    }

    const language_working_memory_data_t* wm = &bridge->wm_data;
    uint32_t count = wm->phonological_count < max_items ? wm->phonological_count : max_items;

    memcpy(items, wm->phonological_buffer, count * sizeof(phonological_item_t));
    return (int)count;
}

float language_cognitive_bridge_wm_get_load(const language_cognitive_bridge_t* bridge) {
    if (!bridge) return 0.0f;

    const language_working_memory_data_t* wm = &bridge->wm_data;
    if (wm->phonological_capacity == 0) return 0.0f;

    return (float)wm->phonological_count / (float)wm->phonological_capacity;
}

//=============================================================================
// Semantic Memory API Implementation
//=============================================================================

uint32_t language_cognitive_bridge_activate_concept(
    language_cognitive_bridge_t* bridge,
    uint32_t word_id,
    float activation)
{
    if (!bridge) return 0;
    if (!bridge->active) return 0;

    language_semantic_memory_data_t* sem = &bridge->semantic_data;

    /* Check if concept already active */
    for (uint32_t i = 0; i < sem->num_active; i++) {
        if (sem->active_concepts[i].source_word_id == word_id) {
            /* Boost existing activation */
            sem->active_concepts[i].activation += activation;
            if (sem->active_concepts[i].activation > 1.0f) {
                sem->active_concepts[i].activation = 1.0f;
            }
            return sem->active_concepts[i].concept_id;
        }
    }

    /* Add new concept */
    if (sem->num_active >= sem->max_concepts) {
        LOG_WARN(LOG_MODULE, "Concept buffer full");
        return 0;
    }

    active_concept_t* concept = &sem->active_concepts[sem->num_active];
    concept->concept_id = word_id + 1000;  /* Simple ID mapping */
    snprintf(concept->name, sizeof(concept->name), "concept_%u", word_id);
    concept->activation = activation;
    concept->source_word_id = word_id;
    concept->spreading_depth = 0;
    concept->activation_time_ms = bridge->stats.last_update_time_ms;

    /* Allocate semantic vector */
    concept->semantic_dim = 128;
    concept->semantic_vector = (float*)nimcp_calloc(concept->semantic_dim, sizeof(float));
    if (concept->semantic_vector) {
        /* Initialize with pseudo-random embedding based on word_id */
        for (uint32_t i = 0; i < concept->semantic_dim; i++) {
            concept->semantic_vector[i] = sinf((float)(word_id * i) * 0.1f) * 0.5f;
        }
    }

    sem->num_active++;
    bridge->stats.concepts_activated++;

    return concept->concept_id;
}

int language_cognitive_bridge_spread_activation(
    language_cognitive_bridge_t* bridge,
    uint32_t source_concept_id)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_spread_activation: bridge is NULL");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_spread_activation: bridge->active is NULL");
        return -1;
    }

    language_semantic_memory_data_t* sem = &bridge->semantic_data;

    /* Find source concept */
    active_concept_t* source = NULL;
    for (uint32_t i = 0; i < sem->num_active; i++) {
        if (sem->active_concepts[i].concept_id == source_concept_id) {
            source = &sem->active_concepts[i];
            break;
        }
    }

    if (!source) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_spread_activation: source is NULL");
        return -1;
    }

    /* Mark spreading in progress */
    sem->spreading_in_progress = true;

    /* Simulate spreading to related concepts */
    /* In production, this would query the semantic memory network */
    float spread_activation = source->activation * sem->spreading_decay;

    if (spread_activation > LANGUAGE_COGNITIVE_ACTIVATION_THRESHOLD &&
        source->spreading_depth < sem->max_spreading_depth) {
        /* Create pseudo-related concepts */
        uint32_t related_id = source_concept_id + 100;
        language_cognitive_bridge_activate_concept(bridge, related_id, spread_activation);
    }

    sem->spreading_in_progress = false;
    bridge->stats.spreading_events++;

    return 0;
}

int language_cognitive_bridge_get_active_concepts(
    const language_cognitive_bridge_t* bridge,
    active_concept_t* concepts,
    uint32_t max_concepts)
{
    if (!bridge || !concepts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_get_active_concepts: required parameter is NULL (bridge, concepts)");
        return -1;
    }

    const language_semantic_memory_data_t* sem = &bridge->semantic_data;
    uint32_t count = sem->num_active < max_concepts ? sem->num_active : max_concepts;

    memcpy(concepts, sem->active_concepts, count * sizeof(active_concept_t));
    return (int)count;
}

int language_cognitive_bridge_get_concept_for_word(
    const language_cognitive_bridge_t* bridge,
    uint32_t word_id,
    active_concept_t* concept)
{
    if (!bridge || !concept) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_get_concept_for_word: required parameter is NULL (bridge, concept)");
        return -1;
    }

    const language_semantic_memory_data_t* sem = &bridge->semantic_data;

    for (uint32_t i = 0; i < sem->num_active; i++) {
        if (sem->active_concepts[i].source_word_id == word_id) {
            memcpy(concept, &sem->active_concepts[i], sizeof(active_concept_t));
            return 0;
        }
    }

    return -1;  /* Not found - normal lookup behavior */
}

//=============================================================================
// Attention API Implementation
//=============================================================================

int language_cognitive_bridge_set_word_attention(
    language_cognitive_bridge_t* bridge,
    uint32_t word_idx,
    float attention)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_set_word_attention: bridge is NULL");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_set_word_attention: bridge->active is NULL");
        return -1;
    }

    language_attention_data_t* attn = &bridge->attention_data;

    if (word_idx >= attn->num_words) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "language_cognitive_bridge_set_word_attention: capacity exceeded");
        return -1;
    }

    attn->word_attention[word_idx] = attention;
    if (attention > attn->word_attention[attn->focus_word_idx]) {
        attn->focus_word_idx = word_idx;
    }

    return 0;
}

int language_cognitive_bridge_get_attention(
    const language_cognitive_bridge_t* bridge,
    float* attention,
    uint32_t max_words)
{
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }

    const language_attention_data_t* attn = &bridge->attention_data;
    uint32_t count = attn->num_words < max_words ? attn->num_words : max_words;

    memcpy(attention, attn->word_attention, count * sizeof(float));
    return (int)count;
}

int language_cognitive_bridge_set_attention_target(
    language_cognitive_bridge_t* bridge,
    attention_target_t target)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_set_attention_target: bridge is NULL");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_set_attention_target: bridge->active is NULL");
        return -1;
    }

    bridge->attention_data.current_target = target;
    return 0;
}

//=============================================================================
// Reasoning API Implementation
//=============================================================================

int language_cognitive_bridge_request_inference(
    language_cognitive_bridge_t* bridge,
    inference_type_t type)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_request_inference: bridge is NULL");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_request_inference: bridge->active is NULL");
        return -1;
    }

    language_reasoning_data_t* reason = &bridge->reasoning_data;

    if (!reason->inference_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_request_inference: reason->inference_enabled is NULL");
        return -1;
    }

    if (reason->num_inferences >= reason->max_inferences) {
        LOG_WARN(LOG_MODULE, "Inference buffer full");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "language_cognitive_bridge_request_inference: capacity exceeded");
        return -1;
    }

    /* Create inference request */
    language_inference_t* inf = &reason->inferences[reason->num_inferences];
    inf->type = type;
    inf->source_word_idx = 0;  /* Would be set by caller */
    inf->confidence = 0.0f;    /* Computed later */
    inf->inference_text[0] = '\0';
    inf->resolved_reference_id = 0;
    inf->valid = false;

    reason->num_inferences++;
    bridge->stats.inferences_made++;

    return 0;
}

int language_cognitive_bridge_get_inferences(
    const language_cognitive_bridge_t* bridge,
    language_inference_t* inferences,
    uint32_t max_inferences)
{
    if (!bridge || !inferences) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_get_inferences: required parameter is NULL (bridge, inferences)");
        return -1;
    }

    const language_reasoning_data_t* reason = &bridge->reasoning_data;
    uint32_t count = reason->num_inferences < max_inferences ?
        reason->num_inferences : max_inferences;

    memcpy(inferences, reason->inferences, count * sizeof(language_inference_t));
    return (int)count;
}

int language_cognitive_bridge_resolve_reference(
    language_cognitive_bridge_t* bridge,
    uint32_t referring_word_idx,
    uint32_t* referent_id)
{
    if (!bridge || !referent_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_resolve_reference: required parameter is NULL (bridge, referent_id)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_resolve_reference: bridge->active is NULL");
        return -1;
    }

    language_reasoning_data_t* reason = &bridge->reasoning_data;

    /* Simple reference resolution: return first candidate above threshold */
    for (uint32_t i = 0; i < reason->num_candidates; i++) {
        if (reason->reference_scores[i] > 0.5f) {
            *referent_id = reason->reference_candidates[i];
            return 0;
        }
    }

    return -1;  /* Unresolved - normal behavior when no referent found */
}

//=============================================================================
// Executive API Implementation
//=============================================================================

int language_cognitive_bridge_report_conflict(
    language_cognitive_bridge_t* bridge,
    uint32_t word_idx,
    float conflict_level)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_report_conflict: bridge is NULL");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_report_conflict: bridge->active is NULL");
        return -1;
    }

    bridge->executive_data.conflict_level = conflict_level;
    bridge->executive_data.ambiguity_detected = (conflict_level > 0.3f);
    bridge->executive_data.ambiguity_word_idx = word_idx;

    return 0;
}

float language_cognitive_bridge_get_conflict(const language_cognitive_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->executive_data.conflict_level;
}

//=============================================================================
// Update and Query API Implementation
//=============================================================================

int language_cognitive_bridge_update(
    language_cognitive_bridge_t* bridge,
    uint64_t current_time_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_update: bridge is NULL");
        return -1;
    }
    if (!bridge->active) return 0;

    bridge->stats.last_update_time_ms = current_time_ms;

    /* Apply decay to phonological buffer */
    apply_phonological_decay(bridge, current_time_ms);

    /* Perform rehearsal if enabled */
    if (bridge->wm_data.articulatory_rehearsal_active) {
        perform_rehearsal(bridge, current_time_ms);
    }

    /* Apply decay to semantic activations */
    apply_semantic_decay(bridge, current_time_ms);

    /* Update statistics */
    bridge->stats.avg_buffer_utilization =
        (bridge->stats.avg_buffer_utilization * 0.9f) +
        (language_cognitive_bridge_wm_get_load(bridge) * 0.1f);

    if (bridge->semantic_data.num_active > 0) {
        float sum_activation = 0.0f;
        for (uint32_t i = 0; i < bridge->semantic_data.num_active; i++) {
            sum_activation += bridge->semantic_data.active_concepts[i].activation;
        }
        bridge->stats.avg_concept_activation =
            sum_activation / (float)bridge->semantic_data.num_active;
    }

    return 0;
}

int language_cognitive_bridge_get_stats(
    const language_cognitive_bridge_t* bridge,
    language_cognitive_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    memcpy(stats, &bridge->stats, sizeof(language_cognitive_stats_t));
    return 0;
}

//=============================================================================
// Bio-Async Integration Implementation
//=============================================================================

int language_cognitive_bridge_bio_async_register(
    language_cognitive_bridge_t* bridge,
    bio_router_t* router)
{
    if (!bridge || !router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_bio_async_register: required parameter is NULL (bridge, router)");
        return -1;
    }

    bridge->bio_router = router;
    bridge->bio_async_registered = true;

    LOG_DEBUG(LOG_MODULE, "Registered with bio-async router");
    return 0;
}

int language_cognitive_bridge_bio_async_unregister(language_cognitive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cognitive_bridge_bio_async_unregister: bridge is NULL");
        return -1;
    }

    bridge->bio_router = NULL;
    bridge->bio_async_registered = false;

    LOG_DEBUG(LOG_MODULE, "Unregistered from bio-async router");
    return 0;
}
