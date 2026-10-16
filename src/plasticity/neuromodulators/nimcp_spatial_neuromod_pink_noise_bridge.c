/*
 * DEPRECATED — STATUE (audit 2026-04-30)
 *
 * spatial_neuromod_pink_noise_create has zero callers in production
 * code. Wrapper around pink_noise_create that is unused. Either wire
 * a consumer or delete before the next major version. Do not extend.
 */

#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_spatial_neuromod_pink_noise_bridge.c - Implementation
//=============================================================================
/**
 * @file nimcp_spatial_neuromod_pink_noise_bridge.c
 * @brief Implementation of spatial pink noise neuromodulation bridge
 *
 * WHAT: Integrates spatially-correlated pink noise with spatial neuromodulation
 * WHY:  Inject biologically realistic stochastic dynamics into neuromodulator diffusion
 * HOW:  Generate regional pink noise, map to neurons, modulate diffusion parameters
 */

#include "plasticity/neuromodulators/nimcp_spatial_neuromod_pink_noise_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(spatial_neuromod_pink_noise_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(spatial_pink_bridge)


//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Compute Euclidean distance between two 3D points
 */
static inline float distance_3d(float x1, float y1, float z1,
                                 float x2, float y2, float z2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dz = z2 - z1;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

//=============================================================================
// Configuration
//=============================================================================

/**
 * WHAT: Returns default configuration for given neuromodulator type
 * WHY:  Different neuromodulators have different noise characteristics
 * HOW:  Set parameters based on biological literature
 */
spatial_pink_bridge_config_t spatial_pink_bridge_default_config(
    neuromodulator_type_t neuromod_type)
{
    spatial_pink_bridge_config_t config;
    memset(&config, 0, sizeof(config));

    // Get default pink spatial config
    config.pink_config = pink_spatial_default_config();

    // Common defaults
    config.modulation_mode = PINK_MOD_HYBRID;
    config.auto_map_regions_to_neurons = true;
    config.region_influence_radius = 20.0f;  // 20mm
    config.enabled = true;
    config.update_interval = 1;

    // Neuromodulator-specific defaults
    switch (neuromod_type) {
        case NEUROMOD_DOPAMINE:
            config.noise_amplitude = 0.10f;               // 10% noise
            config.diffusion_modulation_strength = 0.20f;
            config.decay_modulation_strength = 0.15f;
            break;

        case NEUROMOD_SEROTONIN:
            config.noise_amplitude = 0.08f;               // 8% noise (slower dynamics)
            config.diffusion_modulation_strength = 0.15f;
            config.decay_modulation_strength = 0.10f;
            break;

        case NEUROMOD_ACETYLCHOLINE:
            config.noise_amplitude = 0.12f;               // 12% noise (faster dynamics)
            config.diffusion_modulation_strength = 0.25f;
            config.decay_modulation_strength = 0.20f;
            break;

        case NEUROMOD_NOREPINEPHRINE:
            config.noise_amplitude = 0.10f;
            config.diffusion_modulation_strength = 0.18f;
            config.decay_modulation_strength = 0.12f;
            break;

        default:
            // Generic defaults
            config.noise_amplitude = 0.10f;
            config.diffusion_modulation_strength = 0.15f;
            config.decay_modulation_strength = 0.10f;
            break;
    }

    return config;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * WHAT: Creates spatial pink noise neuromodulation bridge
 * WHY:  Initialize bridge for noise injection into spatial neuromodulation
 * HOW:  Allocate buffers, create pink_spatial generator, initialize mappings
 */
spatial_pink_bridge_t* spatial_pink_bridge_create(
    const spatial_pink_bridge_config_t* config,
    uint32_t num_neurons)
{
    if (!config || num_neurons == 0) {
        NIMCP_LOGGING_ERROR("Invalid parameters for bridge creation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "spatial_pink_bridge_create: config is NULL");
        return NULL;
    }

    // Allocate bridge
    spatial_pink_bridge_t* bridge = (spatial_pink_bridge_t*)nimcp_malloc(
        sizeof(spatial_pink_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }
    memset(bridge, 0, sizeof(spatial_pink_bridge_t));

    // Copy configuration
    bridge->config = *config;

    // Create pink spatial generator
    bridge->pink_spatial = pink_spatial_create(&config->pink_config);
    if (!bridge->pink_spatial) {
        NIMCP_LOGGING_ERROR("Failed to create pink spatial generator");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "spatial_pink_bridge_create: bridge->pink_spatial is NULL");
        return NULL;
    }

    // Allocate neuron buffers
    size_t neuron_buffer_size = num_neurons * sizeof(float);

    bridge->current_noise_values = (float*)nimcp_malloc(neuron_buffer_size);
    bridge->diffusion_modulation = (float*)nimcp_malloc(neuron_buffer_size);
    bridge->decay_modulation = (float*)nimcp_malloc(neuron_buffer_size);
    bridge->neuron_to_region = (uint32_t*)nimcp_malloc(num_neurons * sizeof(uint32_t));
    bridge->neuron_region_weights = (float*)nimcp_malloc(neuron_buffer_size);

    if (!bridge->current_noise_values || !bridge->diffusion_modulation ||
        !bridge->decay_modulation || !bridge->neuron_to_region ||
        !bridge->neuron_region_weights) {
        NIMCP_LOGGING_ERROR("Failed to allocate neuron buffers");
        spatial_pink_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_pink_bridge_create: operation failed");
        return NULL;
    }

    // Initialize buffers
    memset(bridge->current_noise_values, 0, neuron_buffer_size);
    memset(bridge->diffusion_modulation, 0, neuron_buffer_size);
    memset(bridge->decay_modulation, 0, neuron_buffer_size);
    memset(bridge->neuron_to_region, 0, num_neurons * sizeof(uint32_t));

    // Initialize to 1.0 (no modulation), 1.0 (full weight)
    for (uint32_t i = 0; i < num_neurons; i++) {
        bridge->diffusion_modulation[i] = 1.0f;
        bridge->decay_modulation[i] = 1.0f;
        bridge->neuron_region_weights[i] = 1.0f;
    }

    // Initialize statistics
    bridge->update_count = 0;
    bridge->avg_noise_magnitude = 0.0f;
    bridge->max_noise_magnitude = 0.0f;
    bridge->noise_spatial_correlation = 0.0f;

    // State flags
    bridge->is_connected = false;
    bridge->auto_mapping_initialized = false;

    NIMCP_LOGGING_INFO("Created spatial pink noise bridge");
    return bridge;
}

/**
 * WHAT: Destroys spatial pink noise bridge
 * WHY:  Free all allocated memory
 * HOW:  Destroy pink_spatial, free buffers, clear region maps
 */
void spatial_pink_bridge_destroy(spatial_pink_bridge_t* bridge) {
    if (!bridge) return;

    // Destroy pink spatial generator
    if (bridge->pink_spatial) {
        pink_spatial_destroy(bridge->pink_spatial);
    }

    // Free neuron buffers
    if (bridge->current_noise_values) nimcp_free(bridge->current_noise_values);
    if (bridge->diffusion_modulation) nimcp_free(bridge->diffusion_modulation);
    if (bridge->decay_modulation) nimcp_free(bridge->decay_modulation);
    if (bridge->neuron_to_region) nimcp_free(bridge->neuron_to_region);
    if (bridge->neuron_region_weights) nimcp_free(bridge->neuron_region_weights);

    // Free region maps
    for (uint32_t i = 0; i < bridge->num_region_maps; i++) {
        if (bridge->region_maps[i].neuron_ids) {
            nimcp_free(bridge->region_maps[i].neuron_ids);
        }
    }

    // Free bridge
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed spatial pink noise bridge");
}

//=============================================================================
// Connection Functions
//=============================================================================

/**
 * WHAT: Connects bridge to spatial neuromodulator field
 * WHY:  Bridge needs reference to apply noise
 * HOW:  Store pointer, validate compatibility
 */
int spatial_pink_bridge_connect_neuromod(
    spatial_pink_bridge_t* bridge,
    spatial_neuromod_field_t* neuromod_field)
{
    if (!bridge || !neuromod_field) {
        NIMCP_LOGGING_ERROR("NULL pointer in connect_neuromod");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_pink_bridge_connect_neuromod: required parameter is NULL (bridge, neuromod_field)");
        return -1;
    }

    // Validate num_neurons matches
    if (neuromod_field->num_neurons == 0) {
        NIMCP_LOGGING_ERROR("Neuromod field has 0 neurons");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "spatial_pink_bridge_connect_neuromod: neuromod_field->num_neurons is zero");
        return -1;
    }

    // Connect
    bridge->neuromod_field = neuromod_field;
    bridge->is_connected = true;

    NIMCP_LOGGING_INFO("Connected bridge to neuromod field");
    return 0;
}

/**
 * WHAT: Disconnects bridge from neuromodulator field
 * WHY:  Allow dynamic enable/disable
 * HOW:  Clear pointer, set flag
 */
int spatial_pink_bridge_disconnect_neuromod(spatial_pink_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->neuromod_field = NULL;
    bridge->is_connected = false;

    NIMCP_LOGGING_INFO("Disconnected bridge from neuromod field");
    return 0;
}

/**
 * WHAT: Check if bridge is connected
 * WHY:  Validate before operations
 * HOW:  Check flag and pointer
 */
bool spatial_pink_bridge_is_connected(const spatial_pink_bridge_t* bridge) {
    return bridge && bridge->is_connected && bridge->neuromod_field != NULL;
}

//=============================================================================
// Region-Neuron Mapping
//=============================================================================

/**
 * WHAT: Adds brain region to pink noise generator
 * WHY:  Define spatial structure for noise
 * HOW:  Forward to pink_spatial_add_region
 */
int spatial_pink_bridge_add_region(
    spatial_pink_bridge_t* bridge,
    const char* name,
    float x, float y, float z,
    float alpha,
    float amplitude)
{
    BRIDGE_BBB_VALIDATE(bridge, name, sizeof(*name));
    if (!bridge || !bridge->pink_spatial) {
        NIMCP_LOGGING_ERROR("Invalid bridge in add_region");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_pink_bridge_add_region: required parameter is NULL (bridge, bridge->pink_spatial)");
        return -1;
    }

    int result = pink_spatial_add_region(
        bridge->pink_spatial,
        name, x, y, z, alpha, amplitude
    );

    if (result < 0) {
        NIMCP_LOGGING_ERROR("Failed to add region to pink spatial");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "spatial_pink_bridge_add_region: validation failed");
        return -1;
    }

    NIMCP_LOGGING_DEBUG("Added region to bridge");
    return result;  // Region index
}

/**
 * WHAT: Maps neuron to brain region
 * WHY:  Route regional noise to specific neurons
 * HOW:  Add neuron to region's neuron array, update mapping
 */
int spatial_pink_bridge_map_neuron_to_region(
    spatial_pink_bridge_t* bridge,
    uint32_t neuron_id,
    uint32_t region_index,
    float weight)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    // Validate region index
    if (region_index >= bridge->num_region_maps) {
        NIMCP_LOGGING_ERROR("Invalid region index in map_neuron");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "spatial_pink_bridge_map_neuron_to_region: capacity exceeded");
        return -1;
    }

    // Update neuron→region mapping
    bridge->neuron_to_region[neuron_id] = region_index;
    bridge->neuron_region_weights[neuron_id] = nimcp_clampf(weight, 0.0f, 1.0f);

    // Add to region's neuron list (simplified - reallocate if needed)
    // NOTE: For production, use dynamic array or pre-allocate
    region_neuron_map_t* map = &bridge->region_maps[region_index];
    map->num_neurons++;

    NIMCP_LOGGING_DEBUG("Mapped neuron to region");
    return 0;
}

