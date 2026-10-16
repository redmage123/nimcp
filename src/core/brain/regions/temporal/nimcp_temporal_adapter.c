/**
 * @file nimcp_temporal_adapter.c
 * @brief Implementation of temporal cortex brain adapter
 *
 * WHAT: Unified adapter connecting temporal cortex sub-modules to the brain system
 * WHY:  Enable seamless integration of auditory, object recognition, and semantic memory
 * HOW:  Orchestrates auditory, object, and semantic processors
 *
 * @version Phase T1: Temporal Cortex Brain Integration
 * @date 2025-12-30
 */

#include "core/brain/regions/temporal/nimcp_temporal_adapter.h"
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(temporal_adapter, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define TEMPORAL_LOG_MODULE "TEMPORAL"

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Concept entry for internal storage
 */
typedef struct concept_node {
    temporal_concept_t concept;
    struct concept_node* next;           /**< Hash collision chain */
} concept_node_t;

/**
 * @brief Object prototype entry
 */
typedef struct prototype_node {
    uint32_t object_id;
    char name[NIMCP_ID_BUFFER_SIZE];
    float* features;
    uint32_t feature_dim;
    struct prototype_node* next;
} prototype_node_t;

/**
 * @brief Working memory slot
 */
typedef struct {
    uint32_t concept_id;
    float activation;                    /**< Decay-based activation */
    double timestamp;                    /**< When added */
} temporal_wm_slot_t;

/**
 * @brief Internal auditory processor (placeholder)
 */
struct auditory_processor {
    float* spectral_buffer;              /**< Current spectral representation */
    uint32_t num_bands;                  /**< Number of frequency bands */
    uint32_t fft_size;                   /**< FFT window size */
    uint32_t sample_rate;                /**< Sample rate */
    float last_f0;                       /**< Last detected F0 */
    bool last_is_speech;                 /**< Last speech detection */
};

/**
 * @brief Internal object recognition (placeholder)
 */
struct object_recognition {
    prototype_node_t** prototypes;       /**< Prototype hash table */
    uint32_t prototype_capacity;
    uint32_t prototype_count;
    uint32_t feature_dim;
};

/**
 * @brief Internal semantic memory (placeholder)
 */
struct semantic_memory_core {
    concept_node_t** concepts;           /**< Concept hash table */
    uint32_t concept_capacity;
    uint32_t concept_count;
    uint32_t embedding_dim;
    bool spreading_enabled;
    bool priming_enabled;
};

/**
 * @brief Internal adapter structure
 */
struct temporal_adapter {
    /* Configuration */
    temporal_config_t config;

    /* Sub-modules */
    auditory_processor_t* auditory;
    object_recognition_t* object;
    semantic_memory_core_t* semantic;

    /* Working memory */
    temporal_wm_slot_t* working_memory;
    uint32_t wm_count;
    uint32_t wm_head;                    /**< Next insert position */

    /* Callbacks */
    temporal_auditory_callback_t auditory_callback;
    void* auditory_user_data;
    temporal_recognition_callback_t recognition_callback;
    void* recognition_user_data;
    temporal_semantic_callback_t semantic_callback;
    void* semantic_user_data;
    temporal_event_callback_t event_callback;
    void* event_user_data;

    /* State */
    temporal_status_t status;
    temporal_error_t last_error;
    double current_time_ms;

    /* Bio-async communication context */
    bio_module_context_t bio_ctx;
    nimcp_bio_channel_type_t default_channel;

    /* Statistics */
    temporal_stats_t stats;
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Simple string hash function
 */
static uint32_t hash_string(const char* str, uint32_t capacity) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + (uint32_t)c;
    }
    return hash % capacity;
}

/**
 * @brief Hash concept ID
 */
static uint32_t hash_id(uint32_t id, uint32_t capacity) {
    return id % capacity;
}

/**
 * @brief Set error state
 */
static void set_error(temporal_adapter_t* adapter, temporal_error_t error) {
    if (!adapter) return;
    adapter->last_error = error;
    if (error != TEMPORAL_ERROR_NONE) {
        adapter->status = TEMPORAL_STATUS_ERROR;
        LOG_ERROR("[%s] Error set: %d", TEMPORAL_LOG_MODULE, error);
    }
}

/**
 * @brief Emit event to callback
 */
static void emit_event(temporal_adapter_t* adapter, uint32_t event_type, const void* data) {
    if (adapter->config.enable_events && adapter->event_callback) {
        adapter->event_callback(event_type, data, adapter->event_user_data);
    }
}

/**
 * @brief Create auditory processor
 */
