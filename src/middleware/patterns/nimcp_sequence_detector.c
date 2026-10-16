#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_sequence_detector.c - Temporal Sequence Detection
//=============================================================================

#include "middleware/patterns/nimcp_sequence_detector.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/encoding/nimcp_positional_encoding.h"
#include "api/nimcp_api_exception.h"

/* Quantum bridge integration */
#define NIMCP_SEQUENCE_QUANTUM_BRIDGE_IMPLEMENTATION
#include "middleware/patterns/nimcp_sequence_detector_quantum_bridge.h"

#include "utils/memory/nimcp_memory.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"



#define LOG_MODULE "nimcp_sequence_detector"
#define LOG_MODULE_ID 0x0527
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_dimension_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(sequence_detector)

// ============================================================================
// CONSTANTS
// ============================================================================

#define SPIKE_BUFFER_SIZE 10000
#define NGRAM_HASH_SIZE 1000

// ============================================================================
// INTERNAL STRUCTURES
// ============================================================================

typedef struct {
    uint32_t neuron_id;
    double timestamp_ms;
} spike_record_t;

typedef struct {
    spike_record_t* spikes;
    uint32_t capacity;
    uint32_t count;
    uint32_t head;
} spike_buffer_t;

typedef struct ngram_node {
    ngram_pattern_t pattern;
    struct ngram_node* next;
} ngram_node_t;

struct sequence_detector {
    // Configuration
    sequence_detector_config_t config;

    // Spike buffer
    spike_buffer_t buffer;

    // Template library
    sequence_template_t* templates;
    uint32_t num_templates;
    uint32_t next_template_id;

    // N-gram hash table
    ngram_node_t* ngram_table[NGRAM_HASH_SIZE];
    uint32_t total_ngrams;

    // Statistics
    uint64_t total_detections;
    double sum_strength;

    // Positional Encoding
    nimcp_pos_encoder_t* pe_encoder;      // Position encoder instance
    uint64_t pe_matches;                   // Number of PE-enhanced matches
    double pe_similarity_sum;              // Sum of PE similarities
    uint64_t pe_cache_hits;                // PE cache hits
    uint64_t pe_cache_misses;              // PE cache misses

    // Quantum Bridge
    sequence_quantum_bridge_t* quantum_bridge;  // Quantum-accelerated matching
    uint64_t quantum_matches;                    // Number of quantum-accelerated matches