/**
 * WHAT: Automatically maps neurons to nearest regions
 * WHY:  Convenient auto-setup
 * HOW:  For each neuron, find nearest region, assign with distance-based weight
 *
 * NOTE: Requires neural_network_t with neuron positions (stub implementation)
 */
int spatial_pink_bridge_auto_map_neurons(
    spatial_pink_bridge_t* bridge,
    neural_network_t network)
{
    if (!bridge || !network) {
        NIMCP_LOGGING_ERROR("Invalid parameters in auto_map_neurons");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_pink_bridge_auto_map_neurons: required parameter is NULL (bridge, network)");
        return -1;
    }

    if (!bridge->config.auto_map_regions_to_neurons) {
        NIMCP_LOGGING_WARN("Auto-mapping disabled in config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_pink_bridge_auto_map_neurons: bridge->config is NULL");
        return -1;
    }

    // Get number of regions
    uint32_t num_regions = bridge->pink_spatial->config.num_regions;
    if (num_regions == 0) {
        NIMCP_LOGGING_ERROR("No regions defined for auto-mapping");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "spatial_pink_bridge_auto_map_neurons: num_regions is zero");
        return -1;
    }

    // Get number of neurons from neuromod field
    if (!bridge->neuromod_field) {
        NIMCP_LOGGING_ERROR("Neuromod field not connected");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_pink_bridge_auto_map_neurons: bridge->neuromod_field is NULL");
        return -1;
    }
    uint32_t num_neurons = bridge->neuromod_field->num_neurons;

    // NOTE: Full implementation would:
    // 1. Get neuron positions from network
    // 2. For each neuron, compute distance to all regions
    // 3. Assign to nearest region with weight = exp(-distance/radius)
    //
    // Stub: Assign neurons uniformly to regions
    for (uint32_t i = 0; i < num_neurons; i++) {
        uint32_t region_idx = i % num_regions;  // Round-robin assignment
        bridge->neuron_to_region[i] = region_idx;
        bridge->neuron_region_weights[i] = 1.0f;
    }

    bridge->auto_mapping_initialized = true;
    NIMCP_LOGGING_INFO("Auto-mapped neurons to regions");
    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

