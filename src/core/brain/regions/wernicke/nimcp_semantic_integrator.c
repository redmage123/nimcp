/**
 * @file nimcp_semantic_integrator.c
 * @brief Semantic integration layer for Wernicke's area - Implementation
 *
 * Implements word-to-concept mapping, sense disambiguation, context
 * integration, and semantic priming for language comprehension.
 *
 * @version Phase W2: Wernicke's Area Semantic Integration
 * @date 2026-01-04
 */

#include "core/brain/regions/wernicke/nimcp_semantic_integrator.h"
#include "core/brain/regions/wernicke/nimcp_lexical_access.h"
#include "cognitive/memory/nimcp_semantic_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(semantic_integrator, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Word-sense entry in sense table
 */
typedef struct {
    uint32_t word_id;                  /**< Word ID */
    word_sense_t* senses;              /**< Array of senses */
    uint32_t num_senses;               /**< Number of senses */
    uint32_t selected_sense;           /**< Currently selected sense (0 = none) */
    float selection_confidence;        /**< Selection confidence */
} sense_entry_t;

/**
 * @brief Sense table hash bucket
 */
typedef struct sense_bucket {
    sense_entry_t entry;
    struct sense_bucket* next;
} sense_bucket_t;

/**
 * @brief Semantic integrator internal state
 */
struct semantic_integrator {
    semantic_config_t config;

    /* Sense table (hash map: word_id -> senses) */
    sense_bucket_t** sense_table;
    uint32_t sense_table_size;
    uint32_t sense_count;

    /* Context buffer */
    sentence_context_t context;

    /* Active concepts */
    active_concept_t* active_concepts;
    uint32_t num_active;
    uint32_t max_active;

    /* Semantic priming */
    semantic_priming_t priming;

    /* Thematic frame */
    uint32_t role_fillers[ROLE_COUNT];
    float role_confidences[ROLE_COUNT];

    /* Anomaly detection */
    float anomaly_threshold;
    float cumulative_anomaly;

    /* Statistics */
    semantic_stats_t stats;

    /* External connections */
    semantic_memory_system_t* memory;

    /* Internal state */
    uint32_t next_sense_id;
    bool in_utterance;
};

/*=============================================================================
 * INTERNAL CONSTANTS
 *===========================================================================*/

#define SENSE_TABLE_SIZE 1024
#define MAX_PRIMED_CONCEPTS 64
#define CONTEXT_FEATURE_DIM 32

/* Thematic role names */
static const char* ROLE_NAMES[] = {
    "NONE",
    "AGENT",
    "PATIENT",
    "THEME",
    "EXPERIENCER",
    "BENEFICIARY",
    "INSTRUMENT",
    "LOCATION",
    "SOURCE",
    "GOAL",
    "TIME"
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Hash function for word IDs
 */
static inline uint32_t hash_word_id(uint32_t word_id, uint32_t table_size) {
    /* Simple multiplicative hash */
    return (word_id * 2654435761u) % table_size;
}

/**
 * @brief Compute cosine similarity
 */
static float compute_similarity(
    const float* a,
    const float* b,
    uint32_t dim
) {
    if (!a || !b || dim == 0) return 0.0f;

    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;

    for (uint32_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    if (norm_a < 1e-10f || norm_b < 1e-10f) return 0.0f;

    return dot / (sqrtf(norm_a) * sqrtf(norm_b));
}

/**
 * @brief Get sense entry for word
 */
static sense_entry_t* get_sense_entry(
    semantic_integrator_t* sem,
    uint32_t word_id,
    bool create
) {
    uint32_t hash = hash_word_id(word_id, sem->sense_table_size);
    sense_bucket_t* bucket = sem->sense_table[hash];

    /* Search chain */
    while (bucket) {
        if (bucket->entry.word_id == word_id) {
            return &bucket->entry;
        }
        bucket = bucket->next;
    }

    if (!create) {
        return NULL;  /* Not found and create=false — normal lookup miss */
    }

    /* Create new entry */
    sense_bucket_t* new_bucket = nimcp_calloc(1, sizeof(sense_bucket_t));
    if (!new_bucket) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "new_bucket is NULL");

        return NULL;

    }

    new_bucket->entry.word_id = word_id;
    new_bucket->entry.senses = nimcp_calloc(sem->config.max_senses, sizeof(word_sense_t));
    if (!new_bucket->entry.senses) {
        nimcp_free(new_bucket);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "get_sense_entry: new_bucket->entry is NULL");
        return NULL;
    }

    new_bucket->next = sem->sense_table[hash];
    sem->sense_table[hash] = new_bucket;
    sem->sense_count++;

    return &new_bucket->entry;
}

/**
 * @brief Add concept to active list
 */
