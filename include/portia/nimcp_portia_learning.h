#ifndef NIMCP_PORTIA_LEARNING_H
#define NIMCP_PORTIA_LEARNING_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_portia_learning.h
 * @brief Portia spider learning modes - lightweight learning for constrained platforms
 *
 * Portia spiders exhibit multiple learning modes despite limited neural resources:
 * habituation, associative learning, and trial-and-error. This module implements
 * memory-efficient learning mechanisms suitable for resource-constrained systems.
 */

/**
 * Learning mode types
 */
typedef enum {
    LEARNING_MODE_DISABLED = 0,
    LEARNING_MODE_HABITUATION = 1 << 0,    // Decrease response to repeated stimuli
    LEARNING_MODE_SENSITIZATION = 1 << 1,  // Increase response to important stimuli
    LEARNING_MODE_ASSOCIATIVE = 1 << 2,    // Classical conditioning
    LEARNING_MODE_TRIAL_ERROR = 1 << 3,    // Operant conditioning
    LEARNING_MODE_OBSERVATIONAL = 1 << 4,  // Learn from others
    LEARNING_MODE_FULL = 0xFF              // All modes active
} portia_learning_mode_t;

/**
 * Habituation entry - tracks response decrease to repeated stimuli
 */
typedef struct {
    uint32_t stimulus_id;
    float response_strength;       // Current response level (0.0-1.0)
    uint32_t exposure_count;       // Times encountered
    uint64_t last_exposure_ms;     // Last exposure timestamp
    float habituation_rate;        // Rate of response decrease
    float recovery_rate;           // Spontaneous recovery rate
    bool is_active;                // Entry is in use
} habituation_entry_t;

/**
 * Association entry - tracks stimulus-response associations
 */
typedef struct {
    uint32_t stimulus_id;
    uint32_t response_id;
    float association_strength;    // 0.0-1.0
    uint32_t reinforcement_count;  // Times reinforced
    uint64_t last_reinforcement_ms;
    bool is_positive;              // Reward (true) or punishment (false)
    bool is_active;                // Entry is in use
} association_entry_t;

/**
 * Forward declarations - use void* to avoid platform dependencies
 */

/**
 * Learning state - main learning system state
 */
typedef struct {
    portia_learning_mode_t active_mode;
    habituation_entry_t* habituation_table;
    uint32_t habituation_count;
    uint32_t habituation_capacity;
    uint32_t habituation_evictions;   // Count of LRU evictions
    association_entry_t* association_table;
    uint32_t association_count;
    uint32_t association_capacity;
    uint32_t association_evictions;   // Count of LRU evictions
    float learning_rate;
    float forgetting_rate;
    uint64_t last_consolidation_ms;
    uint32_t consolidation_interval_ms;
    void* mutex;                   // Opaque mutex pointer
    void* inbox;                   // Bio-async inbox (opaque pointer for future use)
    void* bio_ctx;                 // Bio-async context for message routing
    bool is_initialized;
} portia_learning_state_t;

/**
 * Learning configuration
 */
typedef struct {
    portia_learning_mode_t allowed_modes;  // Bitmask of allowed modes
    uint32_t max_habituation_entries;
    uint32_t max_association_entries;
    float default_learning_rate;
    float default_forgetting_rate;
    uint32_t consolidation_interval_ms;
    float habituation_threshold;           // Minimum response strength
    float association_threshold;           // Minimum association strength
} portia_learning_config_t;

/**
 * Learning query result
 */
typedef struct {
    bool found;
    float strength;                // Response/association strength
    uint32_t exposure_count;       // How many times seen
    uint64_t last_update_ms;       // Last modification time
} portia_learning_query_result_t;

/**
 * Learning statistics
 */
typedef struct {
    uint32_t total_habituation_entries;
    uint32_t active_habituation_entries;
    uint32_t total_association_entries;
    uint32_t active_association_entries;
    uint32_t habituation_evictions;
    uint32_t association_evictions;
    uint64_t total_exposures;
    uint64_t total_reinforcements;
    float avg_habituation_strength;
    float avg_association_strength;
} portia_learning_stats_t;

/**
 * Initialize learning system
 */
portia_learning_state_t* portia_learning_init(const portia_learning_config_t* config);

/**
 * Destroy learning system
 */
void portia_learning_destroy(portia_learning_state_t* state);

/**
 * Set active learning mode
 */
int portia_learning_set_mode(portia_learning_state_t* state, portia_learning_mode_t mode);

/**
 * Process repeated stimulus (habituation)
 */
int portia_learning_habituate(portia_learning_state_t* state, uint32_t stimulus_id,
                               uint64_t timestamp_ms);

/**
 * Increase response to important stimulus (sensitization)
 */
int portia_learning_sensitize(portia_learning_state_t* state, uint32_t stimulus_id,
                               float boost_amount, uint64_t timestamp_ms);

/**
 * Create or strengthen association
 */
int portia_learning_associate(portia_learning_state_t* state, uint32_t stimulus_id,
                               uint32_t response_id, bool is_positive,
                               uint64_t timestamp_ms);

/**
 * Reinforce association (learning from outcome)
 */
int portia_learning_reinforce(portia_learning_state_t* state, uint32_t stimulus_id,
                               uint32_t response_id, float reward, uint64_t timestamp_ms);

/**
 * Query learned response strength
 */
portia_learning_query_result_t portia_learning_query(portia_learning_state_t* state,
                                                      uint32_t stimulus_id);

/**
 * Query association strength
 */
portia_learning_query_result_t portia_learning_query_association(
    portia_learning_state_t* state, uint32_t stimulus_id, uint32_t response_id);

/**
 * Apply forgetting to all entries
 */
int portia_learning_forget(portia_learning_state_t* state, uint64_t timestamp_ms);

/**
 * Consolidate memories (strengthen important, remove weak)
 */
int portia_learning_consolidate(portia_learning_state_t* state, uint64_t timestamp_ms);

/**
 * Get learning statistics
 */
portia_learning_stats_t portia_learning_get_stats(portia_learning_state_t* state);

/**
 * Reset all learning
 */
int portia_learning_reset(portia_learning_state_t* state);

/**
 * Export learning data (for inspection/debugging)
 */
int portia_learning_export(portia_learning_state_t* state, const char* filepath);

/**
 * Process bio-async messages
 */
int portia_learning_process_inbox(portia_learning_state_t* state);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PORTIA_LEARNING_H