static auditory_processor_t* create_auditory_processor(const temporal_config_t* config) {
    auditory_processor_t* proc = (auditory_processor_t*)nimcp_calloc(1, sizeof(auditory_processor_t));
    if (!proc) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "proc is NULL");

        return NULL;

    }

    proc->num_bands = config->tonotopic_bands;
    proc->fft_size = config->fft_size;
    proc->sample_rate = config->sample_rate;

    proc->spectral_buffer = (float*)nimcp_calloc(proc->num_bands, sizeof(float));
    if (!proc->spectral_buffer) {
        nimcp_free(proc);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_auditory_processor: proc->spectral_buffer is NULL");
        return NULL;
    }

    return proc;
}

/**
 * @brief Destroy auditory processor
 */
static void destroy_auditory_processor(auditory_processor_t* proc) {
    if (!proc) return;
    if (proc->spectral_buffer) nimcp_free(proc->spectral_buffer);
    nimcp_free(proc);
}

/**
 * @brief Create object recognition
 */
static object_recognition_t* create_object_recognition(const temporal_config_t* config) {
    object_recognition_t* obj = (object_recognition_t*)nimcp_calloc(1, sizeof(object_recognition_t));
    if (!obj) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "obj is NULL");

        return NULL;

    }

    obj->prototype_capacity = config->max_objects;
    obj->feature_dim = config->feature_dim;

    obj->prototypes = (prototype_node_t**)nimcp_calloc(obj->prototype_capacity, sizeof(prototype_node_t*));
    if (!obj->prototypes) {
        nimcp_free(obj);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_object_recognition: obj->prototypes is NULL");
        return NULL;
    }

    return obj;
}

/**
 * @brief Destroy object recognition
 */
static void destroy_object_recognition(object_recognition_t* obj) {
    if (!obj) return;

    if (obj->prototypes) {
        for (uint32_t i = 0; i < obj->prototype_capacity; i++) {
            prototype_node_t* node = obj->prototypes[i];
            while (node) {
                prototype_node_t* next = node->next;
                if (node->features) nimcp_free(node->features);
                nimcp_free(node);
                node = next;
            }
        }
        nimcp_free(obj->prototypes);
    }
    nimcp_free(obj);
}

/**
 * @brief Create semantic memory core
 */
static semantic_memory_core_t* create_semantic_memory(const temporal_config_t* config) {
    semantic_memory_core_t* sem = (semantic_memory_core_t*)nimcp_calloc(1, sizeof(semantic_memory_core_t));
    if (!sem) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sem is NULL");

        return NULL;

    }

    sem->concept_capacity = config->max_concepts;
    sem->embedding_dim = config->concept_dim;
    sem->spreading_enabled = config->enable_spreading_activation;
    sem->priming_enabled = config->enable_priming;

    sem->concepts = (concept_node_t**)nimcp_calloc(sem->concept_capacity, sizeof(concept_node_t*));
    if (!sem->concepts) {
        nimcp_free(sem);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_semantic_memory: sem->concepts is NULL");
        return NULL;
    }

    return sem;
}

/**
 * @brief Destroy semantic memory
 */