static bool add_active_concept(
    semantic_integrator_t* sem,
    uint32_t concept_id,
    float activation,
    uint32_t source_word,
    uint32_t sense_id
) {
    /* Check if already active */
    for (uint32_t i = 0; i < sem->num_active; i++) {
        if (sem->active_concepts[i].concept_id == concept_id) {
            /* Update existing */
            if (activation > sem->active_concepts[i].activation) {
                sem->active_concepts[i].activation = activation;
                sem->active_concepts[i].source_word_id = source_word;
                sem->active_concepts[i].sense_id = sense_id;
            }
            return true;
        }
    }

    /* Add new if room */
    if (sem->num_active >= sem->max_active) {
        /* Find lowest activation to replace */
        uint32_t min_idx = 0;
        float min_act = sem->active_concepts[0].activation;
        for (uint32_t i = 1; i < sem->num_active; i++) {
            if (sem->active_concepts[i].activation < min_act) {
                min_act = sem->active_concepts[i].activation;
                min_idx = i;
            }
        }
        if (activation > min_act) {
            sem->active_concepts[min_idx].concept_id = concept_id;
            sem->active_concepts[min_idx].activation = activation;
            sem->active_concepts[min_idx].source_word_id = source_word;
            sem->active_concepts[min_idx].sense_id = sense_id;
            sem->active_concepts[min_idx].role = ROLE_NONE;
            return true;
        }
        return false;
    }

    /* Add to end */
    sem->active_concepts[sem->num_active].concept_id = concept_id;
    sem->active_concepts[sem->num_active].activation = activation;
    sem->active_concepts[sem->num_active].source_word_id = source_word;
    sem->active_concepts[sem->num_active].sense_id = sense_id;
    sem->active_concepts[sem->num_active].role = ROLE_NONE;
    sem->num_active++;

    return true;
}

/**
 * @brief Update context vector with new word
 */
static void update_context_vector(
    semantic_integrator_t* sem,
    const float* features,
    uint32_t dim
) {
    if (!sem->context.context_vector || !features) return;

    uint32_t use_dim = (dim < sem->context.context_dim) ?
                       dim : sem->context.context_dim;

    /* Weighted blend with decay */
    float weight = 1.0f / (float)(sem->context.num_words + 1);

    for (uint32_t i = 0; i < use_dim; i++) {
        sem->context.context_vector[i] =
            (1.0f - weight) * sem->context.context_vector[i] +
            weight * features[i];
    }
}

/**
 * @brief Compute context fit for sense
 */
static float compute_context_fit(
    semantic_integrator_t* sem,
    const word_sense_t* sense
) {
    if (!sem->context.context_vector || sem->context.num_words == 0) {
        return 0.5f;  /* Neutral without context */
    }

    /* Use concept features from semantic memory if available */
    if (sem->memory && sense->concept_id > 0) {
        const semantic_concept_t* concept =
            semantic_memory_get_concept(sem->memory, sense->concept_id);
        if (concept && concept->features) {
            return compute_similarity(
                sem->context.context_vector,
                concept->features,
                sem->context.context_dim
            );
        }
    }

    /* Fallback: use priming if concept is primed */
    for (uint32_t i = 0; i < sem->priming.num_primed; i++) {
        if (sem->priming.primed_concepts[i] == sense->concept_id) {
            return 0.5f + 0.5f * sem->priming.priming_levels[i];
        }
    }

    return 0.5f;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

semantic_config_t semantic_default_config(void) {
    return (semantic_config_t){
        .max_senses = SEM_DEFAULT_MAX_SENSES,
        .max_context_words = SEM_DEFAULT_MAX_CONTEXT_WORDS,
        .max_active_concepts = SEM_DEFAULT_MAX_ACTIVE_CONCEPTS,
        .concept_feature_dim = SEM_DEFAULT_CONCEPT_FEATURE_DIM,
        .context_window = SEM_DEFAULT_CONTEXT_WINDOW,
        .context_decay = NIMCP_EMA_DECAY_FAST,
        .strategy = DISAMBIG_COMBINED,
        .disambiguation_threshold = SEM_DEFAULT_DISAMBIGUATION_THRESHOLD,
        .frequency_weight = 0.4f,
        .context_weight = 0.6f,
        .activation_threshold = 0.1f,
        .activation_decay = NIMCP_ELIGIBILITY_DECAY_DEFAULT,
        .spreading_rate = 0.7f,
        .enable_spreading_activation = true,
        .enable_thematic_roles = true,
        .enable_inference = false,
        .enable_anomaly_detection = true
    };
}

semantic_integrator_t* semantic_create(const semantic_config_t* config) {
    semantic_integrator_t* sem = nimcp_calloc(1, sizeof(semantic_integrator_t));
    if (!sem) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sem is NULL");

        return NULL;

    }

    /* Use config or defaults */
    if (config) {
        sem->config = *config;
    } else {
        sem->config = semantic_default_config();
    }

    /* Allocate sense table */
    sem->sense_table_size = SENSE_TABLE_SIZE;
    sem->sense_table = nimcp_calloc(sem->sense_table_size, sizeof(sense_bucket_t*));
    if (!sem->sense_table) {
        nimcp_free(sem);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "semantic_create: sem->sense_table is NULL");
        return NULL;
    }

    /* Allocate context */
    sem->context.max_words = sem->config.max_context_words;
    sem->context.words = nimcp_calloc(sem->context.max_words, sizeof(context_word_t));
    sem->context.context_dim = sem->config.concept_feature_dim;
    sem->context.context_vector = nimcp_calloc(sem->context.context_dim, sizeof(float));

    if (!sem->context.words || !sem->context.context_vector) {
        semantic_destroy(sem);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "semantic_create: required parameter is NULL (sem->context, sem->context)");
        return NULL;
    }

    /* Allocate feature vectors for context words */
    for (uint32_t i = 0; i < sem->context.max_words; i++) {
        sem->context.words[i].concept_features =
            nimcp_calloc(sem->config.concept_feature_dim, sizeof(float));
    }

    /* Allocate active concepts */
    sem->max_active = sem->config.max_active_concepts;
    sem->active_concepts = nimcp_calloc(sem->max_active, sizeof(active_concept_t));
    if (!sem->active_concepts) {
        semantic_destroy(sem);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "semantic_create: sem->active_concepts is NULL");
        return NULL;
    }

    /* Allocate priming */
    sem->priming.max_primed = MAX_PRIMED_CONCEPTS;
    sem->priming.primed_concepts = nimcp_calloc(sem->priming.max_primed, sizeof(uint32_t));
    sem->priming.priming_levels = nimcp_calloc(sem->priming.max_primed, sizeof(float));

    if (!sem->priming.primed_concepts || !sem->priming.priming_levels) {
        semantic_destroy(sem);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "semantic_create: required parameter is NULL (sem->priming, sem->priming)");
        return NULL;
    }

    /* Initialize state */
    sem->next_sense_id = 1;
    sem->anomaly_threshold = 0.6f;
    sem->in_utterance = false;

    return sem;
}