/**
 * WHAT: Updates pink noise and applies to neuromodulator diffusion
 * WHY:  Inject stochastic variability into neuromodulator dynamics
 * HOW:  Generate noise, map to neurons, modulate diffusion parameters
 */
int spatial_pink_bridge_update(spatial_pink_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in update");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_pink_bridge_update: bridge is NULL");
        return -1;
    }

    // Check if enabled
    if (!bridge->config.enabled) {
        return 0;  // Silently skip if disabled
    }

    // Check update interval
    if (bridge->update_count % bridge->config.update_interval != 0) {
        bridge->update_count++;
        return 0;
    }

    // Check if connected
    if (!spatial_pink_bridge_is_connected(bridge)) {
        NIMCP_LOGGING_WARN("Bridge not connected in update");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "spatial_pink_bridge_update: spatial_pink_bridge_is_connected is NULL");
        return -1;
    }

    uint32_t num_neurons = bridge->neuromod_field->num_neurons;

    // Step 1: Generate regional pink noise
    int result = pink_spatial_step(bridge->pink_spatial);
    if (result < 0) {
        NIMCP_LOGGING_ERROR("Failed to step pink spatial");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "spatial_pink_bridge_update: validation failed");
        return -1;
    }

    // Step 2: Map regional noise to neurons
    float sum_noise = 0.0f;
    float max_noise = 0.0f;

    for (uint32_t i = 0; i < num_neurons; i++) {
        uint32_t region_idx = bridge->neuron_to_region[i];
        float region_noise = pink_spatial_get_region(bridge->pink_spatial, region_idx);
        float weight = bridge->neuron_region_weights[i];

        // Apply weight
        float neuron_noise = region_noise * weight;
        bridge->current_noise_values[i] = neuron_noise;

        // Update statistics
        float abs_noise = fabsf(neuron_noise);
        sum_noise += abs_noise;
        if (abs_noise > max_noise) {
            max_noise = abs_noise;
        }
    }

    // Update statistics
    bridge->avg_noise_magnitude = (num_neurons > 0) ? (sum_noise / num_neurons) : 0.0f;
    bridge->max_noise_magnitude = max_noise;

    // Step 3: Apply noise modulation
    result = spatial_pink_bridge_apply_modulation(bridge);
    if (result < 0) {
        NIMCP_LOGGING_ERROR("Failed to apply modulation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "spatial_pink_bridge_update: validation failed");
        return -1;
    }

    bridge->update_count++;

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/**
 * WHAT: Applies noise modulation to neuromodulator field
 * WHY:  Modify diffusion dynamics based on current noise
 * HOW:  Update diffusion_coeff, decay_rate, or concentration based on mode
 */
int spatial_pink_bridge_apply_modulation(spatial_pink_bridge_t* bridge) {
    if (!bridge || !bridge->neuromod_field) {
        NIMCP_LOGGING_ERROR("Invalid bridge in apply_modulation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_pink_bridge_apply_modulation: required parameter is NULL (bridge, bridge->neuromod_field)");
        return -1;
    }

    uint32_t num_neurons = bridge->neuromod_field->num_neurons;
    float noise_amp = bridge->config.noise_amplitude;
    float diffusion_strength = bridge->config.diffusion_modulation_strength;
    float decay_strength = bridge->config.decay_modulation_strength;

    switch (bridge->config.modulation_mode) {
        case PINK_MOD_ADDITIVE:
            // Add noise directly to concentration
            for (uint32_t i = 0; i < num_neurons; i++) {
                float noise = bridge->current_noise_values[i];
                float delta = noise_amp * noise;
                bridge->neuromod_field->concentration[i] += delta;
                // Clamp to valid range
                bridge->neuromod_field->concentration[i] = nimcp_clampf(
                    bridge->neuromod_field->concentration[i],
                    bridge->neuromod_field->min_concentration,
                    bridge->neuromod_field->max_concentration
                );
            }
            break;

        case PINK_MOD_MULTIPLICATIVE:
            // Modulate diffusion coefficient
            for (uint32_t i = 0; i < num_neurons; i++) {
                float noise = bridge->current_noise_values[i];
                float mod_factor = 1.0f + diffusion_strength * noise;
                bridge->diffusion_modulation[i] = nimcp_clampf(mod_factor, 0.5f, 2.0f);
            }
            // NOTE: Actual application to diffusion requires access to per-neuron
            // diffusion coefficients (not in current spatial_neuromod_field_t)
            break;

        case PINK_MOD_DECAY_RATE:
            // Modulate decay rate
            for (uint32_t i = 0; i < num_neurons; i++) {
                float noise = bridge->current_noise_values[i];
                float mod_factor = 1.0f + decay_strength * noise;
                bridge->decay_modulation[i] = nimcp_clampf(mod_factor, 0.5f, 2.0f);
            }
            break;

        case PINK_MOD_SOURCE_TERM:
            // Add to source term
            for (uint32_t i = 0; i < num_neurons; i++) {
                float noise = bridge->current_noise_values[i];
                float delta = noise_amp * noise;
                bridge->neuromod_field->source_rate[i] += delta;
                // Clamp source rate to positive
                if (bridge->neuromod_field->source_rate[i] < 0.0f) {
                    bridge->neuromod_field->source_rate[i] = 0.0f;
                }
            }
            break;

        case PINK_MOD_HYBRID:
            // Combine additive + multiplicative
            for (uint32_t i = 0; i < num_neurons; i++) {
                float noise = bridge->current_noise_values[i];

                // Additive component (50% weight)
                float delta = 0.5f * noise_amp * noise;
                bridge->neuromod_field->concentration[i] += delta;
                bridge->neuromod_field->concentration[i] = nimcp_clampf(
                    bridge->neuromod_field->concentration[i],
                    bridge->neuromod_field->min_concentration,
                    bridge->neuromod_field->max_concentration
                );

                // Multiplicative component (50% weight)
                float mod_factor = 1.0f + 0.5f * diffusion_strength * noise;
                bridge->diffusion_modulation[i] = nimcp_clampf(mod_factor, 0.5f, 2.0f);
            }
            break;

        default:
            NIMCP_LOGGING_ERROR("Unknown modulation mode");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "spatial_pink_bridge_apply_modulation: operation failed");
            return -1;
    }

    return 0;
}