static void destroy_semantic_memory(semantic_memory_core_t* sem) {
    if (!sem) return;

    if (sem->concepts) {
        for (uint32_t i = 0; i < sem->concept_capacity; i++) {
            concept_node_t* node = sem->concepts[i];
            while (node) {
                concept_node_t* next = node->next;
                if (node->concept.embedding) nimcp_free(node->concept.embedding);
                if (node->concept.related_concepts) nimcp_free(node->concept.related_concepts);
                nimcp_free(node);
                node = next;
            }
        }
        nimcp_free(sem->concepts);
    }
    nimcp_free(sem);
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

temporal_config_t temporal_default_config(void) {
    temporal_config_t config;
    memset(&config, 0, sizeof(config));

    config.max_audio_frames = TEMPORAL_DEFAULT_MAX_AUDIO_FRAMES;
    config.max_objects = TEMPORAL_DEFAULT_MAX_OBJECTS;
    config.max_concepts = TEMPORAL_DEFAULT_MAX_CONCEPTS;
    config.working_memory_slots = TEMPORAL_DEFAULT_WORKING_MEMORY_SLOTS;
    config.enable_working_memory = true;
    config.sample_rate = 44100;
    config.fft_size = TEMPORAL_DEFAULT_FFT_SIZE;
    config.tonotopic_bands = TEMPORAL_DEFAULT_TONOTOPIC_BANDS;
    config.enable_spectral_analysis = true;
    config.enable_pitch_tracking = true;
    config.enable_rhythm_analysis = true;
    config.feature_dim = 256;
    config.enable_face_recognition = true;
    config.enable_invariant_recognition = true;
    config.concept_dim = TEMPORAL_DEFAULT_CONCEPT_DIM;
    config.enable_spreading_activation = true;
    config.enable_priming = true;
    config.enable_events = true;
    config.enable_training = false;
    config.learning_rate = NIMCP_LEARNING_RATE_DEFAULT;
    config.processing_window_ms = TEMPORAL_DEFAULT_PROCESSING_WINDOW_MS;
    config.enable_bio_async = true;
    config.default_channel = BIO_CHANNEL_ACETYLCHOLINE;

    return config;
}

temporal_adapter_t* temporal_create(const temporal_config_t* config) {
    temporal_config_t cfg = config ? *config : temporal_default_config();

    temporal_adapter_t* adapter = (temporal_adapter_t*)nimcp_calloc(1, sizeof(temporal_adapter_t));
    if (!adapter) {
        LOG_ERROR("[%s] Failed to allocate adapter", TEMPORAL_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "temporal_create: adapter is NULL");
        return NULL;
    }

    adapter->config = cfg;

    /* Create sub-modules */
    adapter->auditory = create_auditory_processor(&cfg);
    if (!adapter->auditory) {
        LOG_ERROR("[%s] Failed to create auditory processor", TEMPORAL_LOG_MODULE);
        temporal_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "temporal_create: adapter->auditory is NULL");
        return NULL;
    }

    adapter->object = create_object_recognition(&cfg);
    if (!adapter->object) {
        LOG_ERROR("[%s] Failed to create object recognition", TEMPORAL_LOG_MODULE);
        temporal_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "temporal_create: adapter->object is NULL");
        return NULL;
    }

    adapter->semantic = create_semantic_memory(&cfg);
    if (!adapter->semantic) {
        LOG_ERROR("[%s] Failed to create semantic memory", TEMPORAL_LOG_MODULE);
        temporal_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "temporal_create: adapter->semantic is NULL");
        return NULL;
    }

    /* Create working memory */
    if (cfg.enable_working_memory) {
        adapter->working_memory = (temporal_wm_slot_t*)nimcp_calloc(
            cfg.working_memory_slots, sizeof(temporal_wm_slot_t));
        if (!adapter->working_memory) {
            LOG_ERROR("[%s] Failed to create working memory", TEMPORAL_LOG_MODULE);
            temporal_destroy(adapter);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "temporal_create: adapter->working_memory is NULL");
            return NULL;
        }
    }

    adapter->status = TEMPORAL_STATUS_IDLE;
    adapter->last_error = TEMPORAL_ERROR_NONE;
    adapter->default_channel = cfg.default_channel;

    LOG_INFO("[%s] Temporal adapter created successfully", TEMPORAL_LOG_MODULE);
    return adapter;
}

void temporal_destroy(temporal_adapter_t* adapter) {
    if (!adapter) return;

    destroy_auditory_processor(adapter->auditory);
    destroy_object_recognition(adapter->object);
    destroy_semantic_memory(adapter->semantic);

    if (adapter->working_memory) {
        nimcp_free(adapter->working_memory);
    }

    nimcp_free(adapter);
    LOG_DEBUG("[%s] Temporal adapter destroyed", TEMPORAL_LOG_MODULE);
}

bool temporal_reset(temporal_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_reset: adapter is NULL");
        return false;
    }

    /* Reset auditory state */
    if (adapter->auditory && adapter->auditory->spectral_buffer) {
        memset(adapter->auditory->spectral_buffer, 0,
               adapter->auditory->num_bands * sizeof(float));
        adapter->auditory->last_f0 = 0.0f;
        adapter->auditory->last_is_speech = false;
    }

    /* Reset working memory */
    if (adapter->working_memory) {
        memset(adapter->working_memory, 0,
               adapter->config.working_memory_slots * sizeof(temporal_wm_slot_t));
        adapter->wm_count = 0;
        adapter->wm_head = 0;
    }

    adapter->status = TEMPORAL_STATUS_IDLE;
    adapter->last_error = TEMPORAL_ERROR_NONE;

    return true;
}

/*=============================================================================
 * AUDITORY PROCESSING
 *===========================================================================*/