void semantic_destroy(semantic_integrator_t* sem) {
    if (!sem) return;

    /* Free sense table */
    if (sem->sense_table) {
        for (uint32_t i = 0; i < sem->sense_table_size; i++) {
            sense_bucket_t* bucket = sem->sense_table[i];
            while (bucket) {
                sense_bucket_t* next = bucket->next;
                if (bucket->entry.senses) {
                    for (uint32_t j = 0; j < bucket->entry.num_senses; j++) {
                        nimcp_free(bucket->entry.senses[j].related_concepts);
                    }
                    nimcp_free(bucket->entry.senses);
                }
                nimcp_free(bucket);
                bucket = next;
            }
        }
        nimcp_free(sem->sense_table);
    }

    /* Free context */
    if (sem->context.words) {
        for (uint32_t i = 0; i < sem->context.max_words; i++) {
            nimcp_free(sem->context.words[i].concept_features);
        }
        nimcp_free(sem->context.words);
    }
    nimcp_free(sem->context.context_vector);

    /* Free active concepts */
    nimcp_free(sem->active_concepts);

    /* Free priming */
    nimcp_free(sem->priming.primed_concepts);
    nimcp_free(sem->priming.priming_levels);

    nimcp_free(sem);
}

bool semantic_reset(semantic_integrator_t* sem) {
    if (!sem) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_reset: sem is NULL");
        return false;
    }

    /* Clear context */
    sem->context.num_words = 0;
    sem->context.agent_concept = 0;
    sem->context.patient_concept = 0;
    sem->context.action_concept = 0;
    sem->context.coherence_score = 1.0f;
    sem->context.anomaly_level = 0.0f;

    if (sem->context.context_vector) {
        memset(sem->context.context_vector, 0,
               sem->context.context_dim * sizeof(float));
    }

    /* Clear active concepts */
    sem->num_active = 0;

    /* Clear priming */
    sem->priming.num_primed = 0;

    /* Clear thematic frame */
    memset(sem->role_fillers, 0, sizeof(sem->role_fillers));
    memset(sem->role_confidences, 0, sizeof(sem->role_confidences));

    /* Reset state */
    sem->cumulative_anomaly = 0.0f;
    sem->in_utterance = false;

    return true;
}

bool semantic_connect_memory(
    semantic_integrator_t* sem,
    void* memory  /* semantic_memory_system_t* */
) {
    if (!sem) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_connect_memory: sem is NULL");
        return false;
    }
    sem->memory = (semantic_memory_system_t*)memory;
    return true;
}

/*=============================================================================
 * WORD SENSE MANAGEMENT
 *===========================================================================*/

