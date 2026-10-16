#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_lnn_config.c - LNN Configuration Implementation
//=============================================================================
/**
 * @file nimcp_lnn_config.c
 * @brief Implementation of LNN configuration functions
 *
 * WHAT: Configuration initialization, validation, and NCP builder
 * WHY:  Provides sensible defaults and validates user configurations
 * HOW:  Guard clauses, helper functions, WHAT/WHY/HOW documentation
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include "lnn/nimcp_lnn_config.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_learning_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(lnn_config)

//=============================================================================
// Default Constants
//=============================================================================

#define LNN_DEFAULT_TAU_BASE_MS       10.0f   /**< Baseline time constant (ms) */
#define LNN_DEFAULT_TAU_MIN_MS        0.1f    /**< Minimum time constant (ms) */
#define LNN_DEFAULT_TAU_MAX_MS        1000.0f /**< Maximum time constant (ms) */
#define LNN_DEFAULT_WEIGHT_INIT_STD   0.1f    /**< Weight init std dev */
#define LNN_DEFAULT_DT_MS             1.0f    /**< Default integration step (ms) */
#define LNN_DEFAULT_ADAPTIVE_DT_MIN   0.01f   /**< Min adaptive dt (ms) */
#define LNN_DEFAULT_ADAPTIVE_DT_MAX   10.0f   /**< Max adaptive dt (ms) */
#define LNN_DEFAULT_ADAPTIVE_TOL      1e-5f   /**< Adaptive error tolerance */
#define LNN_DEFAULT_BPTT_TRUNCATION   100     /**< BPTT truncation length */
#define LNN_DEFAULT_CHECKPOINT_INTERVAL 10    /**< Checkpoint every N steps */
#define LNN_DEFAULT_GRADIENT_CLIP     NIMCP_GRADIENT_CLIP_DEFAULT    /**< Gradient clipping threshold */
#define LNN_DEFAULT_INSTABILITY_THRESH 1e6f   /**< State explosion threshold */

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Initialize neuron config with defaults
 *
 * WHAT: Set default neuron parameters
 * WHY:  Reusable across different config builders
 * HOW:  Set tanh activation, 10ms tau, learnable time constants
 */
static void lnn_neuron_config_default(lnn_neuron_config_t* nc) {
    nc->activation = LNN_ACTIVATION_TANH;
    nc->tau_base_init = LNN_DEFAULT_TAU_BASE_MS;
    nc->tau_min = LNN_DEFAULT_TAU_MIN_MS;
    nc->tau_max = LNN_DEFAULT_TAU_MAX_MS;
    nc->weight_init_std = LNN_DEFAULT_WEIGHT_INIT_STD;
    nc->learn_tau = true;
}

/**
 * @brief Initialize layer config with defaults
 *
 * WHAT: Set default layer parameters
 * WHY:  Reusable for creating layer configs
 * HOW:  Set RK4 solver, 1ms dt, full connectivity
 */
static void lnn_layer_config_default(lnn_layer_config_t* lc, uint32_t n_neurons) {
    lc->n_neurons = n_neurons;

    /* Set neuron-level defaults (flat structure) */
    lc->activation = LNN_ACTIVATION_TANH;
    lc->tau_base_init = LNN_DEFAULT_TAU_BASE_MS;
    lc->tau_min = LNN_DEFAULT_TAU_MIN_MS;
    lc->tau_max = LNN_DEFAULT_TAU_MAX_MS;
    lc->learn_tau = true;
    lc->weight_init_std = 0.1f;
    lc->seed = 0;

    /* Layer-level settings */
    lc->wiring_type = LNN_WIRING_FULL;
    lc->sparsity = 0.0f;
    lc->ode_method = LNN_ODE_RK4;
    lc->dt = LNN_DEFAULT_DT_MS;
    lc->use_layer_norm = true;
    lc->layer_norm_eps = 1e-5f;
}

/**
 * @brief Validate neuron config
 *
 * WHAT: Check neuron config for invalid values
 * WHY:  Catch configuration errors early
 * HOW:  Validate tau bounds, weight std
 */