bool temporal_process_audio(
    temporal_adapter_t* adapter,
    const temporal_audio_frame_t* frame,
    temporal_auditory_result_t* result
) {
    if (!adapter || !frame) {
        if (adapter) set_error(adapter, TEMPORAL_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_process_audio: validation failed");
        return false;
    }

    adapter->status = TEMPORAL_STATUS_AUDITORY_PROCESSING;

    /* Simplified spectral analysis (placeholder for FFT) */
    if (adapter->auditory && frame->samples && frame->num_samples > 0) {
        /* Compute simple power in frequency bands */
        uint32_t samples_per_band = frame->num_samples / adapter->auditory->num_bands;
        if (samples_per_band == 0) samples_per_band = 1;

        for (uint32_t b = 0; b < adapter->auditory->num_bands; b++) {
            float power = 0.0f;
            uint32_t start = b * samples_per_band;
            uint32_t end = (b + 1) * samples_per_band;
            if (end > frame->num_samples) end = frame->num_samples;

            for (uint32_t i = start; i < end; i++) {
                power += frame->samples[i] * frame->samples[i];
            }
            adapter->auditory->spectral_buffer[b] = sqrtf(power / (float)(end - start));
        }

        /* Simple speech detection based on spectral energy */
        float total_energy = 0.0f;
        for (uint32_t b = 0; b < adapter->auditory->num_bands; b++) {
            total_energy += adapter->auditory->spectral_buffer[b];
        }
        adapter->auditory->last_is_speech = (total_energy > 0.1f);
    }

    /* Fill result if requested */
    if (result) {
        memset(result, 0, sizeof(temporal_auditory_result_t));
        if (adapter->auditory) {
            result->spectral_power = adapter->auditory->spectral_buffer;
            result->num_bands = adapter->auditory->num_bands;
            result->fundamental_freq = adapter->auditory->last_f0;
            result->is_speech = adapter->auditory->last_is_speech;
            result->timestamp_ms = frame->timestamp_ms;

            /* Compute loudness from spectral power */
            float loudness = 0.0f;
            for (uint32_t b = 0; b < adapter->auditory->num_bands; b++) {
                loudness += adapter->auditory->spectral_buffer[b];
            }
            result->loudness = fminf(1.0f, loudness / (float)adapter->auditory->num_bands);
        }
    }

    adapter->stats.audio_frames_processed++;
    adapter->status = TEMPORAL_STATUS_READY;

    /* Emit callback */
    if (adapter->auditory_callback && result) {
        adapter->auditory_callback(result, adapter->auditory_user_data);
    }

    return true;
}

uint32_t temporal_get_spectral_state(
    const temporal_adapter_t* adapter,
    float* spectral_power,
    uint32_t buffer_size
) {
    if (!adapter || !spectral_power) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_get_spectral_state: required parameter is NULL");
        return 0;
    }
    if (!adapter->auditory) return 0;

    uint32_t copy_count = buffer_size < adapter->auditory->num_bands ?
                          buffer_size : adapter->auditory->num_bands;

    memcpy(spectral_power, adapter->auditory->spectral_buffer, copy_count * sizeof(float));
    return copy_count;
}

bool temporal_detect_speech(temporal_adapter_t* adapter, float* confidence) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_detect_speech: adapter is NULL");
        return false;
    }
    if (!adapter->auditory) return false;

    if (confidence) {
        *confidence = adapter->auditory->last_is_speech ? 0.8f : 0.2f;
    }

    return adapter->auditory->last_is_speech;
}

/*=============================================================================
 * OBJECT RECOGNITION
 *===========================================================================*/