//=============================================================================
// Enable/Disable
//=============================================================================

/**
 * WHAT: Enables pink noise injection
 * WHY:  Allow dynamic control
 * HOW:  Set config flag
 */
int spatial_pink_bridge_enable(spatial_pink_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->config.enabled = true;
    NIMCP_LOGGING_INFO("Enabled pink noise bridge");
    return 0;
}

/**
 * WHAT: Disables pink noise injection
 * WHY:  Preserve state but stop modulation
 * HOW:  Clear config flag
 */
int spatial_pink_bridge_disable(spatial_pink_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->config.enabled = false;
    NIMCP_LOGGING_INFO("Disabled pink noise bridge");
    return 0;
}

/**
 * WHAT: Check if bridge is enabled
 * WHY:  Query state
 * HOW:  Return config flag
 */
bool spatial_pink_bridge_is_enabled(const spatial_pink_bridge_t* bridge) {
    return bridge && bridge->config.enabled;
}

//=============================================================================
// Query Functions
//=============================================================================

/**
 * WHAT: Get current noise value at neuron
 * WHY:  Query stochastic component
 * HOW:  Array lookup
 */
float spatial_pink_bridge_get_noise(
    const spatial_pink_bridge_t* bridge,
    uint32_t neuron_id)
{
    if (!bridge || !bridge->current_noise_values) return 0.0f;
    if (neuron_id >= bridge->neuromod_field->num_neurons) return 0.0f;
    return bridge->current_noise_values[neuron_id];
}