bool semantic_register_senses(
    semantic_integrator_t* sem,
    uint32_t word_id,
    const word_sense_t* senses,
    uint32_t num_senses
) {
    if (!sem || !senses || num_senses == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_register_senses: required parameter is NULL (sem, senses)");
        return false;
    }

    sense_entry_t* entry = get_sense_entry(sem, word_id, true);
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_register_senses: entry is NULL");
        return false;
    }

    /* Clear existing senses */
    for (uint32_t i = 0; i < entry->num_senses; i++) {
        nimcp_free(entry->senses[i].related_concepts);
    }
    entry->num_senses = 0;

    /* Copy new senses */
    uint32_t copy_count = (num_senses < sem->config.max_senses) ?
                          num_senses : sem->config.max_senses;

    for (uint32_t i = 0; i < copy_count; i++) {
        entry->senses[i] = senses[i];
        entry->senses[i].sense_id = sem->next_sense_id++;

        /* Copy related concepts if present */
        if (senses[i].related_concepts && senses[i].num_related > 0) {
            entry->senses[i].related_concepts =
                nimcp_malloc(senses[i].num_related * sizeof(uint32_t));
            if (entry->senses[i].related_concepts) {
                memcpy(entry->senses[i].related_concepts,
                       senses[i].related_concepts,
                       senses[i].num_related * sizeof(uint32_t));
            }
        } else {
            entry->senses[i].related_concepts = NULL;
            entry->senses[i].num_related = 0;
        }
    }

    entry->num_senses = copy_count;
    entry->selected_sense = 0;
    entry->selection_confidence = 0.0f;

    return true;
}

bool semantic_get_senses(
    const semantic_integrator_t* sem,
    uint32_t word_id,
    word_sense_t* senses,
    uint32_t max_senses,
    uint32_t* num_senses
) {
    if (!sem || !senses || !num_senses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_get_senses: required parameter is NULL (sem, senses, num_senses)");
        return false;
    }

    sense_entry_t* entry = get_sense_entry((semantic_integrator_t*)sem,
                                           word_id, false);
    if (!entry) {
        *num_senses = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_get_senses: entry is NULL");
        return false;
    }

    uint32_t copy_count = (entry->num_senses < max_senses) ?
                          entry->num_senses : max_senses;

    for (uint32_t i = 0; i < copy_count; i++) {
        senses[i] = entry->senses[i];
        /* Don't copy related_concepts pointer */
        senses[i].related_concepts = NULL;
    }

    *num_senses = copy_count;
    return true;
}

uint32_t semantic_add_sense(
    semantic_integrator_t* sem,
    uint32_t word_id,
    uint32_t concept_id,
    const char* gloss,
    float frequency
) {
    if (!sem) return 0;

    sense_entry_t* entry = get_sense_entry(sem, word_id, true);
    if (!entry) return 0;

    if (entry->num_senses >= sem->config.max_senses) {
        return 0;  /* Full */
    }

    word_sense_t* sense = &entry->senses[entry->num_senses];
    sense->sense_id = sem->next_sense_id++;
    sense->concept_id = concept_id;
    sense->frequency = frequency;
    sense->activation = 0.0f;
    sense->context_fit = 0.0f;
    sense->related_concepts = NULL;
    sense->num_related = 0;

    if (gloss) {
        strncpy(sense->gloss, gloss, sizeof(sense->gloss) - 1);
        sense->gloss[sizeof(sense->gloss) - 1] = '\0';
    } else {
        sense->gloss[0] = '\0';
    }

    entry->num_senses++;
    return sense->sense_id;
}

/*=============================================================================
 * SEMANTIC INTEGRATION (Core Processing)
 *===========================================================================*/