bool temporal_recognize_object(
    temporal_adapter_t* adapter,
    const temporal_visual_input_t* input,
    temporal_recognition_result_t* result
) {
    if (!adapter || !input || !result) {
        if (adapter) set_error(adapter, TEMPORAL_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_recognize_object: validation failed");
        return false;
    }

    adapter->status = TEMPORAL_STATUS_OBJECT_RECOGNITION;
    memset(result, 0, sizeof(temporal_recognition_result_t));

    /* Search prototypes for best match */
    float best_score = -1.0f;
    prototype_node_t* best_match = NULL;

    if (adapter->object && adapter->object->prototypes) {
        for (uint32_t i = 0; i < adapter->object->prototype_capacity; i++) {
            prototype_node_t* node = adapter->object->prototypes[i];
            while (node) {
                /* Compute cosine similarity */
                float dot = 0.0f;
                float norm_a = 0.0f;
                float norm_b = 0.0f;

                uint32_t dim = input->feature_dim < node->feature_dim ?
                               input->feature_dim : node->feature_dim;

                for (uint32_t d = 0; d < dim; d++) {
                    dot += input->features[d] * node->features[d];
                    norm_a += input->features[d] * input->features[d];
                    norm_b += node->features[d] * node->features[d];
                }

                float similarity = (norm_a > 0 && norm_b > 0) ?
                                   dot / (sqrtf(norm_a) * sqrtf(norm_b)) : 0.0f;

                if (similarity > best_score) {
                    best_score = similarity;
                    best_match = node;
                }

                node = node->next;
            }
        }
    }

    if (best_match && best_score > 0.5f) {
        result->object_id = best_match->object_id;
        strncpy(result->object_name, best_match->name, sizeof(result->object_name) - 1);
        result->confidence = best_score;
        result->viewpoint_invariance = 0.8f; /* Placeholder */
        adapter->stats.successful_recognitions++;
        adapter->stats.objects_recognized++;
    } else {
        result->confidence = 0.0f;
        adapter->stats.recognition_errors++;
    }

    adapter->status = TEMPORAL_STATUS_READY;

    /* Emit callback */
    if (adapter->recognition_callback) {
        adapter->recognition_callback(result, adapter->recognition_user_data);
    }

    return (result->confidence > 0.5f);
}

bool temporal_add_object_prototype(
    temporal_adapter_t* adapter,
    uint32_t object_id,
    const char* name,
    const float* features,
    uint32_t feature_dim
) {
    if (!adapter || !name || !features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_add_object_prototype: required parameter is NULL");
        return false;
    }
    if (feature_dim == 0) return false;
    if (!adapter->object) return false;

    uint32_t idx = hash_id(object_id, adapter->object->prototype_capacity);

    /* Check if already exists */
    prototype_node_t* node = adapter->object->prototypes[idx];
    while (node) {
        if (node->object_id == object_id) {
            /* Update existing */
            if (node->feature_dim == feature_dim) {
                memcpy(node->features, features, feature_dim * sizeof(float));
                return true;
            }
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_add_object_prototype: validation failed");
            return false; /* Dimension mismatch */
        }
        node = node->next;
    }

    /* Create new node */
    prototype_node_t* new_node = (prototype_node_t*)nimcp_calloc(1, sizeof(prototype_node_t));
    if (!new_node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "temporal_add_object_prototype: new_node is NULL");
        return false;
    }

    new_node->object_id = object_id;
    strncpy(new_node->name, name, sizeof(new_node->name) - 1);
    new_node->feature_dim = feature_dim;
    new_node->features = (float*)nimcp_malloc(feature_dim * sizeof(float));
    if (!new_node->features) {
        nimcp_free(new_node);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "temporal_add_object_prototype: new_node->features is NULL");
        return false;
    }
    memcpy(new_node->features, features, feature_dim * sizeof(float));

    /* Insert at head of chain */
    new_node->next = adapter->object->prototypes[idx];
    adapter->object->prototypes[idx] = new_node;
    adapter->object->prototype_count++;

    return true;
}

bool temporal_recognize_face(
    temporal_adapter_t* adapter,
    const float* features,
    uint32_t feature_dim,
    uint32_t* face_id,
    float* confidence
) {
    if (!adapter || !features || !face_id || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_recognize_face: required parameter is NULL");
        return false;
    }

    /* Use object recognition with face flag */
    temporal_visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.features = (float*)features;
    input.feature_dim = feature_dim;

    temporal_recognition_result_t result;
    bool success = temporal_recognize_object(adapter, &input, &result);

    *face_id = result.face_id;
    *confidence = result.confidence;

    return success && result.is_face;
}

/*=============================================================================
 * SEMANTIC MEMORY
 *===========================================================================*/

bool temporal_add_concept(
    temporal_adapter_t* adapter,
    const temporal_concept_t* concept_entry
) {
    if (!adapter || !concept_entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_add_concept: required parameter is NULL");
        return false;
    }
    if (!adapter->semantic) return false;

    uint32_t idx = hash_id(concept_entry->concept_id, adapter->semantic->concept_capacity);

    /* Check if already exists */
    concept_node_t* node = adapter->semantic->concepts[idx];
    while (node) {
        if (node->concept.concept_id == concept_entry->concept_id) {
            /* Update existing */
            memcpy(&node->concept, concept_entry, sizeof(temporal_concept_t));
            return true;
        }
        node = node->next;
    }

    /* Create new node */
    concept_node_t* new_node = (concept_node_t*)nimcp_calloc(1, sizeof(concept_node_t));
    if (!new_node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "temporal_add_concept: new_node is NULL");
        return false;
    }

    memcpy(&new_node->concept, concept_entry, sizeof(temporal_concept_t));

    /* Deep copy embedding if present */
    if (concept_entry->embedding && concept_entry->embedding_dim > 0) {
        new_node->concept.embedding = (float*)nimcp_malloc(concept_entry->embedding_dim * sizeof(float));
        if (!new_node->concept.embedding) {
            nimcp_free(new_node);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "temporal_add_concept: new_node->concept is NULL");
            return false;
        }
        memcpy(new_node->concept.embedding, concept_entry->embedding,
               concept_entry->embedding_dim * sizeof(float));
    }

    /* Deep copy related concepts if present */
    if (concept_entry->related_concepts && concept_entry->num_related > 0) {
        new_node->concept.related_concepts = (uint32_t*)nimcp_malloc(
            concept_entry->num_related * sizeof(uint32_t));
        if (!new_node->concept.related_concepts) {
            if (new_node->concept.embedding) nimcp_free(new_node->concept.embedding);
            nimcp_free(new_node);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_add_concept: validation failed");
            return false;
        }
        memcpy(new_node->concept.related_concepts, concept_entry->related_concepts,
               concept_entry->num_related * sizeof(uint32_t));
    }

    /* Insert at head of chain */
    new_node->next = adapter->semantic->concepts[idx];
    adapter->semantic->concepts[idx] = new_node;
    adapter->semantic->concept_count++;

    return true;
}