static int lnn_neuron_config_validate(const lnn_neuron_config_t* nc) {
    /* Guard: Check tau bounds */
    if (nc->tau_min <= 0.0f || nc->tau_max <= 0.0f) {
        NIMCP_LOGGING_ERROR("tau_min and tau_max must be positive");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "Neuron config tau_min (%f) and tau_max (%f) must be positive",
                             nc->tau_min, nc->tau_max);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (nc->tau_min >= nc->tau_max) {
        NIMCP_LOGGING_ERROR("tau_min (%f) must be less than tau_max (%f)",
                           nc->tau_min, nc->tau_max);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "Neuron config tau_min (%f) >= tau_max (%f)",
                             nc->tau_min, nc->tau_max);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (nc->tau_base_init < nc->tau_min || nc->tau_base_init > nc->tau_max) {
        NIMCP_LOGGING_WARN("tau_base_init (%f) outside [tau_min, tau_max], will be clamped",
                          nc->tau_base_init);
    }

    /* Guard: Check weight initialization */
    if (nc->weight_init_std <= 0.0f) {
        NIMCP_LOGGING_ERROR("weight_init_std must be positive");
        NIMCP_THROW_BRAIN(NIMCP_ERROR_WEIGHT_INIT, 0, "LNN",
                         "Neuron config weight_init_std (%f) must be positive",
                         nc->weight_init_std);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Guard: Check activation function */
    if (nc->activation < 0 || nc->activation >= LNN_ACTIVATION_COUNT) {
        NIMCP_LOGGING_ERROR("Invalid activation function: %d", nc->activation);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "Invalid activation function: %d", nc->activation);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Validate layer config
 *
 * WHAT: Check layer config for invalid values
 * WHY:  Catch configuration errors early
 * HOW:  Validate dimensions, sparsity, ODE settings
 */
static int lnn_layer_config_validate(const lnn_layer_config_t* lc) {
    /* Guard: Check neuron count */
    if (lc->n_neurons == 0) {
        NIMCP_LOGGING_ERROR("Layer must have at least one neuron");
        NIMCP_THROW_BRAIN(NIMCP_ERROR_DIMENSION_MISMATCH, 0, "LNN",
                         "Layer config requires at least one neuron");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Guard: Check tau bounds (flat structure) */
    if (lc->tau_min <= 0.0f || lc->tau_max <= 0.0f) {
        NIMCP_LOGGING_ERROR("tau_min and tau_max must be positive");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "Layer config tau_min (%f) and tau_max (%f) must be positive",
                             lc->tau_min, lc->tau_max);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (lc->tau_min >= lc->tau_max) {
        NIMCP_LOGGING_ERROR("tau_min (%f) must be less than tau_max (%f)",
                           lc->tau_min, lc->tau_max);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "Layer config tau_min (%f) >= tau_max (%f)",
                             lc->tau_min, lc->tau_max);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Guard: Check weight initialization */
    if (lc->weight_init_std <= 0.0f) {
        NIMCP_LOGGING_ERROR("weight_init_std must be positive");
        NIMCP_THROW_BRAIN(NIMCP_ERROR_WEIGHT_INIT, 0, "LNN",
                         "Layer config weight_init_std (%f) must be positive",
                         lc->weight_init_std);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Guard: Check activation function */
    if (lc->activation < 0 || lc->activation >= LNN_ACTIVATION_COUNT) {
        NIMCP_LOGGING_ERROR("Invalid activation function: %d", lc->activation);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "Invalid layer config activation function: %d", lc->activation);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Guard: Check sparsity */
    if (lc->sparsity < 0.0f || lc->sparsity >= 1.0f) {
        NIMCP_LOGGING_ERROR("Sparsity must be in [0, 1), got %f", lc->sparsity);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "Invalid sparsity %f, must be in [0, 1)", lc->sparsity);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Guard: Check wiring type */
    if (lc->wiring_type < 0 || lc->wiring_type >= LNN_WIRING_COUNT) {
        NIMCP_LOGGING_ERROR("Invalid wiring type: %d", lc->wiring_type);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "Invalid wiring type: %d", lc->wiring_type);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Guard: Check ODE method */
    if (lc->ode_method < 0 || lc->ode_method >= LNN_ODE_METHOD_COUNT) {
        NIMCP_LOGGING_ERROR("Invalid ODE method: %d", lc->ode_method);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "Invalid ODE method: %d", lc->ode_method);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Guard: Check time step */
    if (lc->dt <= 0.0f) {
        NIMCP_LOGGING_ERROR("Integration time step must be positive, got %f", lc->dt);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "Integration time step must be positive, got %f", lc->dt);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Public API
//=============================================================================

int lnn_config_default(lnn_config_t* config) {
    /* Guard: NULL check */
    if (!config) {
        NIMCP_LOGGING_ERROR("Config pointer is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null config pointer in lnn_config_default");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Zero out the structure */
    memset(config, 0, sizeof(lnn_config_t));

    /* Architecture defaults (no layers allocated yet) */
    config->n_layers = 0;
    config->layer_configs = NULL;
    config->n_inputs = 0;
    config->n_outputs = 0;

    /* ODE settings */
    config->default_ode_method = LNN_ODE_RK4;
    config->default_dt = LNN_DEFAULT_DT_MS;
    config->adaptive_dt_min = LNN_DEFAULT_ADAPTIVE_DT_MIN;
    config->adaptive_dt_max = LNN_DEFAULT_ADAPTIVE_DT_MAX;
    config->adaptive_error_tol = LNN_DEFAULT_ADAPTIVE_TOL;

    /* Training defaults */
    config->train_mode = LNN_TRAIN_ADJOINT;
    config->bptt_truncation = LNN_DEFAULT_BPTT_TRUNCATION;
    config->use_gradient_checkpointing = false;
    config->checkpoint_interval = LNN_DEFAULT_CHECKPOINT_INTERVAL;
    config->gradient_clip_norm = LNN_DEFAULT_GRADIENT_CLIP;

    /* NCP defaults (all zero - not NCP mode by default) */
    config->ncp_sensory = 0;
    config->ncp_inter = 0;
    config->ncp_command = 0;
    config->ncp_motor = 0;

    /* Parallelization defaults */
    config->n_threads = 0;  /* Auto-detect */
    config->enable_simd = true;

    /* Bio-async defaults */
    config->enable_bio_async = false;
    config->bio_module_id = 0;

    /* Immune defaults */
    config->enable_immune_integration = false;
    config->instability_threshold = LNN_DEFAULT_INSTABILITY_THRESH;

    /* Logging defaults */
    config->enable_logging = false;
    config->log_level = 1;  /* WARN level */

    /* Memory defaults */
    config->max_memory_bytes = 0;  /* Unlimited */
    config->preallocate_history = false;

    if (config->enable_logging) {
        NIMCP_LOGGING_INFO("LNN config initialized with defaults");
    }

    return NIMCP_SUCCESS;
}

int lnn_config_ncp(lnn_config_t* config,
                   uint32_t n_inputs,
                   uint32_t n_inter,
                   uint32_t n_command,
                   uint32_t n_outputs) {
    /* Guard: NULL check */
    if (!config) {
        NIMCP_LOGGING_ERROR("Config pointer is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null config pointer in lnn_config_ncp");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Guard: Validate dimensions */
    if (n_inputs == 0 || n_outputs == 0) {
        NIMCP_LOGGING_ERROR("n_inputs and n_outputs must be non-zero");
        NIMCP_THROW_BRAIN(NIMCP_ERROR_DIMENSION_MISMATCH, 0, "LNN",
                         "NCP config requires n_inputs (%u) and n_outputs (%u) > 0",
                         n_inputs, n_outputs);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (n_inter == 0 || n_command == 0) {
        NIMCP_LOGGING_ERROR("n_inter and n_command must be non-zero for NCP");
        NIMCP_THROW_BRAIN(NIMCP_ERROR_DIMENSION_MISMATCH, 0, "LNN",
                         "NCP config requires n_inter (%u) and n_command (%u) > 0",
                         n_inter, n_command);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Initialize with defaults first */
    int result = lnn_config_default(config);
    if (result != NIMCP_SUCCESS) {
        return result;
    }

    /* Set NCP-specific architecture */
    config->n_inputs = n_inputs;
    config->n_outputs = n_outputs;
    config->n_layers = 4;  /* Sensory, Inter, Command, Motor */

    /* Store NCP neuron counts */
    config->ncp_sensory = n_inputs;
    config->ncp_inter = n_inter;
    config->ncp_command = n_command;
    config->ncp_motor = n_outputs;

    /* Allocate layer configs */
    config->layer_configs = (lnn_layer_config_t*)nimcp_calloc(
        config->n_layers, sizeof(lnn_layer_config_t));

    if (!config->layer_configs) {
        NIMCP_LOGGING_ERROR("Failed to allocate layer configs");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                          config->n_layers * sizeof(lnn_layer_config_t),
                          "Failed to allocate %u layer configs for NCP",
                          config->n_layers);
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Initialize each layer with NCP wiring */
    lnn_layer_config_default(&config->layer_configs[0], n_inputs);
    config->layer_configs[0].wiring_type = LNN_WIRING_NCP;
    config->layer_configs[0].sparsity = 0.3f;  /* Sparse sensory layer */

    lnn_layer_config_default(&config->layer_configs[1], n_inter);
    config->layer_configs[1].wiring_type = LNN_WIRING_NCP;
    config->layer_configs[1].sparsity = 0.5f;  /* Sparser interneurons */

    lnn_layer_config_default(&config->layer_configs[2], n_command);
    config->layer_configs[2].wiring_type = LNN_WIRING_NCP;
    config->layer_configs[2].sparsity = 0.3f;  /* Sparse command layer */

    lnn_layer_config_default(&config->layer_configs[3], n_outputs);
    config->layer_configs[3].wiring_type = LNN_WIRING_NCP;
    config->layer_configs[3].sparsity = 0.0f;  /* Dense motor output */

    if (config->enable_logging) {
        NIMCP_LOGGING_INFO("Created NCP config: %u inputs → %u inter → %u command → %u outputs",
                          n_inputs, n_inter, n_command, n_outputs);
    }

    return NIMCP_SUCCESS;
}

int lnn_config_validate(const lnn_config_t* config) {
    /* Guard: NULL check */
    if (!config) {
        NIMCP_LOGGING_ERROR("Config pointer is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null config pointer in lnn_config_validate");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Guard: Check layer count */
    if (config->n_layers == 0) {
        NIMCP_LOGGING_ERROR("Network must have at least one layer");
        NIMCP_THROW_BRAIN(NIMCP_ERROR_NETWORK_CREATION, 0, "LNN",
                         "Network config must have at least one layer");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Guard: Check layer configs allocated */
    if (!config->layer_configs) {
        NIMCP_LOGGING_ERROR("layer_configs is NULL but n_layers = %u", config->n_layers);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "layer_configs is NULL but n_layers = %u", config->n_layers);
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Guard: Check input/output dimensions */
    if (config->n_inputs == 0 || config->n_outputs == 0) {
        NIMCP_LOGGING_ERROR("n_inputs and n_outputs must be non-zero");
        NIMCP_THROW_BRAIN(NIMCP_ERROR_DIMENSION_MISMATCH, 0, "LNN",
                         "n_inputs (%u) and n_outputs (%u) must be non-zero",
                         config->n_inputs, config->n_outputs);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Validate ODE settings */
    if (config->default_ode_method < 0 || config->default_ode_method >= LNN_ODE_METHOD_COUNT) {
        NIMCP_LOGGING_ERROR("Invalid default ODE method: %d", config->default_ode_method);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "Invalid default ODE method: %d", config->default_ode_method);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (config->default_dt <= 0.0f) {
        NIMCP_LOGGING_ERROR("default_dt must be positive, got %f", config->default_dt);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "default_dt must be positive, got %f", config->default_dt);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (config->adaptive_dt_min <= 0.0f || config->adaptive_dt_max <= 0.0f) {
        NIMCP_LOGGING_ERROR("Adaptive dt bounds must be positive");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "Adaptive dt bounds must be positive (min=%f, max=%f)",
                             config->adaptive_dt_min, config->adaptive_dt_max);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (config->adaptive_dt_min >= config->adaptive_dt_max) {
        NIMCP_LOGGING_ERROR("adaptive_dt_min must be less than adaptive_dt_max");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "adaptive_dt_min (%f) must be < adaptive_dt_max (%f)",
                             config->adaptive_dt_min, config->adaptive_dt_max);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (config->adaptive_error_tol <= 0.0f) {
        NIMCP_LOGGING_ERROR("adaptive_error_tol must be positive");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "adaptive_error_tol must be positive, got %f",
                             config->adaptive_error_tol);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Validate training settings */
    if (config->train_mode < 0 || config->train_mode >= LNN_TRAIN_MODE_COUNT) {
        NIMCP_LOGGING_ERROR("Invalid training mode: %d", config->train_mode);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "Invalid training mode: %d", config->train_mode);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (config->bptt_truncation == 0) {
        NIMCP_LOGGING_WARN("BPTT truncation is 0 - will not backpropagate through time");
    }

    if (config->gradient_clip_norm <= 0.0f) {
        NIMCP_LOGGING_ERROR("gradient_clip_norm must be positive");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "gradient_clip_norm must be positive, got %f",
                             config->gradient_clip_norm);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (config->use_gradient_checkpointing && config->checkpoint_interval == 0) {
        NIMCP_LOGGING_ERROR("checkpoint_interval must be non-zero when checkpointing enabled");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "checkpoint_interval must be non-zero when checkpointing enabled");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Validate NCP settings (if NCP mode) */
    bool is_ncp = (config->ncp_sensory > 0 || config->ncp_inter > 0 ||
                   config->ncp_command > 0 || config->ncp_motor > 0);

    if (is_ncp) {
        if (config->ncp_sensory == 0 || config->ncp_inter == 0 ||
            config->ncp_command == 0 || config->ncp_motor == 0) {
            NIMCP_LOGGING_ERROR("All NCP neuron counts must be non-zero in NCP mode");
            NIMCP_THROW_BRAIN(NIMCP_ERROR_NETWORK_CREATION, 0, "LNN",
                             "NCP mode requires all neuron counts > 0");
            return NIMCP_ERROR_INVALID_PARAM;
        }

        /* Verify layer count matches NCP architecture */
        if (config->n_layers != 4) {
            NIMCP_LOGGING_ERROR("NCP mode requires exactly 4 layers, got %u", config->n_layers);
            NIMCP_THROW_BRAIN(NIMCP_ERROR_NETWORK_CREATION, 0, "LNN",
                             "NCP mode requires exactly 4 layers, got %u", config->n_layers);
            return NIMCP_ERROR_INVALID_PARAM;
        }

        /* Verify neuron counts match */
        if (config->layer_configs[0].n_neurons != config->ncp_sensory ||
            config->layer_configs[1].n_neurons != config->ncp_inter ||
            config->layer_configs[2].n_neurons != config->ncp_command ||
            config->layer_configs[3].n_neurons != config->ncp_motor) {
            NIMCP_LOGGING_ERROR("Layer neuron counts don't match NCP specification");
            NIMCP_THROW_BRAIN(NIMCP_ERROR_DIMENSION_MISMATCH, 0, "LNN",
                             "Layer neuron counts don't match NCP specification");
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate parallelization */
    if (config->n_threads > 1024) {
        NIMCP_LOGGING_WARN("n_threads = %u is very high, may cause overhead", config->n_threads);
    }

    /* Validate immune settings */
    if (config->enable_immune_integration && config->instability_threshold <= 0.0f) {
        NIMCP_LOGGING_ERROR("instability_threshold must be positive");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "instability_threshold must be positive, got %f",
                             config->instability_threshold);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Validate each layer */
    for (uint32_t i = 0; i < config->n_layers; i++) {
        int result = lnn_layer_config_validate(&config->layer_configs[i]);
        if (result != NIMCP_SUCCESS) {
            NIMCP_LOGGING_ERROR("Layer %u validation failed", i);
            return result;
        }
    }

    if (config->enable_logging) {
        NIMCP_LOGGING_INFO("LNN config validation passed: %u layers, %u inputs, %u outputs",
                          config->n_layers, config->n_inputs, config->n_outputs);
    }

    return NIMCP_SUCCESS;
}

void lnn_config_destroy(lnn_config_t* config) {
    /* Guard: NULL check (safe to call multiple times) */
    if (!config) {
        return;
    }

    /* Free layer configs if allocated */
    if (config->layer_configs) {
        nimcp_free(config->layer_configs);
        config->layer_configs = NULL;
    }

    /* Zero out the structure */
    memset(config, 0, sizeof(lnn_config_t));

    /* Note: No logging here since this may be called during cleanup */
}