bool semantic_integrate_word(
    semantic_integrator_t* sem,
    uint32_t word_id,
    const float* word_features,
    uint32_t feature_dim,
    semantic_result_t* result
) {
    if (!sem || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_integrate_word: required parameter is NULL (sem, result)");
        return false;
    }

    uint64_t start_time = 0;  /* TODO: Add timing */
    memset(result, 0, sizeof(semantic_result_t));

    /* Get senses for word */
    sense_entry_t* entry = get_sense_entry(sem, word_id, false);

    if (!entry || entry->num_senses == 0) {
        /* Unknown word - create default sense */
        result->word_recognized = false;
        result->confidence = 0.0f;
        sem->stats.words_processed++;
        return true;
    }

    result->was_ambiguous = (entry->num_senses > 1);
    sem->stats.words_processed++;

    /* Activate all senses (Swinney model - initial access) */
    for (uint32_t i = 0; i < entry->num_senses; i++) {
        entry->senses[i].activation = 1.0f;
        entry->senses[i].context_fit = compute_context_fit(sem, &entry->senses[i]);
    }

    /* Disambiguate based on strategy */
    uint32_t selected_idx = 0;
    float best_score = 0.0f;

    if (entry->num_senses > 1 && sem->config.strategy != DISAMBIG_ALL_ACTIVE) {
        sem->stats.disambiguations++;

        for (uint32_t i = 0; i < entry->num_senses; i++) {
            float score = 0.0f;

            switch (sem->config.strategy) {
                case DISAMBIG_FREQUENCY:
                    score = entry->senses[i].frequency;
                    break;

                case DISAMBIG_CONTEXT:
                    score = entry->senses[i].context_fit;
                    break;

                case DISAMBIG_COMBINED:
                default:
                    score = sem->config.frequency_weight * entry->senses[i].frequency +
                            sem->config.context_weight * entry->senses[i].context_fit;
                    break;
            }

            if (score > best_score) {
                best_score = score;
                selected_idx = i;
            }
        }

        if (best_score >= sem->config.disambiguation_threshold) {
            entry->selected_sense = entry->senses[selected_idx].sense_id;
            entry->selection_confidence = best_score;
            sem->stats.successful_disambig++;
        }
    } else if (entry->num_senses == 1) {
        selected_idx = 0;
        best_score = 1.0f;
        entry->selected_sense = entry->senses[0].sense_id;
        entry->selection_confidence = 1.0f;
    }

    /* Fill result */
    word_sense_t* selected = &entry->senses[selected_idx];
    result->concept_id = selected->concept_id;
    result->sense_id = selected->sense_id;
    result->confidence = best_score;
    result->context_fit = selected->context_fit;

    /* Count active senses above threshold */
    result->num_senses_active = 0;
    for (uint32_t i = 0; i < entry->num_senses; i++) {
        if (entry->senses[i].activation >= sem->config.activation_threshold) {
            result->num_senses_active++;
        }
    }

    /* Add to active concepts */
    add_active_concept(sem, selected->concept_id, best_score,
                       word_id, selected->sense_id);

    /* Spread activation if enabled */
    if (sem->config.enable_spreading_activation && sem->memory) {
        semantic_query_result_t* spread_result =
            semantic_memory_activate(sem->memory, selected->concept_id,
                                    best_score * sem->config.spreading_rate);
        if (spread_result) {
            for (uint32_t i = 0; i < spread_result->count; i++) {
                add_active_concept(sem, spread_result->concept_ids[i],
                                   spread_result->activation_levels[i],
                                   word_id, 0);
            }
            semantic_memory_free_result(spread_result);
            sem->stats.spreading_activations++;
        }
    }

    /* Compute anomaly (N400-like) */
    if (sem->config.enable_anomaly_detection && sem->context.num_words > 0) {
        result->anomaly_score = 1.0f - selected->context_fit;
        result->is_anomalous = (result->anomaly_score >= sem->anomaly_threshold);
        sem->context.anomaly_level =
            0.7f * sem->context.anomaly_level + 0.3f * result->anomaly_score;

        if (result->is_anomalous) {
            sem->stats.anomalies_detected++;
        }
    }

    /* Update context */
    if (sem->context.num_words < sem->context.max_words) {
        context_word_t* ctx_word = &sem->context.words[sem->context.num_words];
        ctx_word->word_id = word_id;
        ctx_word->sense_id = selected->sense_id;
        ctx_word->concept_id = selected->concept_id;
        ctx_word->position = sem->context.num_words;
        ctx_word->role = ROLE_NONE;
        sem->context.num_words++;
    } else {
        /* Shift window */
        memmove(&sem->context.words[0], &sem->context.words[1],
                (sem->context.max_words - 1) * sizeof(context_word_t));
        context_word_t* ctx_word = &sem->context.words[sem->context.max_words - 1];
        ctx_word->word_id = word_id;
        ctx_word->sense_id = selected->sense_id;
        ctx_word->concept_id = selected->concept_id;
        ctx_word->position = sem->context.max_words - 1;
        ctx_word->role = ROLE_NONE;
    }

    /* Update context vector */
    if (word_features && feature_dim > 0) {
        update_context_vector(sem, word_features, feature_dim);
    } else if (sem->memory && selected->concept_id > 0) {
        const semantic_concept_t* concept =
            semantic_memory_get_concept(sem->memory, selected->concept_id);
        if (concept && concept->features) {
            update_context_vector(sem, concept->features, concept->feature_dim);
        }
    }

    /* Update coherence */
    sem->context.coherence_score =
        0.8f * sem->context.coherence_score + 0.2f * selected->context_fit;

    result->processing_time_us = 0;  /* TODO: Add timing */

    return true;
}

bool semantic_integrate_entry(
    semantic_integrator_t* sem,
    const void* entry_ptr,  /* lexical_entry_t* */
    semantic_result_t* result
) {
    if (!sem || !entry_ptr || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_integrate_entry: required parameter is NULL (sem, entry_ptr, result)");
        return false;
    }

    const lexical_entry_t* entry = (const lexical_entry_t*)entry_ptr;

    return semantic_integrate_word(
        sem,
        entry->word_id,
        entry->embedding,
        LEX_DEFAULT_EMBEDDING_DIM,
        result
    );
}

bool semantic_begin_utterance(semantic_integrator_t* sem) {
    if (!sem) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_begin_utterance: sem is NULL");
        return false;
    }

    /* Clear context but keep priming */
    sem->context.num_words = 0;
    sem->context.agent_concept = 0;
    sem->context.patient_concept = 0;
    sem->context.action_concept = 0;
    sem->context.coherence_score = 1.0f;
    sem->context.anomaly_level = 0.0f;

    if (sem->context.context_vector) {
        memset(sem->context.context_vector, 0,
               sem->context.context_dim * sizeof(float));
    }

    /* Clear thematic frame */
    memset(sem->role_fillers, 0, sizeof(sem->role_fillers));
    memset(sem->role_confidences, 0, sizeof(sem->role_confidences));

    sem->in_utterance = true;

    return true;
}