bool temporal_get_concept(
    const temporal_adapter_t* adapter,
    uint32_t concept_id,
    temporal_concept_t* concept_out
) {
    if (!adapter || !concept_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_get_concept: required parameter is NULL");
        return false;
    }
    if (!adapter->semantic) return false;

    uint32_t idx = hash_id(concept_id, adapter->semantic->concept_capacity);

    concept_node_t* node = adapter->semantic->concepts[idx];
    while (node) {
        if (node->concept.concept_id == concept_id) {
            memcpy(concept_out, &node->concept, sizeof(temporal_concept_t));
            return true;
        }
        node = node->next;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_get_concept: validation failed");
    return false;
}

uint32_t temporal_search_concepts(
    temporal_adapter_t* adapter,
    const char* query,
    temporal_concept_t* results,
    uint32_t max_results
) {
    if (!adapter || !query || !results) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_search_concepts: required parameter is NULL");
        return 0;
    }
    if (max_results == 0) return 0;
    if (!adapter->semantic) return 0;

    adapter->status = TEMPORAL_STATUS_SEMANTIC_RETRIEVAL;

    uint32_t found = 0;

    for (uint32_t i = 0; i < adapter->semantic->concept_capacity && found < max_results; i++) {
        concept_node_t* node = adapter->semantic->concepts[i];
        while (node && found < max_results) {
            /* Simple substring match */
            if (strstr(node->concept.name, query) != NULL) {
                memcpy(&results[found], &node->concept, sizeof(temporal_concept_t));
                found++;
            }
            node = node->next;
        }
    }

    adapter->stats.semantic_queries++;
    adapter->stats.concepts_retrieved += found;
    adapter->status = TEMPORAL_STATUS_READY;

    return found;
}

bool temporal_get_related(
    temporal_adapter_t* adapter,
    uint32_t concept_id,
    temporal_semantic_result_t* result,
    uint32_t max_depth
) {
    if (!adapter || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_get_related: required parameter is NULL");
        return false;
    }
    if (!adapter->semantic) return false;
    (void)max_depth; /* TODO: Implement spreading activation */

    memset(result, 0, sizeof(temporal_semantic_result_t));

    /* Get the source concept */
    temporal_concept_t source;
    if (!temporal_get_concept(adapter, concept_id, &source)) {
        set_error(adapter, TEMPORAL_ERROR_CONCEPT_NOT_FOUND);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_get_related: temporal_get_concept is NULL");
        return false;
    }

    /* Placeholder: return related concepts without full spreading */
    result->num_concepts = source.num_related;
    result->spreading_depth = 1.0f;
    result->total_activation = source.activation;
    result->priming_active = adapter->semantic->priming_enabled;

    return true;
}

bool temporal_apply_priming(
    temporal_adapter_t* adapter,
    uint32_t concept_id,
    float strength
) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_apply_priming: adapter is NULL");
        return false;
    }
    if (!adapter->semantic) return false;
    if (!adapter->semantic->priming_enabled) return false;

    uint32_t idx = hash_id(concept_id, adapter->semantic->concept_capacity);

    concept_node_t* node = adapter->semantic->concepts[idx];
    while (node) {
        if (node->concept.concept_id == concept_id) {
            /* Boost activation */
            node->concept.activation += strength;
            if (node->concept.activation > 1.0f) {
                node->concept.activation = 1.0f;
            }

            /* Spread to related concepts */
            for (uint32_t r = 0; r < node->concept.num_related; r++) {
                uint32_t rel_idx = hash_id(node->concept.related_concepts[r],
                                           adapter->semantic->concept_capacity);
                concept_node_t* rel_node = adapter->semantic->concepts[rel_idx];
                while (rel_node) {
                    if (rel_node->concept.concept_id == node->concept.related_concepts[r]) {
                        rel_node->concept.activation += strength * 0.5f;
                        if (rel_node->concept.activation > 1.0f) {
                            rel_node->concept.activation = 1.0f;
                        }
                        break;
                    }
                    rel_node = rel_node->next;
                }
            }
            return true;
        }
        node = node->next;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_apply_priming: operation failed");
    return false;
}