    /* Thread safety: mutex protects buffer, templates, ngrams, and statistics.
     * Added to fix thread-safety issue - concurrent calls to sequence_detector
     * functions could corrupt internal state. */
    nimcp_mutex_t* mutex;
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static bool init_spike_buffer(spike_buffer_t* buffer, uint32_t capacity) {
    buffer->spikes = (spike_record_t*)nimcp_calloc(capacity, sizeof(spike_record_t));
    if (!buffer->spikes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_spike_buffer: buffer->spikes is NULL");
        return false;
    }
    buffer->capacity = capacity;
    buffer->count = 0;
    buffer->head = 0;
    return true;
}

static void free_spike_buffer(spike_buffer_t* buffer) {
    if (buffer && buffer->spikes) {
        nimcp_free(buffer->spikes);
        buffer->spikes = NULL;
    }
}

static void buffer_add_spike(spike_buffer_t* buffer, uint32_t neuron_id,
                            double timestamp_ms) {
    buffer->spikes[buffer->head].neuron_id = neuron_id;
    buffer->spikes[buffer->head].timestamp_ms = timestamp_ms;
    buffer->head = (buffer->head + 1) % buffer->capacity;
    if (buffer->count < buffer->capacity) {
        buffer->count++;
    }
}

static uint32_t hash_ngram(const uint32_t* neurons, uint32_t length) {
    uint32_t hash = 5381;
    for (uint32_t i = 0; i < length; i++) {
        hash = ((hash << 5) + hash) + neurons[i];
    }
    return hash % NGRAM_HASH_SIZE;
}

static void add_ngram(sequence_detector_t* detector, const uint32_t* neurons,
                     uint32_t length, float interval_ms) {
    if (length < 2 || length > detector->config.max_ngram) return;

    uint32_t hash = hash_ngram(neurons, length);

    // Search for existing N-gram
    ngram_node_t* node = detector->ngram_table[hash];
    while (node) {
        if (node->pattern.length == length) {
            bool match = true;
            for (uint32_t i = 0; i < length; i++) {
                if (node->pattern.neurons[i] != neurons[i]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                // Update existing
                node->pattern.count++;
                float alpha = 0.1F;
                node->pattern.avg_interval_ms =
                    (1.0F - alpha) * node->pattern.avg_interval_ms + alpha * interval_ms;
                return;
            }
        }
        node = node->next;
    }

    // Create new N-gram
    ngram_node_t* new_node = (ngram_node_t*)nimcp_calloc(1, sizeof(ngram_node_t));
    if (!new_node) return;

    new_node->pattern.length = length;
    for (uint32_t i = 0; i < length; i++) {
        new_node->pattern.neurons[i] = neurons[i];
    }
    new_node->pattern.count = 1;
    new_node->pattern.avg_interval_ms = interval_ms;

    new_node->next = detector->ngram_table[hash];
    detector->ngram_table[hash] = new_node;
    detector->total_ngrams++;
}

static void extract_ngrams(sequence_detector_t* detector) {
    if (!detector->config.enable_ngram_learning || detector->buffer.count < 2) {
        return;
    }

    // Extract N-grams from recent spikes
    for (uint32_t n = 2; n <= detector->config.max_ngram; n++) {
        if (detector->buffer.count < n) continue;

        for (uint32_t i = 0; i + n <= detector->buffer.count; i++) {
            uint32_t neurons[SEQUENCE_MAX_NGRAM];
            double start_time = 0.0;
            double end_time = 0.0;

            for (uint32_t j = 0; j < n; j++) {
                uint32_t idx = (detector->buffer.head + detector->buffer.capacity -
                              detector->buffer.count + i + j) % detector->buffer.capacity;
                neurons[j] = detector->buffer.spikes[idx].neuron_id;

                if (j == 0) start_time = detector->buffer.spikes[idx].timestamp_ms;
                if (j == n - 1) end_time = detector->buffer.spikes[idx].timestamp_ms;
            }

            float interval = (float)(end_time - start_time) / (float)(n - 1);
            add_ngram(detector, neurons, n, interval);
        }
    }
}

static float match_template(const spike_buffer_t* buffer,
                           const sequence_template_t* seq_template,
                           float tolerance_ms,
                           bool* is_forward, bool* is_backward,
                           float* compression) {
    if (!buffer || !seq_template || buffer->count < seq_template->length) {
        return 0.0F;
    }

    uint32_t matched = 0;
    double first_spike_time = 0.0;
    double last_spike_time = 0.0;

    // Try forward match
    for (uint32_t i = 0; i < seq_template->length; i++) {
        uint32_t target_neuron = seq_template->elements[i].neuron_id;
        float target_time = seq_template->elements[i].relative_time_ms;

        // Search in buffer for matching spike
        bool found = false;
        for (uint32_t j = 0; j < buffer->count; j++) {
            uint32_t idx = (buffer->head + buffer->capacity - buffer->count + j) %
                          buffer->capacity;

            if (buffer->spikes[idx].neuron_id == target_neuron) {
                if (i == 0) {
                    first_spike_time = buffer->spikes[idx].timestamp_ms;
                    found = true;
                    matched++;
                } else {
                    double expected_time = first_spike_time + target_time;
                    double actual_time = buffer->spikes[idx].timestamp_ms;
                    double error = fabs(actual_time - expected_time);

                    if (error <= tolerance_ms) {
                        found = true;
                        matched++;
                        if (i == seq_template->length - 1) {
                            last_spike_time = actual_time;
                        }
                    }
                }
                break;
            }
        }

        if (!found) break;
    }

    float strength = (float)matched / (float)seq_template->length;

    if (strength > 0.5F) {
        *is_forward = true;
        *is_backward = false;

        if (matched >= 2) {
            double actual_duration = last_spike_time - first_spike_time;
            *compression = (float)(actual_duration / (seq_template->duration_ms + 1e-6));
        } else {
            *compression = 1.0F;
        }
    }

    return strength;
}

// ============================================================================
// PUBLIC API
// ============================================================================

sequence_detector_config_t sequence_detector_default_config(void) {
    sequence_detector_config_t config;
    config.max_templates = SEQUENCE_MAX_TEMPLATES;
    config.max_sequence_length = SEQUENCE_MAX_LENGTH;
    config.temporal_tolerance_ms = SEQUENCE_TEMPORAL_TOLERANCE_MS;
    config.min_strength_threshold = SEQUENCE_MIN_STRENGTH;
    config.max_ngram = SEQUENCE_MAX_NGRAM;
    config.enable_replay_detection = true;
    config.enable_ngram_learning = true;
    config.enable_compression = true;

    // Positional Encoding defaults (disabled by default)
    config.enable_positional_encoding = false;
    config.pe_type = NIMCP_POS_ROTARY;  // RoPE for temporal sequences
    config.pe_embedding_dim = NIMCP_SMALL_EMBEDDING_DIM;       // Moderate dimension
    config.pe_similarity_weight = 0.3F; // Temporal matching still dominant

    // Quantum acceleration defaults (enabled by default)
    config.enable_quantum_matching = true;
    config.quantum_match_threshold = 0.7F;  // Require 70% similarity for quantum match
    return config;
}

sequence_detector_t* sequence_detector_create(const sequence_detector_config_t* config) {
    if (!config || config->max_templates == 0 || config->max_sequence_length == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sequence_detector_create: config is NULL or invalid");
        return NULL;
    }

    sequence_detector_t* detector = (sequence_detector_t*)nimcp_calloc(1, sizeof(sequence_detector_t));
    if (!detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sequence_detector_create: failed to allocate detector");
        return NULL;
    }

    detector->config = *config;

    /* Thread safety: Create mutex to protect detector state.
     * This fixes thread-safety issue where concurrent calls could corrupt state. */
    detector->mutex = nimcp_mutex_create(NULL);
    if (!detector->mutex) {
        sequence_detector_destroy(detector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sequence_detector_create: detector->mutex is NULL");
        return NULL;
    }

    // Initialize spike buffer
    if (!init_spike_buffer(&detector->buffer, SPIKE_BUFFER_SIZE)) {
        sequence_detector_destroy(detector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "sequence_detector_create: init_spike_buffer is NULL");
        return NULL;
    }

    // Allocate seq_template storage
    detector->templates = (sequence_template_t*)nimcp_calloc(config->max_templates,
                                                       sizeof(sequence_template_t));
    if (!detector->templates) {
        sequence_detector_destroy(detector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sequence_detector_create: detector->templates is NULL");
        return NULL;
    }

    detector->num_templates = 0;
    detector->next_template_id = 1;

    // Initialize N-gram table
    memset(detector->ngram_table, 0, sizeof(detector->ngram_table));
    detector->total_ngrams = 0;

    // Initialize statistics
    detector->total_detections = 0;
    detector->sum_strength = 0.0;

    // Initialize PE fields
    detector->pe_encoder = NULL;
    detector->pe_matches = 0;
    detector->pe_similarity_sum = 0.0;
    detector->pe_cache_hits = 0;
    detector->pe_cache_misses = 0;

    // Auto-create PE encoder if config requests it
    if (detector->config.enable_positional_encoding) {
        nimcp_pos_config_t pe_config;
        memset(&pe_config, 0, sizeof(pe_config));
        pe_config.type = detector->config.pe_type;

        if (pe_config.type == NIMCP_POS_ROTARY) {
            nimcp_pos_rope_config_t rope_cfg = nimcp_pos_rope_default_config();
            rope_cfg.base.embedding_dim = detector->config.pe_embedding_dim;
            rope_cfg.base.max_seq_length = detector->config.max_sequence_length;
            rope_cfg.base.cache_enabled = true;
            rope_cfg.base.thread_safe = true;
            pe_config.config.rope = rope_cfg;
        } else if (pe_config.type == NIMCP_POS_RELATIVE) {
            nimcp_pos_relative_config_t rel_cfg = nimcp_pos_relative_default_config();
            rel_cfg.base.embedding_dim = detector->config.pe_embedding_dim;
            rel_cfg.base.max_seq_length = detector->config.max_sequence_length;
            rel_cfg.base.cache_enabled = true;
            rel_cfg.base.thread_safe = true;
            pe_config.config.relative = rel_cfg;
        }

        if (nimcp_pos_validate_config(&pe_config) == NIMCP_POS_SUCCESS) {
            detector->pe_encoder = nimcp_pos_encoder_create(&pe_config);
            if (detector->pe_encoder) {
                uint32_t cache_length = (detector->config.max_sequence_length < 512) ?
                                        detector->config.max_sequence_length : 512;
                nimcp_pos_cache_precompute(detector->pe_encoder, cache_length);
                NIMCP_LOGGING_INFO("Positional encoding enabled: type=%d, dim=%u",
                                   (int)pe_config.type, detector->config.pe_embedding_dim);
            }
        }
    }

    // Initialize quantum bridge
    detector->quantum_bridge = NULL;
    detector->quantum_matches = 0;
    if (detector->config.enable_quantum_matching) {
        sequence_quantum_config_t qconfig = sequence_quantum_default_config();
        qconfig.match_threshold = detector->config.quantum_match_threshold;
        detector->quantum_bridge = sequence_quantum_bridge_create(&qconfig);
        if (detector->quantum_bridge) {
            NIMCP_LOGGING_INFO("Quantum-accelerated sequence matching enabled");
        }
    }

    return detector;
}

void sequence_detector_destroy(sequence_detector_t* detector) {
    if (!detector) return;

    free_spike_buffer(&detector->buffer);

    // Free templates
    if (detector->templates) {
        for (uint32_t i = 0; i < detector->num_templates; i++) {
            if (detector->templates[i].elements) {
                // Free position embeddings for each element
                for (uint32_t j = 0; j < detector->templates[i].length; j++) {
                    if (detector->templates[i].elements[j].position_embedding) {
                        nimcp_free(detector->templates[i].elements[j].position_embedding);
                    }
                }
                nimcp_free(detector->templates[i].elements);
            }
        }
        nimcp_free(detector->templates);
    }

    // Free N-grams
    for (uint32_t i = 0; i < NGRAM_HASH_SIZE; i++) {
        ngram_node_t* node = detector->ngram_table[i];
        while (node) {
            ngram_node_t* next = node->next;
            nimcp_free(node);
            node = next;
        }
    }

    // Destroy PE encoder
    if (detector->pe_encoder) {
        nimcp_pos_encoder_destroy(detector->pe_encoder);
    }

    // Destroy quantum bridge
    if (detector->quantum_bridge) {
        sequence_quantum_bridge_destroy(detector->quantum_bridge);
    }

    /* Thread safety: Clean up mutex */
    if (detector->mutex) {
        nimcp_mutex_free(detector->mutex);
    }

    nimcp_free(detector);
}

bool sequence_detector_add_spike(sequence_detector_t* detector,
                                  uint32_t neuron_id,
                                  double timestamp_ms) {
    if (!detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sequence_detector_destroy: detector is NULL");
        return false;
    }

    /* Thread safety: Lock mutex to protect buffer and ngram access */
    nimcp_mutex_lock(detector->mutex);

    buffer_add_spike(&detector->buffer, neuron_id, timestamp_ms);

    // Extract N-grams periodically
    if (detector->config.enable_ngram_learning && detector->buffer.count >= 2) {
        extract_ngrams(detector);
    }

    nimcp_mutex_unlock(detector->mutex);
    return true;
}

bool sequence_detector_learn_template(sequence_detector_t* detector,
                                       const sequence_element_t* elements,
                                       uint32_t length,
                                       uint32_t* template_id) {
    if (!detector || !elements || length == 0 ||
        length > detector->config.max_sequence_length ||
        detector->num_templates >= detector->config.max_templates) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sequence_detector_destroy: operation failed");
        return false;
    }

    /* Thread safety: Lock mutex to protect template storage */
    nimcp_mutex_lock(detector->mutex);

    sequence_template_t* tmpl = &detector->templates[detector->num_templates];

    tmpl->elements = (sequence_element_t*)nimcp_malloc(length * sizeof(sequence_element_t));
    if (!tmpl->elements) {
        nimcp_mutex_unlock(detector->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sequence_detector_destroy: tmpl->elements is NULL");
        return false;
    }

    memcpy(tmpl->elements, elements, length * sizeof(sequence_element_t));
    tmpl->length = length;
    tmpl->duration_ms = elements[length - 1].relative_time_ms;
    tmpl->observations = 0;
    tmpl->avg_strength = 0.0F;
    tmpl->template_id = detector->next_template_id++;

    if (template_id) *template_id = tmpl->template_id;

    detector->num_templates++;

    // Register with quantum bridge if enabled
    if (detector->quantum_bridge && sequence_quantum_bridge_is_enabled(detector->quantum_bridge)) {
        // Extract neuron IDs for quantum pattern
        uint32_t* symbols = (uint32_t*)nimcp_malloc(length * sizeof(uint32_t));
        if (symbols) {
            for (uint32_t i = 0; i < length; i++) {
                symbols[i] = elements[i].neuron_id;
            }
            char name[32];
            snprintf(name, sizeof(name), "template_%u", tmpl->template_id);
            sequence_quantum_add_template(detector->quantum_bridge, name, symbols, length);
            nimcp_free(symbols);
        }
    }

    nimcp_mutex_unlock(detector->mutex);
    return true;
}

bool sequence_detector_detect(sequence_detector_t* detector,
                               sequence_detection_t* detections,
                               uint32_t max_detections,
                               uint32_t* num_detected) {
    if (!detector || !detections || !num_detected || max_detections == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sequence_detector_destroy: required parameter is NULL (detector, detections, num_detected)");
        return false;
    }

    /* Thread safety: Lock mutex to protect buffer, templates, and statistics */
    nimcp_mutex_lock(detector->mutex);

    *num_detected = 0;

    // Use quantum-accelerated matching if enabled
    if (detector->quantum_bridge &&
        sequence_quantum_bridge_is_enabled(detector->quantum_bridge) &&
        detector->buffer.count > 0) {

        // Extract current sequence from buffer
        uint32_t seq_len = detector->buffer.count > 32 ? 32 : detector->buffer.count;
        uint32_t* symbols = (uint32_t*)nimcp_malloc(seq_len * sizeof(uint32_t));
        if (symbols) {
            for (uint32_t i = 0; i < seq_len; i++) {
                uint32_t idx = (detector->buffer.head + detector->buffer.capacity -
                               detector->buffer.count + i) % detector->buffer.capacity;
                symbols[i] = detector->buffer.spikes[idx].neuron_id;
            }

            // Quantum match
            qseq_match_result_t qresult;
            if (sequence_quantum_match(detector->quantum_bridge, symbols, seq_len, &qresult) == 0) {
                if (qresult.similarity >= detector->config.quantum_match_threshold) {
                    detector->quantum_matches++;
                    // Use quantum match as hint for classical verification
                }
            }
            nimcp_free(symbols);
        }
    }

    // Match against all templates
    for (uint32_t i = 0; i < detector->num_templates && *num_detected < max_detections; i++) {
        bool is_forward = false;
        bool is_backward = false;
        float compression = 1.0F;

        float strength = match_template(&detector->buffer,
                                       &detector->templates[i],
                                       detector->config.temporal_tolerance_ms,
                                       &is_forward, &is_backward,
                                       &compression);

        if (strength >= detector->config.min_strength_threshold) {
            sequence_detection_t* det = &detections[*num_detected];

            det->template_id = detector->templates[i].template_id;
            det->strength = strength;
            det->is_forward = is_forward;
            det->is_backward = is_backward;
            det->compression_factor = compression;
            det->matched_elements = (uint32_t)(strength * detector->templates[i].length);
            det->total_elements = detector->templates[i].length;

            // Estimate start/end times from buffer
            if (detector->buffer.count > 0) {
                uint32_t first = (detector->buffer.head + detector->buffer.capacity -
                                detector->buffer.count) % detector->buffer.capacity;
                uint32_t last = (detector->buffer.head + detector->buffer.capacity - 1) %
                               detector->buffer.capacity;

                det->start_time_ms = detector->buffer.spikes[first].timestamp_ms;
                det->end_time_ms = detector->buffer.spikes[last].timestamp_ms;
            }

            // Update seq_template statistics
            detector->templates[i].observations++;
            float alpha = 0.1F;
            detector->templates[i].avg_strength =
                (1.0F - alpha) * detector->templates[i].avg_strength + alpha * strength;

            (*num_detected)++;
            detector->total_detections++;
            detector->sum_strength += strength;
        }
    }

    nimcp_mutex_unlock(detector->mutex);
    return true;
}

bool sequence_detector_get_template(const sequence_detector_t* detector,
                                     uint32_t template_id,
                                     sequence_template_t* seq_template) {
    if (!detector || !seq_template) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (detector, seq_template)");
        return false;
    }

    for (uint32_t i = 0; i < detector->num_templates; i++) {
        if (detector->templates[i].template_id == template_id) {
            *seq_template = detector->templates[i];
            return true;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
    return false;
}

bool sequence_detector_get_ngrams(const sequence_detector_t* detector,
                                   ngram_pattern_t* ngrams,
                                   uint32_t max_ngrams,
                                   uint32_t* num_ngrams) {
    if (!detector || !ngrams || !num_ngrams || max_ngrams == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (detector, ngrams, num_ngrams)");
        return false;
    }

    *num_ngrams = 0;

    // Collect all N-grams
    for (uint32_t i = 0; i < NGRAM_HASH_SIZE && *num_ngrams < max_ngrams; i++) {
        ngram_node_t* node = detector->ngram_table[i];
        while (node && *num_ngrams < max_ngrams) {
            ngrams[*num_ngrams] = node->pattern;
            (*num_ngrams)++;
            node = node->next;
        }
    }

    // Sort by count (simple bubble sort for small arrays)
    for (uint32_t i = 0; i < *num_ngrams; i++) {
        for (uint32_t j = i + 1; j < *num_ngrams; j++) {
            if (ngrams[j].count > ngrams[i].count) {
                ngram_pattern_t temp = ngrams[i];
                ngrams[i] = ngrams[j];
                ngrams[j] = temp;
            }
        }
    }

    return true;
}

void sequence_detector_reset(sequence_detector_t* detector) {
    if (!detector) return;

    /* Thread safety: Lock mutex to protect buffer access */
    nimcp_mutex_lock(detector->mutex);

    detector->buffer.count = 0;
    detector->buffer.head = 0;

    nimcp_mutex_unlock(detector->mutex);
}

void sequence_detector_clear_templates(sequence_detector_t* detector) {
    if (!detector) return;

    /* Thread safety: Lock mutex to protect template storage */
    nimcp_mutex_lock(detector->mutex);

    for (uint32_t i = 0; i < detector->num_templates; i++) {
        nimcp_free(detector->templates[i].elements);
    }

    detector->num_templates = 0;
    detector->next_template_id = 1;

    nimcp_mutex_unlock(detector->mutex);
}

bool sequence_detector_get_stats(const sequence_detector_t* detector,
                                  uint32_t* num_templates,
                                  uint64_t* total_detections,
                                  float* avg_strength) {
    if (!detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sequence_detector_clear_templates: detector is NULL");
        return false;
    }

    if (num_templates) *num_templates = detector->num_templates;
    if (total_detections) *total_detections = detector->total_detections;

    if (avg_strength) {
        *avg_strength = (detector->total_detections > 0) ?
                       (float)(detector->sum_strength / detector->total_detections) : 0.0F;
    }

    return true;
}

// ============================================================================
// POSITIONAL ENCODING INTEGRATION
// ============================================================================

/**
 * @brief Compute cosine similarity between two vectors
 *
 * WHAT: Calculate normalized dot product for similarity measure
 * WHY:  Standard metric for comparing position embeddings
 * HOW:  dot(a,b) / (||a|| * ||b||)
 */
static float compute_cosine_similarity(const float* vec1, const float* vec2,
                                       uint32_t dim) {
    if (!vec1 || !vec2 || dim == 0) return 0.0F;

    float dot = 0.0F;
    float norm1 = 0.0F;
    float norm2 = 0.0F;

    for (uint32_t i = 0; i < dim; i++) {
        dot += vec1[i] * vec2[i];
        norm1 += vec1[i] * vec1[i];
        norm2 += vec2[i] * vec2[i];
    }

    float norm_product = sqrtf(norm1) * sqrtf(norm2);
    if (norm_product < 1e-8F) return 0.0F;

    return dot / norm_product;
}

bool sequence_detector_set_pe_config(sequence_detector_t* detector,
                                      const nimcp_pos_config_t* pe_config) {
    /**
     * WHAT: Configure positional encoding for sequence detector
     * WHY:  Enable position-aware pattern matching with temporal context
     * HOW:  Create PE encoder and configure detector for PE-enhanced matching
     *
     * BIOLOGICAL BASIS:
     * - Hippocampal theta phase codes position within sequences
     * - Phase precession provides temporal context for place cells
     * - Grid cells encode relative positions in spatial/temporal domains
     */

    if (!detector || !pe_config) {
        LOG_ERROR("SequenceDetector: NULL parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sequence_detector_clear_templates: required parameter is NULL (detector, pe_config)");
        return false;
    }

    // Validate PE configuration
    if (nimcp_pos_validate_config(pe_config) != NIMCP_POS_SUCCESS) {
        LOG_ERROR("SequenceDetector: Invalid PE configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sequence_detector_clear_templates: validation failed");
        return false;
    }

    // Only support RoPE and Relative encoding for sequences
    if (pe_config->type != NIMCP_POS_ROTARY && pe_config->type != NIMCP_POS_RELATIVE) {
        LOG_ERROR("SequenceDetector: Unsupported PE type %d (use ROTARY or RELATIVE)",
                  pe_config->type);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sequence_detector_clear_templates: validation failed");
        return false;
    }

    /* Thread safety: Lock mutex to protect PE encoder and config */
    nimcp_mutex_lock(detector->mutex);

    // Destroy existing encoder if present
    if (detector->pe_encoder) {
        nimcp_pos_encoder_destroy(detector->pe_encoder);
        detector->pe_encoder = NULL;
    }

    // Create new encoder
    detector->pe_encoder = nimcp_pos_encoder_create(pe_config);
    if (!detector->pe_encoder) {
        LOG_ERROR("SequenceDetector: Failed to create PE encoder");
        nimcp_mutex_unlock(detector->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sequence_detector_clear_templates: detector->pe_encoder is NULL");
        return false;
    }

    // Update detector configuration
    detector->config.enable_positional_encoding = true;
    detector->config.pe_type = pe_config->type;
    detector->config.pe_embedding_dim = (pe_config->type == NIMCP_POS_ROTARY) ?
                                         pe_config->config.rope.base.embedding_dim :
                                         pe_config->config.relative.base.embedding_dim;

    // Pre-compute encodings for typical sequence lengths
    uint32_t cache_length = (detector->config.max_sequence_length < 512) ?
                            detector->config.max_sequence_length : 512;
    nimcp_pos_cache_precompute(detector->pe_encoder, cache_length);

    LOG_INFO("SequenceDetector: PE configured: type=%s, dim=%u",
             nimcp_pos_type_to_string(pe_config->type),
             detector->config.pe_embedding_dim);

    nimcp_mutex_unlock(detector->mutex);
    return true;
}

bool sequence_detector_encode_template(sequence_detector_t* detector,
                                        uint32_t template_id) {
    /**
     * WHAT: Apply positional encoding to sequence template elements
     * WHY:  Enable position-aware matching with temporal context
     * HOW:  Encode each element position using RoPE or Relative PE
     *
     * BIOLOGICAL BASIS:
     * - Hippocampal sequences encode temporal order via phase
     * - Replay detection benefits from position-dependent firing
     * - Sequence consolidation uses temporal context information
     */

    if (!detector) {
        LOG_ERROR("SequenceDetector: NULL detector");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sequence_detector_clear_templates: detector is NULL");
        return false;
    }

    if (!detector->config.enable_positional_encoding || !detector->pe_encoder) {
        LOG_ERROR("SequenceDetector: PE not configured");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sequence_detector_clear_templates: required parameter is NULL (detector->config, detector->pe_encoder)");
        return false;
    }

    /* Thread safety: Lock mutex to protect template access */
    nimcp_mutex_lock(detector->mutex);

    // Find template
    sequence_template_t* tmpl = NULL;
    for (uint32_t i = 0; i < detector->num_templates; i++) {
        if (detector->templates[i].template_id == template_id) {
            tmpl = &detector->templates[i];
            break;
        }
    }

    if (!tmpl) {
        LOG_ERROR("SequenceDetector: Template %u not found", template_id);
        nimcp_mutex_unlock(detector->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sequence_detector_clear_templates: tmpl is NULL");
        return false;
    }

    // Allocate position embeddings for each element
    for (uint32_t i = 0; i < tmpl->length; i++) {
        sequence_element_t* elem = &tmpl->elements[i];

        // Free existing embedding if present
        if (elem->position_embedding) {
            nimcp_free(elem->position_embedding);
        }

        // Allocate new embedding
        elem->embedding_dim = detector->config.pe_embedding_dim;
        elem->position_embedding = (float*)nimcp_malloc(elem->embedding_dim * sizeof(float));
        if (!elem->position_embedding) {
            LOG_ERROR("SequenceDetector: Failed to allocate position embedding");
            nimcp_mutex_unlock(detector->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sequence_detector_clear_templates: elem->position_embedding is NULL");
            return false;
        }

        // Encode position
        int result = nimcp_pos_encode_position(detector->pe_encoder, i,
                                               elem->position_embedding);
        if (result != NIMCP_POS_SUCCESS) {
            LOG_ERROR("SequenceDetector: PE encoding failed: %d", result);
            nimcp_mutex_unlock(detector->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sequence_detector_clear_templates: validation failed");
            return false;
        }
    }

    LOG_DEBUG("SequenceDetector: Template %u encoded with %u positions",
              template_id, tmpl->length);

    nimcp_mutex_unlock(detector->mutex);
    return true;
}

/**
 * @brief Match template with PE-enhanced similarity scoring
 *
 * WHAT: Match spike buffer against template with positional context
 * WHY:  Discriminate sequences with similar content but different timing
 * HOW:  Combine temporal matching + PE similarity for hybrid score
 */
static float match_template_with_pe(const spike_buffer_t* buffer,
                                    const sequence_template_t* tmpl,
                                    float tolerance_ms,
                                    nimcp_pos_encoder_t* pe_encoder,
                                    float pe_weight,
                                    bool* is_forward, bool* is_backward,
                                    float* compression,
                                    float* pe_similarity_out) {
    if (!buffer || !tmpl || !pe_encoder || buffer->count < tmpl->length) {
        return 0.0F;
    }

    uint32_t matched = 0;
    double first_spike_time = 0.0;
    double last_spike_time = 0.0;
    float total_pe_similarity = 0.0F;
    uint32_t pe_comparisons = 0;

    // Match elements with temporal and positional scoring
    for (uint32_t i = 0; i < tmpl->length; i++) {
        uint32_t target_neuron = tmpl->elements[i].neuron_id;
        float target_time = tmpl->elements[i].relative_time_ms;

        // Search buffer for matching spike
        bool found = false;
        for (uint32_t j = 0; j < buffer->count; j++) {
            uint32_t idx = (buffer->head + buffer->capacity - buffer->count + j) %
                          buffer->capacity;

            if (buffer->spikes[idx].neuron_id == target_neuron) {
                if (i == 0) {
                    first_spike_time = buffer->spikes[idx].timestamp_ms;
                    found = true;
                    matched++;
                } else {
                    double expected_time = first_spike_time + target_time;
                    double actual_time = buffer->spikes[idx].timestamp_ms;
                    double error = fabs(actual_time - expected_time);

                    if (error <= tolerance_ms) {
                        found = true;
                        matched++;
                        if (i == tmpl->length - 1) {
                            last_spike_time = actual_time;
                        }

                        // Compute PE similarity if embeddings available
                        if (tmpl->elements[i].position_embedding) {
                            float buffer_encoding[256]; // Max embedding dim
                            uint32_t dim = tmpl->elements[i].embedding_dim;
                            if (dim <= 256) {
                                nimcp_pos_encode_position(pe_encoder, j, buffer_encoding);
                                float sim = compute_cosine_similarity(
                                    tmpl->elements[i].position_embedding,
                                    buffer_encoding, dim);
                                total_pe_similarity += sim;
                                pe_comparisons++;
                            }
                        }
                    }
                }
                break;
            }
        }

        if (!found) break;
    }

    // Compute temporal match strength
    float temporal_strength = (float)matched / (float)tmpl->length;

    // Compute PE similarity score
    float pe_similarity = (pe_comparisons > 0) ?
                         (total_pe_similarity / (float)pe_comparisons) : 0.0F;

    // Combined score: weighted average of temporal and positional
    float combined_strength = (1.0F - pe_weight) * temporal_strength +
                             pe_weight * pe_similarity;

    // Update metadata if strong match
    if (combined_strength > 0.5F) {
        *is_forward = true;
        *is_backward = false;

        if (matched >= 2) {
            double actual_duration = last_spike_time - first_spike_time;
            *compression = (float)(actual_duration / (tmpl->duration_ms + 1e-6));
        } else {
            *compression = 1.0F;
        }
    }

    if (pe_similarity_out) {
        *pe_similarity_out = pe_similarity;
    }

    return combined_strength;
}

bool sequence_detector_match_with_pe(sequence_detector_t* detector,
                                      sequence_detection_t* detections,
                                      uint32_t max_detections,
                                      uint32_t* num_detected) {
    /**
     * WHAT: Detect sequences with position-aware temporal matching
     * WHY:  Discriminate similar sequences differing in temporal structure
     * HOW:  Hybrid scoring combining temporal precision and PE similarity
     *
     * BIOLOGICAL BASIS:
     * - Hippocampal replay uses both content and temporal order
     * - Phase coding provides temporal context for sequence elements
     * - Position-dependent firing enables disambiguation
     */

    if (!detector || !detections || !num_detected || max_detections == 0) {
        LOG_ERROR("SequenceDetector: Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (detector, detections, num_detected)");
        return false;
    }

    if (!detector->config.enable_positional_encoding || !detector->pe_encoder) {
        LOG_ERROR("SequenceDetector: PE not configured");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (detector->config, detector->pe_encoder)");
        return false;
    }

    /* Thread safety: Lock mutex to protect buffer, templates, and statistics */
    nimcp_mutex_lock(detector->mutex);

    *num_detected = 0;

    // Match against all templates with PE enhancement
    for (uint32_t i = 0; i < detector->num_templates && *num_detected < max_detections; i++) {
        bool is_forward = false;
        bool is_backward = false;
        float compression = 1.0F;
        float pe_similarity = 0.0F;

        float strength = match_template_with_pe(&detector->buffer,
                                                &detector->templates[i],
                                                detector->config.temporal_tolerance_ms,
                                                detector->pe_encoder,
                                                detector->config.pe_similarity_weight,
                                                &is_forward, &is_backward,
                                                &compression, &pe_similarity);

        if (strength >= detector->config.min_strength_threshold) {
            sequence_detection_t* det = &detections[*num_detected];

            det->template_id = detector->templates[i].template_id;
            det->strength = strength;
            det->is_forward = is_forward;
            det->is_backward = is_backward;
            det->compression_factor = compression;
            det->matched_elements = (uint32_t)(strength * detector->templates[i].length);
            det->total_elements = detector->templates[i].length;

            // Estimate start/end times
            if (detector->buffer.count > 0) {
                uint32_t first = (detector->buffer.head + detector->buffer.capacity -
                                detector->buffer.count) % detector->buffer.capacity;
                uint32_t last = (detector->buffer.head + detector->buffer.capacity - 1) %
                               detector->buffer.capacity;

                det->start_time_ms = detector->buffer.spikes[first].timestamp_ms;
                det->end_time_ms = detector->buffer.spikes[last].timestamp_ms;
            }

            // Update statistics
            detector->templates[i].observations++;
            float alpha = 0.1F;
            detector->templates[i].avg_strength =
                (1.0F - alpha) * detector->templates[i].avg_strength + alpha * strength;

            (*num_detected)++;
            detector->total_detections++;
            detector->sum_strength += strength;

            // Update PE statistics
            detector->pe_matches++;
            detector->pe_similarity_sum += pe_similarity;
        }
    }

    nimcp_mutex_unlock(detector->mutex);
    return true;
}

bool sequence_detector_get_pe_stats(const sequence_detector_t* detector,
                                     float* pe_match_rate,
                                     float* avg_pe_similarity,
                                     float* pe_cache_hit_rate) {
    /**
     * WHAT: Retrieve positional encoding performance metrics
     * WHY:  Monitor effectiveness of PE-enhanced matching
     * HOW:  Return match rates, similarity scores, cache performance
     */

    if (!detector) {
        LOG_ERROR("SequenceDetector: NULL detector");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: detector is NULL");
        return false;
    }

    if (!detector->config.enable_positional_encoding || !detector->pe_encoder) {
        // PE not configured - return false to indicate PE stats are unavailable
        if (pe_match_rate) *pe_match_rate = 0.0F;
        if (avg_pe_similarity) *avg_pe_similarity = 0.0F;
        if (pe_cache_hit_rate) *pe_cache_hit_rate = 0.0F;
        return false;
    }

    // PE match rate
    if (pe_match_rate) {
        *pe_match_rate = (detector->total_detections > 0) ?
                        (float)detector->pe_matches / (float)detector->total_detections :
                        0.0F;
    }

    // Average PE similarity
    if (avg_pe_similarity) {
        *avg_pe_similarity = (detector->pe_matches > 0) ?
                            (float)(detector->pe_similarity_sum / detector->pe_matches) :
                            0.0F;
    }

    // PE cache hit rate
    if (pe_cache_hit_rate) {
        nimcp_pos_stats_t pe_stats;
        if (nimcp_pos_get_stats(detector->pe_encoder, &pe_stats) == NIMCP_POS_SUCCESS) {
            uint64_t total = pe_stats.cache_hits + pe_stats.cache_misses;
            *pe_cache_hit_rate = (total > 0) ?
                                (float)pe_stats.cache_hits / (float)total : 0.0F;
        } else {
            *pe_cache_hit_rate = 0.0F;
        }
    }

    return true;
}