bool semantic_end_utterance(
    semantic_integrator_t* sem,
    float* coherence
) {
    if (!sem) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_end_utterance: sem is NULL");
        return false;
    }

    if (coherence) {
        *coherence = sem->context.coherence_score;
    }

    sem->in_utterance = false;

    /* Decay activations */
    semantic_decay_activations(sem);

    return true;
}

/*=============================================================================
 * DISAMBIGUATION
 *===========================================================================*/

bool semantic_disambiguate(
    semantic_integrator_t* sem,
    uint32_t word_id,
    const float* context_features,
    uint32_t context_dim,
    uint32_t* selected_sense,
    float* confidence
) {
    if (!sem || !selected_sense || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_disambiguate: required parameter is NULL (sem, selected_sense, confidence)");
        return false;
    }

    sense_entry_t* entry = get_sense_entry(sem, word_id, false);
    if (!entry || entry->num_senses == 0) {
        *selected_sense = 0;
        *confidence = 0.0f;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "semantic_disambiguate: entry is NULL");
        return false;
    }

    if (entry->num_senses == 1) {
        *selected_sense = entry->senses[0].sense_id;
        *confidence = 1.0f;
        return true;
    }

    /* Score each sense */
    uint32_t best_idx = 0;
    float best_score = 0.0f;

    for (uint32_t i = 0; i < entry->num_senses; i++) {
        word_sense_t* sense = &entry->senses[i];

        /* Compute context fit */
        float ctx_fit = 0.5f;
        if (context_features && context_dim > 0 && sem->memory) {
            const semantic_concept_t* concept =
                semantic_memory_get_concept(sem->memory, sense->concept_id);
            if (concept && concept->features) {
                ctx_fit = compute_similarity(context_features, concept->features,
                                            (context_dim < concept->feature_dim) ?
                                             context_dim : concept->feature_dim);
            }
        }

        /* Combined score */
        float score = sem->config.frequency_weight * sense->frequency +
                      sem->config.context_weight * ctx_fit;

        if (score > best_score) {
            best_score = score;
            best_idx = i;
        }
    }

    *selected_sense = entry->senses[best_idx].sense_id;
    *confidence = best_score;

    entry->selected_sense = *selected_sense;
    entry->selection_confidence = best_score;

    return true;
}

bool semantic_select_sense(
    semantic_integrator_t* sem,
    uint32_t word_id,
    uint32_t sense_id
) {
    if (!sem) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_select_sense: sem is NULL");
        return false;
    }

    sense_entry_t* entry = get_sense_entry(sem, word_id, false);
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_select_sense: entry is NULL");
        return false;
    }

    for (uint32_t i = 0; i < entry->num_senses; i++) {
        if (entry->senses[i].sense_id == sense_id) {
            entry->selected_sense = sense_id;
            entry->selection_confidence = 1.0f;
            return true;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "semantic_select_sense: validation failed");
    return false;
}

float semantic_get_disambiguation_confidence(
    const semantic_integrator_t* sem,
    uint32_t word_id
) {
    if (!sem) return 0.0f;

    sense_entry_t* entry = get_sense_entry((semantic_integrator_t*)sem,
                                           word_id, false);
    if (!entry) return 0.0f;

    return entry->selection_confidence;
}

/*=============================================================================
 * CONTEXT MANAGEMENT
 *===========================================================================*/

bool semantic_get_context(
    const semantic_integrator_t* sem,
    const sentence_context_t** context
) {
    if (!sem || !context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_get_context: required parameter is NULL (sem, context)");
        return false;
    }
    *context = &sem->context;
    return true;
}

bool semantic_get_context_vector(
    const semantic_integrator_t* sem,
    float* vector,
    uint32_t dim
) {
    if (!sem || !vector || !sem->context.context_vector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_get_context_vector: required parameter is NULL (sem, vector, sem->context)");
        return false;
    }

    uint32_t copy_dim = (dim < sem->context.context_dim) ?
                        dim : sem->context.context_dim;

    memcpy(vector, sem->context.context_vector, copy_dim * sizeof(float));

    /* Zero-pad if needed */
    for (uint32_t i = copy_dim; i < dim; i++) {
        vector[i] = 0.0f;
    }

    return true;
}

bool semantic_update_context(
    semantic_integrator_t* sem,
    const float* features,
    uint32_t dim,
    float weight
) {
    if (!sem || !features || !sem->context.context_vector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_update_context: required parameter is NULL (sem, features, sem->context)");
        return false;
    }

    uint32_t use_dim = (dim < sem->context.context_dim) ?
                       dim : sem->context.context_dim;

    for (uint32_t i = 0; i < use_dim; i++) {
        sem->context.context_vector[i] =
            (1.0f - weight) * sem->context.context_vector[i] +
            weight * features[i];
    }

    return true;
}

bool semantic_decay_context(semantic_integrator_t* sem) {
    if (!sem) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_decay_context: sem is NULL");
        return false;
    }

    /* Decay context vector */
    if (sem->context.context_vector) {
        for (uint32_t i = 0; i < sem->context.context_dim; i++) {
            sem->context.context_vector[i] *= sem->config.context_decay;
        }
    }

    return true;
}