/*=============================================================================
 * WORKING MEMORY
 *===========================================================================*/

bool temporal_wm_push(temporal_adapter_t* adapter, uint32_t concept_id) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_wm_push: adapter is NULL");
        return false;
    }
    if (!adapter->working_memory) return false;

    if (adapter->wm_count >= adapter->config.working_memory_slots) {
        set_error(adapter, TEMPORAL_ERROR_WORKING_MEMORY_FULL);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "temporal_wm_push: capacity exceeded");
        return false;
    }

    adapter->working_memory[adapter->wm_head].concept_id = concept_id;
    adapter->working_memory[adapter->wm_head].activation = 1.0f;
    adapter->working_memory[adapter->wm_head].timestamp = adapter->current_time_ms;

    adapter->wm_head = (adapter->wm_head + 1) % adapter->config.working_memory_slots;
    adapter->wm_count++;

    return true;
}

bool temporal_wm_pop(temporal_adapter_t* adapter, uint32_t* concept_id) {
    if (!adapter || !concept_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_wm_pop: required parameter is NULL");
        return false;
    }
    if (!adapter->working_memory) return false;
    if (adapter->wm_count == 0) return false;

    uint32_t tail = (adapter->wm_head + adapter->config.working_memory_slots - adapter->wm_count)
                    % adapter->config.working_memory_slots;

    *concept_id = adapter->working_memory[tail].concept_id;
    adapter->wm_count--;

    return true;
}

bool temporal_wm_get_contents(
    const temporal_adapter_t* adapter,
    uint32_t* concept_ids,
    uint32_t* count
) {
    if (!adapter || !concept_ids || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_wm_get_contents: required parameter is NULL");
        return false;
    }
    if (!adapter->working_memory) {
        *count = 0;
        return true;
    }

    uint32_t copy_count = *count < adapter->wm_count ? *count : adapter->wm_count;

    for (uint32_t i = 0; i < copy_count; i++) {
        uint32_t idx = (adapter->wm_head + adapter->config.working_memory_slots - adapter->wm_count + i)
                       % adapter->config.working_memory_slots;
        concept_ids[i] = adapter->working_memory[idx].concept_id;
    }

    *count = copy_count;
    return true;
}

/*=============================================================================
 * EVENT INTEGRATION
 *===========================================================================*/

bool temporal_set_event_callback(
    temporal_adapter_t* adapter,
    temporal_event_callback_t callback,
    void* user_data
) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_set_event_callback: adapter is NULL");
        return false;
    }
    adapter->event_callback = callback;
    adapter->event_user_data = user_data;
    return true;
}

bool temporal_set_auditory_callback(
    temporal_adapter_t* adapter,
    temporal_auditory_callback_t callback,
    void* user_data
) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_set_auditory_callback: adapter is NULL");
        return false;
    }
    adapter->auditory_callback = callback;
    adapter->auditory_user_data = user_data;
    return true;
}

bool temporal_set_recognition_callback(
    temporal_adapter_t* adapter,
    temporal_recognition_callback_t callback,
    void* user_data
) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_set_recognition_callback: adapter is NULL");
        return false;
    }
    adapter->recognition_callback = callback;
    adapter->recognition_user_data = user_data;
    return true;
}

/*=============================================================================
 * TRAINING
 *===========================================================================*/

bool temporal_train_recognition(
    temporal_adapter_t* adapter,
    const temporal_visual_input_t* input,
    uint32_t target_id,
    float learning_rate
) {
    if (!adapter || !input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_train_recognition: required parameter is NULL");
        return false;
    }
    if (!adapter->config.enable_training) return false;
    (void)learning_rate; /* TODO: Implement training */

    adapter->stats.training_iterations++;
    return true;
}

bool temporal_train_association(
    temporal_adapter_t* adapter,
    uint32_t concept_a,
    uint32_t concept_b,
    float strength
) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_train_association: adapter is NULL");
        return false;
    }
    if (!adapter->semantic) return false;
    if (!adapter->config.enable_training) return false;
    (void)concept_a; (void)concept_b; (void)strength;

    /* TODO: Implement Hebbian association learning */
    adapter->stats.training_iterations++;
    return true;
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