/**
 * WHAT: Get diffusion modulation factor
 * WHY:  Query how diffusion is affected
 * HOW:  Array lookup
 */
float spatial_pink_bridge_get_diffusion_modulation(
    const spatial_pink_bridge_t* bridge,
    uint32_t neuron_id)
{
    if (!bridge || !bridge->diffusion_modulation) return 1.0f;
    if (neuron_id >= bridge->neuromod_field->num_neurons) return 1.0f;
    return bridge->diffusion_modulation[neuron_id];
}

/**
 * WHAT: Get decay modulation factor
 * WHY:  Query how decay is affected
 * HOW:  Array lookup
 */
float spatial_pink_bridge_get_decay_modulation(
    const spatial_pink_bridge_t* bridge,
    uint32_t neuron_id)
{
    if (!bridge || !bridge->decay_modulation) return 1.0f;
    if (neuron_id >= bridge->neuromod_field->num_neurons) return 1.0f;
    return bridge->decay_modulation[neuron_id];
}

/**
 * WHAT: Get bridge statistics
 * WHY:  Monitor noise characteristics
 * HOW:  Return cached statistics
 */
int spatial_pink_bridge_get_stats(
    const spatial_pink_bridge_t* bridge,
    float* avg_noise,
    float* max_noise,
    float* spatial_correlation)
{
    BRIDGE_BBB_VALIDATE(bridge, avg_noise, sizeof(*avg_noise));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    if (avg_noise) *avg_noise = bridge->avg_noise_magnitude;
    if (max_noise) *max_noise = bridge->max_noise_magnitude;
    if (spatial_correlation) *spatial_correlation = bridge->noise_spatial_correlation;

    return 0;
}