/*=============================================================================
 * THEMATIC ROLE ASSIGNMENT
 *===========================================================================*/

bool semantic_assign_role(
    semantic_integrator_t* sem,
    uint32_t concept_id,
    thematic_role_t role
) {
    if (!sem || role >= ROLE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "semantic_assign_role: sem is NULL");
        return false;
    }

    sem->role_fillers[role] = concept_id;
    sem->role_confidences[role] = 1.0f;

    /* Update context thematic frame */
    if (role == ROLE_AGENT) {
        sem->context.agent_concept = concept_id;
    } else if (role == ROLE_PATIENT) {
        sem->context.patient_concept = concept_id;
    }

    /* Update active concept */
    for (uint32_t i = 0; i < sem->num_active; i++) {
        if (sem->active_concepts[i].concept_id == concept_id) {
            sem->active_concepts[i].role = role;
            break;
        }
    }

    /* Update context word */
    for (uint32_t i = 0; i < sem->context.num_words; i++) {
        if (sem->context.words[i].concept_id == concept_id) {
            sem->context.words[i].role = role;
            break;
        }
    }

    sem->stats.thematic_assignments++;

    return true;
}

thematic_role_t semantic_get_role(
    const semantic_integrator_t* sem,
    uint32_t concept_id
) {
    if (!sem) return ROLE_NONE;

    for (uint32_t i = 0; i < sem->num_active; i++) {
        if (sem->active_concepts[i].concept_id == concept_id) {
            return sem->active_concepts[i].role;
        }
    }

    return ROLE_NONE;
}

uint32_t semantic_get_role_filler(
    const semantic_integrator_t* sem,
    thematic_role_t role
) {
    if (!sem || role >= ROLE_COUNT) return 0;
    return sem->role_fillers[role];
}

const char* semantic_role_name(thematic_role_t role) {
    if (role >= ROLE_COUNT) return "UNKNOWN";
    return ROLE_NAMES[role];
}

/*=============================================================================
 * SPREADING ACTIVATION
 *===========================================================================*/

bool semantic_activate_concept(
    semantic_integrator_t* sem,
    uint32_t concept_id,
    float activation
) {
    if (!sem) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_activate_concept: sem is NULL");
        return false;
    }

    add_active_concept(sem, concept_id, activation, 0, 0);

    /* Spread if connected to semantic memory */
    if (sem->config.enable_spreading_activation && sem->memory) {
        semantic_query_result_t* result =
            semantic_memory_activate(sem->memory, concept_id,
                                    activation * sem->config.spreading_rate);
        if (result) {
            for (uint32_t i = 0; i < result->count; i++) {
                add_active_concept(sem, result->concept_ids[i],
                                   result->activation_levels[i], 0, 0);
            }
            semantic_memory_free_result(result);
            sem->stats.spreading_activations++;
        }
    }

    return true;
}

bool semantic_get_active_concepts(
    const semantic_integrator_t* sem,
    active_concept_t* concepts,
    uint32_t max_concepts,
    uint32_t* num_concepts
) {
    if (!sem || !concepts || !num_concepts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_get_active_concepts: required parameter is NULL (sem, concepts, num_concepts)");
        return false;
    }

    uint32_t copy_count = (sem->num_active < max_concepts) ?
                          sem->num_active : max_concepts;

    memcpy(concepts, sem->active_concepts, copy_count * sizeof(active_concept_t));
    *num_concepts = copy_count;

    return true;
}

float semantic_get_activation(
    const semantic_integrator_t* sem,
    uint32_t concept_id
) {
    if (!sem) return 0.0f;

    for (uint32_t i = 0; i < sem->num_active; i++) {
        if (sem->active_concepts[i].concept_id == concept_id) {
            return sem->active_concepts[i].activation;
        }
    }

    return 0.0f;
}

bool semantic_decay_activations(semantic_integrator_t* sem) {
    if (!sem) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_decay_activations: sem is NULL");
        return false;
    }

    uint32_t write_idx = 0;

    for (uint32_t i = 0; i < sem->num_active; i++) {
        sem->active_concepts[i].activation *= sem->config.activation_decay;

        if (sem->active_concepts[i].activation >= sem->config.activation_threshold) {
            if (write_idx != i) {
                sem->active_concepts[write_idx] = sem->active_concepts[i];
            }
            write_idx++;
        }
    }

    sem->num_active = write_idx;

    return true;
}

/*=============================================================================
 * SEMANTIC PRIMING
 *===========================================================================*/