temporal_status_t temporal_get_status(const temporal_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_get_status: adapter is NULL");
        return TEMPORAL_STATUS_ERROR;
    }
    return adapter->status;
}

temporal_error_t temporal_get_last_error(const temporal_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_get_last_error: adapter is NULL");
        return TEMPORAL_ERROR_INTERNAL;
    }
    return adapter->last_error;
}

const char* temporal_error_string(temporal_error_t error) {
    switch (error) {
        case TEMPORAL_ERROR_NONE: return "No error";
        case TEMPORAL_ERROR_INVALID_INPUT: return "Invalid input";
        case TEMPORAL_ERROR_AUDITORY_FAILURE: return "Auditory processing failure";
        case TEMPORAL_ERROR_RECOGNITION_FAILURE: return "Object recognition failure";
        case TEMPORAL_ERROR_SEMANTIC_FAILURE: return "Semantic retrieval failure";
        case TEMPORAL_ERROR_WORKING_MEMORY_FULL: return "Working memory full";
        case TEMPORAL_ERROR_CONCEPT_NOT_FOUND: return "Concept not found";
        case TEMPORAL_ERROR_BUFFER_OVERFLOW: return "Buffer overflow";
        case TEMPORAL_ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

const char* temporal_status_string(temporal_status_t status) {
    switch (status) {
        case TEMPORAL_STATUS_IDLE: return "Idle";
        case TEMPORAL_STATUS_AUDITORY_PROCESSING: return "Auditory processing";
        case TEMPORAL_STATUS_OBJECT_RECOGNITION: return "Object recognition";
        case TEMPORAL_STATUS_SEMANTIC_RETRIEVAL: return "Semantic retrieval";
        case TEMPORAL_STATUS_INTEGRATION: return "Integration";
        case TEMPORAL_STATUS_READY: return "Ready";
        case TEMPORAL_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

bool temporal_get_stats(const temporal_adapter_t* adapter, temporal_stats_t* stats) {
    if (!adapter || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_get_stats: required parameter is NULL");
        return false;
    }
    memcpy(stats, &adapter->stats, sizeof(temporal_stats_t));
    return true;
}

bool temporal_get_config(const temporal_adapter_t* adapter, temporal_config_t* config) {
    if (!adapter || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_get_config: required parameter is NULL");
        return false;
    }
    memcpy(config, &adapter->config, sizeof(temporal_config_t));
    return true;
}

/*=============================================================================
 * SUB-MODULE ACCESS
 *===========================================================================*/

auditory_processor_t* temporal_get_auditory_processor(temporal_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_get_auditory_processor: adapter is NULL");
        return NULL;
    }
    return adapter->auditory;
}

object_recognition_t* temporal_get_object_recognition(temporal_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_get_object_recognition: adapter is NULL");
        return NULL;
    }
    return adapter->object;
}

semantic_memory_core_t* temporal_get_semantic_memory(temporal_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_get_semantic_memory: adapter is NULL");
        return NULL;
    }
    return adapter->semantic;
}

/*=============================================================================
 * BIO-ASYNC COMMUNICATION
 *===========================================================================*/

bio_module_context_t temporal_get_bio_context(temporal_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_get_bio_context: adapter is NULL");
        bio_module_context_t empty = {0};
        return empty;
    }
    return adapter->bio_ctx;
}

uint32_t temporal_process_bio_messages(temporal_adapter_t* adapter, uint32_t max_messages) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_process_bio_messages: adapter is NULL");
        return 0;
    }
    (void)max_messages;
    /* TODO: Implement bio-async message processing */
    return 0;
}

nimcp_bio_future_t temporal_request_semantic_async(
    temporal_adapter_t* adapter,
    uint32_t concept_id
) {
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    (void)concept_id;
    /* TODO: Implement async semantic request */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "temporal_request_semantic_async: adapter is NULL");
    return NULL;
}

nimcp_error_t temporal_broadcast_auditory_event(
    temporal_adapter_t* adapter,
    const temporal_auditory_result_t* result
) {
    NIMCP_CHECK_THROW(adapter && result, NIMCP_ERROR_NULL_POINTER, "adapter or result is NULL");
    /* TODO: Implement bio-async broadcast */
    return NIMCP_SUCCESS;
}

nimcp_error_t temporal_broadcast_recognition_event(
    temporal_adapter_t* adapter,
    const temporal_recognition_result_t* result
) {
    NIMCP_CHECK_THROW(adapter && result, NIMCP_ERROR_NULL_POINTER, "adapter or result is NULL");
    /* TODO: Implement bio-async broadcast */
    return NIMCP_SUCCESS;
}