//=============================================================================
// Reset & Debugging
//=============================================================================

/**
 * WHAT: Reset pink noise generator with new seed
 * WHY:  Restart exploration with different random sequence
 * HOW:  Forward to pink_spatial_reset
 */
int spatial_pink_bridge_reset(spatial_pink_bridge_t* bridge, uint32_t new_seed) {
    if (!bridge || !bridge->pink_spatial) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_pink_bridge_reset: required parameter is NULL (bridge, bridge->pink_spatial)");
        return -1;
    }

    int result = pink_spatial_reset(bridge->pink_spatial, new_seed);
    if (result < 0) {
        NIMCP_LOGGING_ERROR("Failed to reset pink spatial");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "spatial_pink_bridge_reset: validation failed");
        return -1;
    }

    // Reset statistics
    bridge->update_count = 0;
    bridge->avg_noise_magnitude = 0.0f;
    bridge->max_noise_magnitude = 0.0f;

    NIMCP_LOGGING_INFO("Reset pink noise bridge");
    return 0;
}

/**
 * WHAT: Validate bridge state
 * WHY:  Catch errors early
 * HOW:  Check invariants
 */
bool spatial_pink_bridge_validate(const spatial_pink_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_pink_bridge_validate: bridge is NULL");
        return false;
    }

    // Check pointers
    if (!bridge->pink_spatial) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_pink_bridge_validate: bridge->pink_spatial is NULL");
        return false;
    }
    if (!bridge->current_noise_values) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_pink_bridge_validate: bridge->current_noise_values is NULL");
        return false;
    }
    if (!bridge->diffusion_modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_pink_bridge_validate: bridge->diffusion_modulation is NULL");
        return false;
    }
    if (!bridge->decay_modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_pink_bridge_validate: bridge->decay_modulation is NULL");
        return false;
    }

    // Check if connected
    if (bridge->is_connected && !bridge->neuromod_field) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_pink_bridge_validate: bridge->neuromod_field is NULL");
        return false;
    }

    // Check configuration
    if (bridge->config.noise_amplitude < 0.0f || bridge->config.noise_amplitude > 1.0f) {
        return false;
    }

    // Check noise values for NaN/Inf
    if (bridge->neuromod_field) {
        uint32_t num_neurons = bridge->neuromod_field->num_neurons;
        for (uint32_t i = 0; i < num_neurons; i++) {
            if (isnan(bridge->current_noise_values[i]) ||
                isinf(bridge->current_noise_values[i])) {
                return false;
            }
        }
    }

    return true;
}

//=============================================================================
// Unified Memory Integration
//=============================================================================

/**
 * WHAT: Connect unified memory manager
 * WHY:  Enable CoW for buffers
 * HOW:  Store manager, migrate allocations
 */
int spatial_pink_bridge_connect_memory_manager(
    spatial_pink_bridge_t* bridge,
    unified_mem_manager_t mem_manager)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->mem_manager = mem_manager;

    // Also connect to pink_spatial
    if (bridge->pink_spatial) {
        pink_spatial_connect_memory_manager(bridge->pink_spatial, mem_manager);
    }

    NIMCP_LOGGING_INFO("Connected UMM to pink noise bridge");
    return 0;
}

/**
 * WHAT: Check if UMM connected
 * WHY:  Query memory mode
 * HOW:  Check manager pointer
 */
bool spatial_pink_bridge_has_memory_manager(const spatial_pink_bridge_t* bridge) {
    return bridge && bridge->mem_manager != NULL;
}