bool semantic_prime_concept(
    semantic_integrator_t* sem,
    uint32_t concept_id,
    float strength
) {
    if (!sem) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_prime_concept: sem is NULL");
        return false;
    }

    /* Check if already primed */
    for (uint32_t i = 0; i < sem->priming.num_primed; i++) {
        if (sem->priming.primed_concepts[i] == concept_id) {
            /* Update strength */
            if (strength > sem->priming.priming_levels[i]) {
                sem->priming.priming_levels[i] = strength;
            }
            return true;
        }
    }

    /* Add new */
    if (sem->priming.num_primed >= sem->priming.max_primed) {
        /* Replace weakest */
        uint32_t min_idx = 0;
        float min_level = sem->priming.priming_levels[0];
        for (uint32_t i = 1; i < sem->priming.num_primed; i++) {
            if (sem->priming.priming_levels[i] < min_level) {
                min_level = sem->priming.priming_levels[i];
                min_idx = i;
            }
        }
        if (strength > min_level) {
            sem->priming.primed_concepts[min_idx] = concept_id;
            sem->priming.priming_levels[min_idx] = strength;
        }
        return true;
    }

    sem->priming.primed_concepts[sem->priming.num_primed] = concept_id;
    sem->priming.priming_levels[sem->priming.num_primed] = strength;
    sem->priming.num_primed++;

    return true;
}

float semantic_is_primed(
    const semantic_integrator_t* sem,
    uint32_t concept_id
) {
    if (!sem) return 0.0f;

    for (uint32_t i = 0; i < sem->priming.num_primed; i++) {
        if (sem->priming.primed_concepts[i] == concept_id) {
            return sem->priming.priming_levels[i];
        }
    }

    return 0.0f;
}

bool semantic_decay_priming(semantic_integrator_t* sem) {
    if (!sem) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_decay_priming: sem is NULL");
        return false;
    }

    uint32_t write_idx = 0;

    for (uint32_t i = 0; i < sem->priming.num_primed; i++) {
        sem->priming.priming_levels[i] *= sem->config.activation_decay;

        if (sem->priming.priming_levels[i] >= 0.1f) {
            sem->priming.primed_concepts[write_idx] =
                sem->priming.primed_concepts[i];
            sem->priming.priming_levels[write_idx] =
                sem->priming.priming_levels[i];
            write_idx++;
        }
    }

    sem->priming.num_primed = write_idx;

    return true;
}

void semantic_clear_priming(semantic_integrator_t* sem) {
    if (!sem) return;
    sem->priming.num_primed = 0;
}

/*=============================================================================
 * ANOMALY DETECTION (N400-like)
 *===========================================================================*/

bool semantic_compute_anomaly(
    semantic_integrator_t* sem,
    uint32_t word_id,
    uint32_t concept_id,
    float* anomaly_score
) {
    if (!sem || !anomaly_score) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_compute_anomaly: required parameter is NULL (sem, anomaly_score)");
        return false;
    }

    /* Anomaly = 1 - context_fit */
    float context_fit = 0.5f;

    if (sem->memory && sem->context.context_vector) {
        const semantic_concept_t* concept =
            semantic_memory_get_concept(sem->memory, concept_id);
        if (concept && concept->features) {
            context_fit = compute_similarity(
                sem->context.context_vector,
                concept->features,
                (sem->context.context_dim < concept->feature_dim) ?
                 sem->context.context_dim : concept->feature_dim
            );
        }
    }

    *anomaly_score = 1.0f - context_fit;
    return true;
}

float semantic_get_coherence(const semantic_integrator_t* sem) {
    if (!sem) return 0.0f;
    return sem->context.coherence_score;
}

void semantic_set_anomaly_threshold(
    semantic_integrator_t* sem,
    float threshold
) {
    if (!sem) return;
    sem->anomaly_threshold = threshold;
}

/*=============================================================================
 * INFERENCE
 *===========================================================================*/

bool semantic_generate_inferences(
    semantic_integrator_t* sem,
    uint32_t* inferred_concepts,
    uint32_t max_inferences,
    uint32_t* num_inferred
) {
    if (!sem || !inferred_concepts || !num_inferred) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_generate_inferences: required parameter is NULL (sem, inferred_concepts, num_inferred)");
        return false;
    }

    *num_inferred = 0;

    if (!sem->config.enable_inference || !sem->memory) {
        return true;  /* No error, just no inference */
    }

    /* Simple inference: find concepts related to all active concepts */
    /* TODO: Implement more sophisticated inference rules */

    return true;
}

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

bool semantic_get_stats(
    const semantic_integrator_t* sem,
    semantic_stats_t* stats
) {
    if (!sem || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_get_stats: required parameter is NULL (sem, stats)");
        return false;
    }
    *stats = sem->stats;

    /* Compute averages */
    if (sem->stats.words_processed > 0) {
        stats->avg_sense_count =
            (float)sem->sense_count / (float)sem->stats.words_processed;
    }

    return true;
}

void semantic_reset_stats(semantic_integrator_t* sem) {
    if (!sem) return;
    memset(&sem->stats, 0, sizeof(semantic_stats_t));
}

/*=============================================================================
 * UTILITY
 *===========================================================================*/

void semantic_free_result(semantic_result_t* result) {
    if (!result) return;

    if (result->active_senses) {
        nimcp_free(result->active_senses);
        result->active_senses = NULL;
    }
}

bool semantic_get_config(
    const semantic_integrator_t* sem,
    semantic_config_t* config
) {
    if (!sem || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_get_config: required parameter is NULL (sem, config)");
        return false;
    }
    *config = sem->config;
    return true;
}
