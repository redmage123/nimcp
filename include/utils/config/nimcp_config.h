/**
 * @file nimcp_config.h
 * @brief NIMCP Configuration file parser (YAML/JSON)
 * @version 2.6.1
 *
 * Supports loading brain configurations from YAML or JSON files.
 */

#ifndef NIMCP_CONFIG_H
#define NIMCP_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Brain configuration structure
 */
typedef struct {
    // Basic settings
    char name[128];
    int size;           // nimcp_brain_size_t
    int task;           // nimcp_brain_task_t

    // Architecture
    uint32_t num_inputs;
    uint32_t num_outputs;
    uint32_t num_hidden;
    float learning_rate;

    // Training parameters
    uint32_t max_epochs;
    uint32_t batch_size;
    float validation_split;
    bool early_stopping;
    uint32_t patience;

    // Plasticity settings
    bool enable_bcm;
    float bcm_tau;
    bool enable_stdp;
    float stdp_window;

    // Ethics
    bool ethics_enabled;
    float golden_rule_threshold;
    float empathy_weight;

    // Model persistence
    char model_path[256];
    uint32_t checkpoint_interval;
} nimcp_brain_config_t;

/**
 * @brief Parse brain configuration from YAML file
 * @param filepath Path to YAML config file
 * @param config Output configuration structure
 * @return true on success, false on error
 */
bool nimcp_config_load_yaml(const char* filepath, nimcp_brain_config_t* config);

/**
 * @brief Parse brain configuration from JSON file
 * @param filepath Path to JSON config file
 * @param config Output configuration structure
 * @return true on success, false on error
 */
bool nimcp_config_load_json(const char* filepath, nimcp_brain_config_t* config);

/**
 * @brief Auto-detect format and load configuration
 * @param filepath Path to config file (.yaml, .yml, or .json)
 * @param config Output configuration structure
 * @return true on success, false on error
 */
bool nimcp_config_load(const char* filepath, nimcp_brain_config_t* config);

/**
 * @brief Initialize config with default values
 * @param config Configuration to initialize
 */
void nimcp_config_init_defaults(nimcp_brain_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CONFIG_H */
