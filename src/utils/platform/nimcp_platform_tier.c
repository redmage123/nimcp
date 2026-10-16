#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_platform_tier.c - Platform Tier System Implementation
//=============================================================================
/**
 * @file nimcp_platform_tier.c
 * @brief Platform tier detection and configuration implementation
 *
 * WHAT: Automatic platform classification and tier-specific configuration
 * WHY:  Enable NIMCP to run on diverse hardware (server → IoT/MCU)
 * HOW:  Detect system resources and apply tier-appropriate constraints
 *
 * IMPLEMENTATION NOTES:
 * - Tier configs are statically defined for fast access
 * - Detection uses system_resources_query() for accurate hardware info
 * - Conservative defaults to prevent OOM on constrained devices
 * - Modular design allows easy addition of new tiers
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include "utils/platform/nimcp_platform_tier.h"
#include "utils/platform/nimcp_system_resources.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(platform_tier)

//=============================================================================
// Tier Detection Thresholds
//=============================================================================

/**
 * WHAT: Hardware thresholds for tier classification
 * WHY:  Clear, conservative boundaries for automatic detection
 * HOW:  Based on typical hardware capabilities and NIMCP resource needs
 *
 * NOTE: Standard tiers are auto-detected. Specialized tiers (NEUROMORPHIC,
 *       QUANTUM) must be explicitly configured - they are not auto-detected.
 */

// FULL tier: High-end workstation/server
#define TIER_FULL_MIN_RAM_MB        8192    // 8GB RAM
#define TIER_FULL_MIN_CORES         8       // 8 cores

// MEDIUM tier: Laptop/tablet/dev board
#define TIER_MEDIUM_MIN_RAM_MB      2048    // 2GB RAM
#define TIER_MEDIUM_MIN_CORES       4       // 4 cores

// CONSTRAINED tier: Phone/drone/embedded
#define TIER_CONSTRAINED_MIN_RAM_MB 256     // 256MB RAM
#define TIER_CONSTRAINED_MIN_CORES  2       // 2 cores

// MINIMAL tier: IoT/MCU (64-256MB RAM)
#define TIER_MINIMAL_MIN_RAM_MB     64      // 64MB RAM

// BASIC tier: Arduino/ESP32/ultra-constrained MCU (below 64MB)
// No minimums - anything that doesn't meet MINIMAL goes here

//=============================================================================
// Pre-configured Tier Settings
//=============================================================================

/**
 * WHAT: Statically defined configurations for each tier
 * WHY:  Fast lookups, no dynamic allocation, cache-friendly
 * HOW:  Carefully tuned defaults based on hardware class
 *
 * TUNING PHILOSOPHY:
 * - Conservative memory budgets (leave room for OS, other processes)
 * - Progressive feature reduction (graceful degradation)
 * - Essential modules prioritized on constrained platforms
 * - Visual/audio configs scale with available compute
 */

/**
 * Standard tier configurations (BASIC through FULL)
 * NOTE: Array indexed by platform_tier_t enum values
 *       Specialized tiers (NEUROMORPHIC, QUANTUM) have separate configs
 */
static const platform_tier_config_t TIER_CONFIGS[PLATFORM_TIER_COUNT] = {
    // ========================================================================
    // [0] PLATFORM_TIER_BASIC - Arduino/ESP32/ultra-constrained MCU
    // ========================================================================
    {
        .tier = PLATFORM_TIER_BASIC,

        // Core neural network - Ultra-tiny (sub-Portia)
        .max_neurons = 100,                  // 100 neurons maximum
        .max_synapses_per_neuron = 5,        // Extremely sparse
        .initial_neurons = 50,               // Start with 50 neurons

        // Cognitive modules - NONE (pure reactive only)
        .cognitive_modules_enabled = COGNITIVE_MODULE_NONE,

        // Visual cortex - Bare minimum (if any)
        .visual = {
            .max_input_width = 32,
            .max_input_height = 24,
            .num_filters_conv1 = 4,
            .num_filters_conv2 = 0,          // No second conv layer
            .max_feature_maps = 8,
            .enable_pooling = false,
            .enable_attention = false,
        },

        // Audio cortex - Disabled
        .audio = {
            .max_sample_rate = 0,            // No audio processing
            .num_mel_filters = 0,
            .num_mfcc = 0,
            .frame_size = 0,
            .enable_attention = false,
            .enable_memory = false,
        },

        // Resource budgets
        .memory_budget_mb = 8,               // 8MB budget (some MCUs have only 32KB!)
        .compute_budget_ops = 1000000ULL,    // 1 MFLOPS
        .max_threads = 1,

        // Feature flags - Absolutely nothing optional
        .enable_gpu = false,
        .enable_bio_async = false,
        .enable_plasticity = false,
        .enable_neuromodulation = false,
        .enable_checkpointing = false,

        // Performance tuning
        .update_batch_size = 10,
        .spike_buffer_size = 100,
        .sampling_rate = 0.1F,               // 10% sampling
    },

    // ========================================================================
    // [1] PLATFORM_TIER_MINIMAL - IoT/MCU/ultra-constrained
    // ========================================================================
    {
        .tier = PLATFORM_TIER_MINIMAL,

        // Core neural network - Tiny brain
        .max_neurons = 1000,                 // 1K neurons (Portia-scale!)
        .max_synapses_per_neuron = 10,       // Very sparse
        .initial_neurons = 100,              // Start with 100 neurons

        // Cognitive modules - Bare minimum
        .cognitive_modules_enabled =
            COGNITIVE_MODULE_ATTENTION |     // Basic attention only
            COGNITIVE_MODULE_VISUAL_CORTEX,  // Minimal vision

        // Visual cortex - Minimal resolution
        .visual = {
            .max_input_width = 64,
            .max_input_height = 48,
            .num_filters_conv1 = 8,
            .num_filters_conv2 = 16,
            .max_feature_maps = 32,
            .enable_pooling = false,         // No pooling
            .enable_attention = false,
        },

        // Audio cortex - Disabled (too expensive)
        .audio = {
            .max_sample_rate = 8000,         // Minimal
            .num_mel_filters = 16,
            .num_mfcc = 8,
            .frame_size = 256,
            .enable_attention = false,
            .enable_memory = false,
        },

        // Resource budgets
        .memory_budget_mb = 32,              // 32MB budget
        .compute_budget_ops = 10000000ULL,   // 10 MFLOPS
        .max_threads = 2,

        // Feature flags - Absolutely minimal
        .enable_gpu = false,
        .enable_bio_async = false,           // Too expensive
        .enable_plasticity = false,          // No learning
        .enable_neuromodulation = false,
        .enable_checkpointing = false,

        // Performance tuning
        .update_batch_size = 50,
        .spike_buffer_size = 1000,
        .sampling_rate = 0.25F,              // 25% sampling
    },

    // ========================================================================
    // [2] PLATFORM_TIER_CONSTRAINED - Phone/drone/embedded
    // ========================================================================
    {
        .tier = PLATFORM_TIER_CONSTRAINED,

        // Core neural network - Small-scale brain
        .max_neurons = 10000,                // 10K neurons
        .max_synapses_per_neuron = 100,      // Sparse connectivity
        .initial_neurons = 1000,             // Start with 1K neurons

        // Cognitive modules - Essential only
        .cognitive_modules_enabled =
            COGNITIVE_MODULE_ATTENTION |
            COGNITIVE_MODULE_WORKING_MEMORY |
            COGNITIVE_MODULE_SALIENCE |
            COGNITIVE_MODULE_EMOTIONS |
            COGNITIVE_MODULE_VISUAL_CORTEX |
            COGNITIVE_MODULE_AUDIO_CORTEX,

        // Visual cortex - Low resolution
        .visual = {
            .max_input_width = 160,
            .max_input_height = 120,
            .num_filters_conv1 = 16,
            .num_filters_conv2 = 32,
            .max_feature_maps = 64,
            .enable_pooling = true,
            .enable_attention = false,       // Attention disabled
        },

        // Audio cortex - Low fidelity
        .audio = {
            .max_sample_rate = 16000,        // Phone quality
            .num_mel_filters = 32,
            .num_mfcc = 13,
            .frame_size = 512,
            .enable_attention = false,
            .enable_memory = false,          // No auditory memory
        },

        // Resource budgets
        .memory_budget_mb = 128,             // 128MB budget
        .compute_budget_ops = 100000000ULL,  // 100 MFLOPS
        .max_threads = 4,

        // Feature flags - Minimal features
        .enable_gpu = false,
        .enable_bio_async = true,            // Bio-async efficient
        .enable_plasticity = true,           // Basic learning
        .enable_neuromodulation = false,     // No neuromodulation
        .enable_checkpointing = false,       // No checkpoints

        // Performance tuning
        .update_batch_size = 100,
        .spike_buffer_size = 10000,
        .sampling_rate = 0.5F,               // 50% sampling
    },

    // ========================================================================
    // [3] PLATFORM_TIER_MEDIUM - Laptop/tablet/dev board
    // ========================================================================
    {
        .tier = PLATFORM_TIER_MEDIUM,

        // Core neural network - Medium-scale brain
        .max_neurons = 100000,               // 100K neurons
        .max_synapses_per_neuron = 1000,     // Moderate connectivity
        .initial_neurons = 5000,             // Start with 5K neurons

        // Cognitive modules - Core cognition enabled
        .cognitive_modules_enabled =
            COGNITIVE_MODULE_ATTENTION |
            COGNITIVE_MODULE_WORKING_MEMORY |
            COGNITIVE_MODULE_SALIENCE |
            COGNITIVE_MODULE_EMOTIONS |
            COGNITIVE_MODULE_EMOTIONAL_TAG |
            COGNITIVE_MODULE_SEMANTIC_MEMORY |
            COGNITIVE_MODULE_EPISODIC_MEMORY |
            COGNITIVE_MODULE_EXECUTIVE |
            COGNITIVE_MODULE_REASONING |
            COGNITIVE_MODULE_GLOBAL_WORKSPACE |
            COGNITIVE_MODULE_VISUAL_CORTEX |
            COGNITIVE_MODULE_AUDIO_CORTEX,

        // Visual cortex - Medium resolution
        .visual = {
            .max_input_width = 320,
            .max_input_height = 240,
            .num_filters_conv1 = 32,
            .num_filters_conv2 = 64,
            .max_feature_maps = 128,
            .enable_pooling = true,
            .enable_attention = true,
        },

        // Audio cortex - Moderate fidelity
        .audio = {
            .max_sample_rate = 22050,        // Half CD quality
            .num_mel_filters = 64,
            .num_mfcc = 20,
            .frame_size = 1024,
            .enable_attention = true,
            .enable_memory = true,
        },

        // Resource budgets
        .memory_budget_mb = 1024,            // 1GB budget
        .compute_budget_ops = 1000000000ULL, // 1 GFLOPS
        .max_threads = 8,

        // Feature flags - Most enabled
        .enable_gpu = false,                 // GPU optional
        .enable_bio_async = true,
        .enable_plasticity = true,
        .enable_neuromodulation = true,
        .enable_checkpointing = true,

        // Performance tuning
        .update_batch_size = 500,
        .spike_buffer_size = 50000,
        .sampling_rate = 0.8F,               // 80% sampling
    },

    // ========================================================================
    // [4] PLATFORM_TIER_FULL - High-end desktop/server
    // ========================================================================
    {
        .tier = PLATFORM_TIER_FULL,

        // Core neural network - Large-scale brain
        .max_neurons = 1000000,              // 1M neurons (research-scale)
        .max_synapses_per_neuron = 10000,    // Dense connectivity
        .initial_neurons = 10000,            // Start with 10K neurons

        // Cognitive modules - ALL ENABLED
        .cognitive_modules_enabled =
            COGNITIVE_MODULE_ATTENTION |
            COGNITIVE_MODULE_WORKING_MEMORY |
            COGNITIVE_MODULE_SALIENCE |
            COGNITIVE_MODULE_EMOTIONS |
            COGNITIVE_MODULE_EMOTIONAL_TAG |
            COGNITIVE_MODULE_SEMANTIC_MEMORY |
            COGNITIVE_MODULE_EPISODIC_MEMORY |
            COGNITIVE_MODULE_CONSOLIDATION |
            COGNITIVE_MODULE_EXECUTIVE |
            COGNITIVE_MODULE_REASONING |
            COGNITIVE_MODULE_CURIOSITY |
            COGNITIVE_MODULE_META_LEARNING |
            COGNITIVE_MODULE_INTROSPECTION |
            COGNITIVE_MODULE_SELF_AWARENESS |
            COGNITIVE_MODULE_THEORY_OF_MIND |
            COGNITIVE_MODULE_MIRROR_NEURONS |
            COGNITIVE_MODULE_EMPATHY |
            COGNITIVE_MODULE_GLOBAL_WORKSPACE |
            COGNITIVE_MODULE_PREDICTIVE |
            COGNITIVE_MODULE_ETHICS |
            COGNITIVE_MODULE_VISUAL_CORTEX |
            COGNITIVE_MODULE_AUDIO_CORTEX,

        // Visual cortex - High resolution, full features
        .visual = {
            .max_input_width = 640,
            .max_input_height = 480,
            .num_filters_conv1 = 64,
            .num_filters_conv2 = 128,
            .max_feature_maps = 256,
            .enable_pooling = true,
            .enable_attention = true,
        },

        // Audio cortex - High fidelity
        .audio = {
            .max_sample_rate = 48000,        // CD quality
            .num_mel_filters = 128,
            .num_mfcc = 40,
            .frame_size = 2048,
            .enable_attention = true,
            .enable_memory = true,
        },

        // Resource budgets
        .memory_budget_mb = 4096,            // 4GB budget
        .compute_budget_ops = 10000000000ULL, // 10 GFLOPS
        .max_threads = 16,

        // Feature flags - All enabled
        .enable_gpu = true,
        .enable_bio_async = true,
        .enable_plasticity = true,
        .enable_neuromodulation = true,
        .enable_checkpointing = true,

        // Performance tuning
        .update_batch_size = 1000,
        .spike_buffer_size = 100000,
        .sampling_rate = 1.0F,               // Full sampling
    },

    // ========================================================================
    // [5] PLATFORM_TIER_NEUROMORPHIC - Placeholder (use specialized config)
    // ========================================================================
    {
        .tier = PLATFORM_TIER_NEUROMORPHIC,
        .max_neurons = 1000000,              // Hardware-dependent
        .max_synapses_per_neuron = 1000,
        .initial_neurons = 10000,
        .cognitive_modules_enabled = COGNITIVE_MODULE_ALL,  // Hardware determines
        .visual = { .max_input_width = 640, .max_input_height = 480 },
        .audio = { .max_sample_rate = 48000 },
        .memory_budget_mb = 256,
        .compute_budget_ops = 100000000000ULL,  // Event-driven, different metric
        .max_threads = 128,                     // Neuromorphic cores
        .enable_gpu = false,
        .enable_bio_async = true,
        .enable_plasticity = true,
        .enable_neuromodulation = true,
        .enable_checkpointing = false,
        .update_batch_size = 0,                 // Event-driven
        .spike_buffer_size = 1000000,
        .sampling_rate = 1.0F,
    },

    // ========================================================================
    // [6] PLATFORM_TIER_QUANTUM - Placeholder (use specialized config)
    // ========================================================================
    {
        .tier = PLATFORM_TIER_QUANTUM,
        .max_neurons = 100000,               // Classical fallback
        .max_synapses_per_neuron = 1000,
        .initial_neurons = 5000,
        .cognitive_modules_enabled = COGNITIVE_MODULE_ALL,
        .visual = { .max_input_width = 320, .max_input_height = 240 },
        .audio = { .max_sample_rate = 22050 },
        .memory_budget_mb = 8192,
        .compute_budget_ops = 100000000000ULL,  // Quantum ops different
        .max_threads = 16,
        .enable_gpu = true,
        .enable_bio_async = true,
        .enable_plasticity = true,
        .enable_neuromodulation = true,
        .enable_checkpointing = true,
        .update_batch_size = 500,
        .spike_buffer_size = 50000,
        .sampling_rate = 1.0F,
    },
};

//=============================================================================
// Tier Name Strings
//=============================================================================

static const char* TIER_NAMES[PLATFORM_TIER_COUNT] = {
    "BASIC",
    "MINIMAL",
    "CONSTRAINED",
    "MEDIUM",
    "FULL",
    "NEUROMORPHIC",
    "QUANTUM"
};

static const char* NEUROMORPHIC_HW_NAMES[NEUROMORPHIC_HW_COUNT] = {
    "Generic",
    "Loihi",
    "SpiNNaker",
    "BrainScaleS",
    "TrueNorth",
    "Dynapse",
    "Akida"
};

static const char* QUANTUM_BACKEND_NAMES[QUANTUM_BACKEND_COUNT] = {
    "Simulator",
    "IBM Quantum",
    "Rigetti",
    "D-Wave",
    "IonQ",
    "Google"
};

//=============================================================================
// API Implementation
//=============================================================================

platform_tier_t platform_tier_detect(void)
{
    system_resources_t resources;

    // Query system resources
    if (!system_resources_query(&resources)) {
        // If query fails, assume minimal platform
        LOG_WARN("Failed to query system resources, defaulting to MINIMAL tier");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Platform tier detection: system resources query failed, using MINIMAL");
        return PLATFORM_TIER_MINIMAL;
    }

    // Apply tier detection logic (BASIC → FULL, descending from best)
    // WHAT: Classify based on RAM and core count thresholds
    // WHY:  Both memory and compute affect NIMCP performance
    // HOW:  Use AND logic (both must meet threshold for tier)
    // NOTE: NEUROMORPHIC and QUANTUM are NOT auto-detected (require explicit config)

    // Check FULL tier (≥8GB RAM AND ≥8 cores)
    if (resources.total_ram_mb >= TIER_FULL_MIN_RAM_MB &&
        resources.num_cpu_cores >= TIER_FULL_MIN_CORES) {
        LOG_INFO("Detected FULL platform tier: %llu MB RAM, %u cores",
                 (unsigned long long)resources.total_ram_mb,
                 resources.num_cpu_cores);
        return PLATFORM_TIER_FULL;
    }

    // Check MEDIUM tier (≥2GB RAM AND ≥4 cores)
    if (resources.total_ram_mb >= TIER_MEDIUM_MIN_RAM_MB &&
        resources.num_cpu_cores >= TIER_MEDIUM_MIN_CORES) {
        LOG_INFO("Detected MEDIUM platform tier: %llu MB RAM, %u cores",
                 (unsigned long long)resources.total_ram_mb,
                 resources.num_cpu_cores);
        return PLATFORM_TIER_MEDIUM;
    }

    // Check CONSTRAINED tier (≥256MB RAM AND ≥2 cores)
    if (resources.total_ram_mb >= TIER_CONSTRAINED_MIN_RAM_MB &&
        resources.num_cpu_cores >= TIER_CONSTRAINED_MIN_CORES) {
        LOG_INFO("Detected CONSTRAINED platform tier: %llu MB RAM, %u cores",
                 (unsigned long long)resources.total_ram_mb,
                 resources.num_cpu_cores);
        return PLATFORM_TIER_CONSTRAINED;
    }

    // Check MINIMAL tier (≥64MB RAM)
    if (resources.total_ram_mb >= TIER_MINIMAL_MIN_RAM_MB) {
        LOG_INFO("Detected MINIMAL platform tier: %llu MB RAM, %u cores",
                 (unsigned long long)resources.total_ram_mb,
                 resources.num_cpu_cores);
        return PLATFORM_TIER_MINIMAL;
    }

    // Default to BASIC tier (ultra-constrained MCU)
    LOG_INFO("Detected BASIC platform tier: %llu MB RAM, %u cores",
             (unsigned long long)resources.total_ram_mb,
             resources.num_cpu_cores);
    return PLATFORM_TIER_BASIC;
}

platform_tier_config_t platform_tier_get_config(platform_tier_t tier)
{
    // Validate tier
    if (tier >= PLATFORM_TIER_COUNT) {
        LOG_WARN("Invalid tier %d, defaulting to MINIMAL", tier);
        tier = PLATFORM_TIER_MINIMAL;
    }

    // Return copy of config (safe for modification by caller)
    return TIER_CONFIGS[tier];
}

const char* platform_tier_get_name(platform_tier_t tier)
{
    // Validate tier
    if (tier >= PLATFORM_TIER_COUNT) {
        return "UNKNOWN";
    }

    return TIER_NAMES[tier];
}

bool platform_tier_can_enable_module(platform_tier_t tier,
                                      cognitive_module_flags_t module)
{
    // Validate tier
    if (tier >= PLATFORM_TIER_COUNT) {
        return false;
    }

    // Check if module bit is set in tier's enabled modules
    uint32_t enabled = TIER_CONFIGS[tier].cognitive_modules_enabled;
    return (enabled & module) != 0;
}

uint32_t platform_tier_recommend_neuron_count(platform_tier_t tier,
                                               const system_resources_t* resources)
{
    // Validate inputs
    if (tier >= PLATFORM_TIER_COUNT) {
        LOG_WARN("Invalid tier %d in platform_tier_recommend_neuron_count", tier);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Recommend neuron count: invalid tier value");
        return 1000;  // Safe default
    }
    if (!resources) {
        LOG_WARN("NULL resources pointer in platform_tier_recommend_neuron_count");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Recommend neuron count: NULL resources pointer");
        return 1000;  // Safe default
    }

    // Get tier defaults
    const platform_tier_config_t* config = &TIER_CONFIGS[tier];

    // Calculate max neurons based on available memory
    // WHAT: Use conservative estimate of memory per neuron
    // WHY:  Prevent OOM from over-optimistic sizing
    // HOW:  10KB per neuron for CPU mode, 80% safety margin

    const uint32_t KB_PER_NEURON = 10;
    const float SAFETY_MARGIN = 0.8F;

    uint64_t available_kb = resources->available_ram_mb * 1024;
    uint64_t safe_kb = (uint64_t)(available_kb * SAFETY_MARGIN);
    uint32_t max_by_memory = (uint32_t)(safe_kb / KB_PER_NEURON);

    // Take minimum of tier max and memory-based max
    uint32_t recommended = (max_by_memory < config->max_neurons) ?
                           max_by_memory : config->max_neurons;

    // Don't go below tier's initial neuron count, UNLESS memory is the
    // bottleneck (bumping above max_by_memory would risk OOM)
    if (recommended < config->initial_neurons && max_by_memory >= config->initial_neurons) {
        recommended = config->initial_neurons;
    }

    // Absolute minimum: at least 1 neuron
    if (recommended == 0) {
        recommended = 1;
    }

    LOG_DEBUG("Recommended neuron count for %s tier: %u "
              "(tier_max=%u, memory_max=%u, available_ram=%llu MB)",
              platform_tier_get_name(tier),
              recommended,
              config->max_neurons,
              max_by_memory,
              (unsigned long long)resources->available_ram_mb);

    return recommended;
}

bool platform_tier_validate_config(platform_tier_t tier,
                                    const platform_tier_config_t* config,
                                    char* error_msg,
                                    size_t error_msg_len)
{
    // Validate inputs
    if (tier >= PLATFORM_TIER_COUNT) {
        LOG_WARN("Invalid tier %d in platform_tier_validate_config", tier);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Validate config: invalid tier value");
        if (error_msg && error_msg_len > 0) {
            snprintf(error_msg, error_msg_len, "Invalid tier value: %d", tier);
        }
        return false;
    }
    if (!config) {
        LOG_WARN("NULL config pointer in platform_tier_validate_config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Validate config: NULL config pointer");
        if (error_msg && error_msg_len > 0) {
            snprintf(error_msg, error_msg_len, "NULL config pointer");
        }
        return false;
    }

    // Get tier limits
    const platform_tier_config_t* limits = &TIER_CONFIGS[tier];

    // Check neuron count
    if (config->max_neurons > limits->max_neurons) {
        if (error_msg && error_msg_len > 0) {
            snprintf(error_msg, error_msg_len,
                     "max_neurons %u exceeds tier limit %u",
                     config->max_neurons, limits->max_neurons);
        }
        return false;
    }

    // Check synapses per neuron
    if (config->max_synapses_per_neuron > limits->max_synapses_per_neuron) {
        if (error_msg && error_msg_len > 0) {
            snprintf(error_msg, error_msg_len,
                     "max_synapses_per_neuron %u exceeds tier limit %u",
                     config->max_synapses_per_neuron,
                     limits->max_synapses_per_neuron);
        }
        return false;
    }

    // Check memory budget
    if (config->memory_budget_mb > limits->memory_budget_mb) {
        if (error_msg && error_msg_len > 0) {
            snprintf(error_msg, error_msg_len,
                     "memory_budget_mb %u exceeds tier limit %u",
                     config->memory_budget_mb, limits->memory_budget_mb);
        }
        return false;
    }

    // Check visual cortex resolution
    if (config->visual.max_input_width > limits->visual.max_input_width ||
        config->visual.max_input_height > limits->visual.max_input_height) {
        if (error_msg && error_msg_len > 0) {
            snprintf(error_msg, error_msg_len,
                     "Visual resolution %ux%u exceeds tier limit %ux%u",
                     config->visual.max_input_width,
                     config->visual.max_input_height,
                     limits->visual.max_input_width,
                     limits->visual.max_input_height);
        }
        return false;
    }

    // Check audio sample rate
    if (config->audio.max_sample_rate > limits->audio.max_sample_rate) {
        if (error_msg && error_msg_len > 0) {
            snprintf(error_msg, error_msg_len,
                     "Audio sample rate %u exceeds tier limit %u",
                     config->audio.max_sample_rate,
                     limits->audio.max_sample_rate);
        }
        return false;
    }

    // Check for enabled modules not supported at tier
    uint32_t unsupported = config->cognitive_modules_enabled &
                          ~limits->cognitive_modules_enabled;
    if (unsupported != 0) {
        if (error_msg && error_msg_len > 0) {
            snprintf(error_msg, error_msg_len,
                     "Config enables modules (0x%08X) not supported at %s tier",
                     unsupported, platform_tier_get_name(tier));
        }
        return false;
    }

    // Validation passed
    if (error_msg && error_msg_len > 0) {
        error_msg[0] = '\0';  // Clear error message
    }

    return true;
}

//=============================================================================
// Internal Utilities
//=============================================================================

/**
 * @brief Get module name for logging (internal helper)
 *
 * WHAT: Convert module flag to human-readable name
 * WHY:  Better logging and debugging
 * HOW:  Simple switch statement
 *
 * NOTE: Not exposed in public API (internal use only)
 */
static const char* get_module_name(cognitive_module_flags_t module)
{
    switch (module) {
        case COGNITIVE_MODULE_ATTENTION:        return "Attention";
        case COGNITIVE_MODULE_WORKING_MEMORY:   return "Working Memory";
        case COGNITIVE_MODULE_SALIENCE:         return "Salience";
        case COGNITIVE_MODULE_EMOTIONS:         return "Emotions";
        case COGNITIVE_MODULE_EMOTIONAL_TAG:    return "Emotional Tagging";
        case COGNITIVE_MODULE_SEMANTIC_MEMORY:  return "Semantic Memory";
        case COGNITIVE_MODULE_EPISODIC_MEMORY:  return "Episodic Memory";
        case COGNITIVE_MODULE_CONSOLIDATION:    return "Consolidation";
        case COGNITIVE_MODULE_EXECUTIVE:        return "Executive";
        case COGNITIVE_MODULE_REASONING:        return "Reasoning";
        case COGNITIVE_MODULE_CURIOSITY:        return "Curiosity";
        case COGNITIVE_MODULE_META_LEARNING:    return "Meta-Learning";
        case COGNITIVE_MODULE_INTROSPECTION:    return "Introspection";
        case COGNITIVE_MODULE_SELF_AWARENESS:   return "Self-Awareness";
        case COGNITIVE_MODULE_THEORY_OF_MIND:   return "Theory of Mind";
        case COGNITIVE_MODULE_MIRROR_NEURONS:   return "Mirror Neurons";
        case COGNITIVE_MODULE_EMPATHY:          return "Empathy";
        case COGNITIVE_MODULE_GLOBAL_WORKSPACE: return "Global Workspace";
        case COGNITIVE_MODULE_PREDICTIVE:       return "Predictive";
        case COGNITIVE_MODULE_ETHICS:           return "Ethics";
        case COGNITIVE_MODULE_VISUAL_CORTEX:    return "Visual Cortex";
        case COGNITIVE_MODULE_AUDIO_CORTEX:     return "Audio Cortex";
        default:                                return "Unknown";
    }
}

//=============================================================================
// Diagnostic Functions (Optional, for testing/debugging)
//=============================================================================

/**
 * @brief Print tier configuration summary
 *
 * WHAT: Output detailed config to console for debugging
 * WHY:  Helpful for verifying tier settings
 * HOW:  Format and print all config fields
 *
 * NOTE: This is a diagnostic function, not part of core API
 *       Can be conditionally compiled out in production builds
 */
void platform_tier_print_config(platform_tier_t tier)
{
    if (tier >= PLATFORM_TIER_COUNT) {
        printf("Invalid tier: %d\n", tier);
        return;
    }

    const platform_tier_config_t* cfg = &TIER_CONFIGS[tier];

    printf("\n========== Platform Tier: %s ==========\n", TIER_NAMES[tier]);
    printf("Core Neural Network:\n");
    printf("  Max neurons:              %u\n", cfg->max_neurons);
    printf("  Max synapses/neuron:      %u\n", cfg->max_synapses_per_neuron);
    printf("  Initial neurons:          %u\n", cfg->initial_neurons);

    printf("\nVisual Cortex:\n");
    printf("  Max resolution:           %ux%u\n",
           cfg->visual.max_input_width, cfg->visual.max_input_height);
    printf("  Conv1 filters:            %u\n", cfg->visual.num_filters_conv1);
    printf("  Conv2 filters:            %u\n", cfg->visual.num_filters_conv2);
    printf("  Pooling enabled:          %s\n", cfg->visual.enable_pooling ? "yes" : "no");
    printf("  Attention enabled:        %s\n", cfg->visual.enable_attention ? "yes" : "no");

    printf("\nAudio Cortex:\n");
    printf("  Max sample rate:          %u Hz\n", cfg->audio.max_sample_rate);
    printf("  Mel filters:              %u\n", cfg->audio.num_mel_filters);
    printf("  MFCC coefficients:        %u\n", cfg->audio.num_mfcc);
    printf("  Frame size:               %u\n", cfg->audio.frame_size);

    printf("\nResource Budgets:\n");
    printf("  Memory budget:            %u MB\n", cfg->memory_budget_mb);
    printf("  Compute budget:           %llu ops/sec\n",
           (unsigned long long)cfg->compute_budget_ops);
    printf("  Max threads:              %u\n", cfg->max_threads);

    printf("\nFeature Flags:\n");
    printf("  GPU enabled:              %s\n", cfg->enable_gpu ? "yes" : "no");
    printf("  Bio-async enabled:        %s\n", cfg->enable_bio_async ? "yes" : "no");
    printf("  Plasticity enabled:       %s\n", cfg->enable_plasticity ? "yes" : "no");
    printf("  Neuromodulation enabled:  %s\n", cfg->enable_neuromodulation ? "yes" : "no");
    printf("  Checkpointing enabled:    %s\n", cfg->enable_checkpointing ? "yes" : "no");

    printf("\nPerformance Tuning:\n");
    printf("  Update batch size:        %u\n", cfg->update_batch_size);
    printf("  Spike buffer size:        %u\n", cfg->spike_buffer_size);
    printf("  Sampling rate:            %.2f\n", cfg->sampling_rate);

    printf("\nCognitive Modules Enabled: 0x%08X\n", cfg->cognitive_modules_enabled);

    printf("=============================================\n\n");
}

//=============================================================================
// Specialized Tier API Functions
//=============================================================================

/**
 * @brief Get default configuration for BASIC tier (ultra-constrained MCU)
 *
 * WHAT: Returns configuration for Arduino/ESP32 class devices
 * WHY:  These platforms need specific handling due to extreme constraints
 * HOW:  Pre-configured defaults for typical MCU capabilities
 */
basic_tier_config_t platform_tier_get_basic_config(void)
{
    basic_tier_config_t config = {
        // Core constraints - typical Arduino Mega/ESP32 class
        .max_neurons = 500,                  // 500 neurons max
        .max_synapses = 2500,                // 5 synapses per neuron average
        .memory_kb = 32,                     // 32KB budget (ESP32 has 520KB SRAM)

        // Fixed-point arithmetic for MCUs without FPU
        .use_fixed_point = true,
        .fixed_point_bits = 15,              // Q15 format

        // Minimal features
        .spike_only = true,                  // Spike-based only
        .reactive_only = true,               // No learning (flash constraints)
        .single_layer = true,                // Single layer feedforward

        // Power management
        .power_budget_mw = 50.0f,            // 50mW budget
        .sleep_threshold_ms = 100,           // Sleep after 100ms idle
        .deep_sleep_enabled = true           // Enable deep sleep
    };

    return config;
}

/**
 * @brief Get default configuration for neuromorphic hardware
 *
 * WHAT: Returns configuration for specific neuromorphic chip
 * WHY:  Each chip has unique constraints and capabilities
 * HOW:  Pre-configured defaults based on known hardware specs
 */
neuromorphic_tier_config_t platform_tier_get_neuromorphic_config(neuromorphic_hardware_t hardware)
{
    neuromorphic_tier_config_t config = {0};

    // Common defaults for all neuromorphic chips
    config.hardware = hardware;
    config.spike_native = true;
    config.async_events = true;

    switch (hardware) {
        case NEUROMORPHIC_HW_LOIHI:
            // Intel Loihi (1/2) specs
            config.max_neurons = 131072;             // 128K neurons per chip
            config.max_synapses_per_neuron = 4096;
            config.cores = 128;
            config.neurons_per_core = 1024;
            config.spike_buffer_depth = 16384;
            config.max_spike_rate_hz = 1000;
            config.routing_delay_timesteps = 64;
            config.stdp_hardware = true;
            config.homeostatic_hardware = false;
            config.learning_rate_granularity = 0.001f;
            config.timestep_ns = 1000;               // 1µs timestep
            config.power_budget_mw = 500.0f;         // ~500mW typical
            config.on_chip_memory_kb = 2048;         // 2MB on-chip
            config.external_memory_mb = 0;           // No external memory
            config.weight_sharing = true;
            config.enable_axon_delay = true;
            config.enable_multi_compartment = true;
            config.enable_dendritic_computation = false;
            break;

        case NEUROMORPHIC_HW_SPINNAKER:
            // SpiNNaker specs (single board)
            config.max_neurons = 1000000;            // 1M neurons per board
            config.max_synapses_per_neuron = 1000;
            config.cores = 864;                      // 48 chips × 18 cores
            config.neurons_per_core = 1200;
            config.spike_buffer_depth = 65536;
            config.max_spike_rate_hz = 200;
            config.routing_delay_timesteps = 16;
            config.stdp_hardware = false;            // Software STDP
            config.homeostatic_hardware = false;
            config.learning_rate_granularity = 0.0001f;
            config.timestep_ns = 1000000;            // 1ms timestep
            config.power_budget_mw = 5000.0f;        // ~5W per board
            config.on_chip_memory_kb = 128;          // 128KB per core
            config.external_memory_mb = 128;         // SDRAM per chip
            config.weight_sharing = false;
            config.enable_axon_delay = true;
            config.enable_multi_compartment = false;
            config.enable_dendritic_computation = false;
            break;

        case NEUROMORPHIC_HW_BRAINSCALES:
            // BrainScaleS-2 specs
            config.max_neurons = 512;                // 512 neurons per chip
            config.max_synapses_per_neuron = 256;
            config.cores = 1;                        // Analog ASIC
            config.neurons_per_core = 512;
            config.spike_buffer_depth = 1024;
            config.max_spike_rate_hz = 100000;       // Accelerated (1000x)
            config.routing_delay_timesteps = 1;
            config.stdp_hardware = true;
            config.homeostatic_hardware = true;
            config.learning_rate_granularity = 0.01f;
            config.timestep_ns = 1;                  // ~1ns (accelerated)
            config.power_budget_mw = 100.0f;         // ~100mW
            config.on_chip_memory_kb = 64;
            config.external_memory_mb = 0;
            config.weight_sharing = false;
            config.enable_axon_delay = false;
            config.enable_multi_compartment = true;
            config.enable_dendritic_computation = true;
            break;

        case NEUROMORPHIC_HW_TRUENORTH:
            // IBM TrueNorth specs
            config.max_neurons = 1048576;            // 1M neurons per chip
            config.max_synapses_per_neuron = 256;
            config.cores = 4096;
            config.neurons_per_core = 256;
            config.spike_buffer_depth = 4096;
            config.max_spike_rate_hz = 1000;
            config.routing_delay_timesteps = 1;
            config.stdp_hardware = false;            // No on-chip learning
            config.homeostatic_hardware = false;
            config.learning_rate_granularity = 0.0f; // No learning
            config.timestep_ns = 1000000;            // 1ms timestep
            config.power_budget_mw = 70.0f;          // ~70mW ultra-low power
            config.on_chip_memory_kb = 16384;        // 16MB SRAM total
            config.external_memory_mb = 0;
            config.weight_sharing = false;
            config.enable_axon_delay = false;
            config.enable_multi_compartment = false;
            config.enable_dendritic_computation = false;
            break;

        case NEUROMORPHIC_HW_DYNAPSE:
            // Dynapse specs (INI Zurich)
            config.max_neurons = 1024;               // 1024 neurons per chip
            config.max_synapses_per_neuron = 64;
            config.cores = 4;
            config.neurons_per_core = 256;
            config.spike_buffer_depth = 512;
            config.max_spike_rate_hz = 1000;
            config.routing_delay_timesteps = 4;
            config.stdp_hardware = true;
            config.homeostatic_hardware = false;
            config.learning_rate_granularity = 0.01f;
            config.timestep_ns = 100;                // 100ns (analog)
            config.power_budget_mw = 10.0f;          // ~10mW
            config.on_chip_memory_kb = 4;
            config.external_memory_mb = 0;
            config.weight_sharing = true;
            config.enable_axon_delay = true;
            config.enable_multi_compartment = false;
            config.enable_dendritic_computation = false;
            break;

        case NEUROMORPHIC_HW_AKIDA:
            // BrainChip Akida specs
            config.max_neurons = 1280000;            // 1.28M neurons per chip
            config.max_synapses_per_neuron = 4096;
            config.cores = 80;
            config.neurons_per_core = 16000;
            config.spike_buffer_depth = 8192;
            config.max_spike_rate_hz = 10000;
            config.routing_delay_timesteps = 1;
            config.stdp_hardware = true;
            config.homeostatic_hardware = false;
            config.learning_rate_granularity = 0.001f;
            config.timestep_ns = 1000;               // 1µs
            config.power_budget_mw = 300.0f;         // ~300mW
            config.on_chip_memory_kb = 1024;         // 1MB SRAM
            config.external_memory_mb = 16;
            config.weight_sharing = true;
            config.enable_axon_delay = false;
            config.enable_multi_compartment = false;
            config.enable_dendritic_computation = false;
            break;

        case NEUROMORPHIC_HW_GENERIC:
        default:
            // Generic neuromorphic simulation defaults
            config.max_neurons = 100000;
            config.max_synapses_per_neuron = 1000;
            config.cores = 16;
            config.neurons_per_core = 6250;
            config.spike_buffer_depth = 10000;
            config.max_spike_rate_hz = 1000;
            config.routing_delay_timesteps = 8;
            config.stdp_hardware = false;
            config.homeostatic_hardware = false;
            config.learning_rate_granularity = 0.0001f;
            config.timestep_ns = 1000000;            // 1ms
            config.power_budget_mw = 1000.0f;        // Simulation, not real
            config.on_chip_memory_kb = 4096;
            config.external_memory_mb = 1024;
            config.weight_sharing = true;
            config.enable_axon_delay = true;
            config.enable_multi_compartment = true;
            config.enable_dendritic_computation = true;
            break;
    }

    LOG_DEBUG("Created neuromorphic config for %s: %u neurons, %u cores",
              NEUROMORPHIC_HW_NAMES[hardware],
              config.max_neurons,
              config.cores);

    return config;
}

/**
 * @brief Get default configuration for quantum backend
 *
 * WHAT: Returns configuration for specific quantum platform
 * WHY:  Each backend has different qubit counts and error rates
 * HOW:  Pre-configured defaults based on known hardware/simulator specs
 */
quantum_tier_config_t platform_tier_get_quantum_config(quantum_backend_t backend)
{
    quantum_tier_config_t config = {0};

    // Common defaults
    config.backend = backend;
    config.native_gates_clifford = true;

    switch (backend) {
        case QUANTUM_BACKEND_IBMQ:
            // IBM Quantum (e.g., IBM Eagle/Osprey class)
            config.num_qubits = 127;                 // Eagle processor
            config.num_logical_qubits = 10;          // After error correction
            config.connectivity = 0.15f;             // Sparse heavy-hex
            config.t1_coherence_us = 200.0f;
            config.t2_coherence_us = 150.0f;
            config.single_qubit_error = 0.0003f;
            config.two_qubit_error = 0.01f;
            config.readout_error = 0.01f;
            config.native_gates_t = true;
            config.native_gates_rx_ry = true;
            config.max_circuit_depth = 100;
            config.quantum_consensus = true;
            config.quantum_optimization = true;
            config.quantum_sampling = true;
            config.superposition_states = true;
            config.classical_cores = 8;
            config.classical_memory_gb = 16;
            config.quantum_classical_ratio = 0.3f;
            config.max_shots = 8192;
            config.queue_depth = 100;
            config.real_hardware = true;
            break;

        case QUANTUM_BACKEND_RIGETTI:
            // Rigetti (e.g., Aspen-M class)
            config.num_qubits = 80;
            config.num_logical_qubits = 6;
            config.connectivity = 0.12f;             // Octagonal lattice
            config.t1_coherence_us = 30.0f;
            config.t2_coherence_us = 25.0f;
            config.single_qubit_error = 0.001f;
            config.two_qubit_error = 0.02f;
            config.readout_error = 0.02f;
            config.native_gates_t = false;
            config.native_gates_rx_ry = true;
            config.max_circuit_depth = 50;
            config.quantum_consensus = true;
            config.quantum_optimization = true;
            config.quantum_sampling = true;
            config.superposition_states = true;
            config.classical_cores = 8;
            config.classical_memory_gb = 16;
            config.quantum_classical_ratio = 0.25f;
            config.max_shots = 10000;
            config.queue_depth = 50;
            config.real_hardware = true;
            break;

        case QUANTUM_BACKEND_DWAVE:
            // D-Wave (quantum annealer)
            config.num_qubits = 5000;                // Advantage system
            config.num_logical_qubits = 100;         // After embedding
            config.connectivity = 0.06f;             // Pegasus topology
            config.t1_coherence_us = 10.0f;          // Different metric for annealer
            config.t2_coherence_us = 10.0f;
            config.single_qubit_error = 0.05f;       // Not gate-based
            config.two_qubit_error = 0.05f;
            config.readout_error = 0.03f;
            config.native_gates_clifford = false;    // Not gate-based
            config.native_gates_t = false;
            config.native_gates_rx_ry = false;
            config.max_circuit_depth = 1;            // Single annealing operation
            config.quantum_consensus = false;        // Not suitable
            config.quantum_optimization = true;      // Primary use case
            config.quantum_sampling = true;
            config.superposition_states = true;
            config.classical_cores = 4;
            config.classical_memory_gb = 8;
            config.quantum_classical_ratio = 0.8f;   // Heavy quantum use
            config.max_shots = 10000;
            config.queue_depth = 100;
            config.real_hardware = true;
            break;

        case QUANTUM_BACKEND_IONQ:
            // IonQ (trapped ion)
            config.num_qubits = 32;
            config.num_logical_qubits = 8;
            config.connectivity = 1.0f;              // All-to-all
            config.t1_coherence_us = 10000.0f;       // Excellent coherence
            config.t2_coherence_us = 1000.0f;
            config.single_qubit_error = 0.0001f;     // Very low error
            config.two_qubit_error = 0.005f;
            config.readout_error = 0.005f;
            config.native_gates_t = true;
            config.native_gates_rx_ry = true;
            config.max_circuit_depth = 200;          // Deep circuits possible
            config.quantum_consensus = true;
            config.quantum_optimization = true;
            config.quantum_sampling = true;
            config.superposition_states = true;
            config.classical_cores = 4;
            config.classical_memory_gb = 8;
            config.quantum_classical_ratio = 0.4f;
            config.max_shots = 10000;
            config.queue_depth = 50;
            config.real_hardware = true;
            break;

        case QUANTUM_BACKEND_GOOGLE:
            // Google (e.g., Sycamore)
            config.num_qubits = 72;
            config.num_logical_qubits = 8;
            config.connectivity = 0.1f;              // 2D grid
            config.t1_coherence_us = 15.0f;
            config.t2_coherence_us = 10.0f;
            config.single_qubit_error = 0.0015f;
            config.two_qubit_error = 0.006f;
            config.readout_error = 0.02f;
            config.native_gates_t = false;
            config.native_gates_rx_ry = true;
            config.max_circuit_depth = 40;
            config.quantum_consensus = true;
            config.quantum_optimization = true;
            config.quantum_sampling = true;
            config.superposition_states = true;
            config.classical_cores = 8;
            config.classical_memory_gb = 16;
            config.quantum_classical_ratio = 0.3f;
            config.max_shots = 20000;
            config.queue_depth = 20;
            config.real_hardware = true;
            break;

        case QUANTUM_BACKEND_SIMULATOR:
        default:
            // Classical quantum simulator
            config.num_qubits = 32;                  // Limited by RAM (~4GB for 32 qubits)
            config.num_logical_qubits = 32;          // No error correction needed
            config.connectivity = 1.0f;              // All-to-all
            config.t1_coherence_us = 1000000.0f;     // Infinite (simulated)
            config.t2_coherence_us = 1000000.0f;
            config.single_qubit_error = 0.0f;        // Perfect simulation
            config.two_qubit_error = 0.0f;
            config.readout_error = 0.0f;
            config.native_gates_t = true;
            config.native_gates_rx_ry = true;
            config.max_circuit_depth = 1000;         // Limited by RAM
            config.quantum_consensus = true;
            config.quantum_optimization = true;
            config.quantum_sampling = true;
            config.superposition_states = true;
            config.classical_cores = 8;
            config.classical_memory_gb = 16;
            config.quantum_classical_ratio = 0.5f;
            config.max_shots = 100000;
            config.queue_depth = 1000;
            config.real_hardware = false;
            break;
    }

    LOG_DEBUG("Created quantum config for %s: %u qubits, real_hw=%s",
              QUANTUM_BACKEND_NAMES[backend],
              config.num_qubits,
              config.real_hardware ? "yes" : "no");

    return config;
}

/**
 * @brief Check if tier is a specialized (non-standard) tier
 *
 * WHAT: Test if tier is NEUROMORPHIC or QUANTUM (vs standard compute tiers)
 * WHY:  Specialized tiers need different handling
 * HOW:  Simple range check
 */
bool platform_tier_is_specialized(platform_tier_t tier)
{
    return (tier == PLATFORM_TIER_NEUROMORPHIC || tier == PLATFORM_TIER_QUANTUM);
}

/**
 * @brief Get human-readable name for neuromorphic hardware
 */
const char* platform_tier_neuromorphic_name(neuromorphic_hardware_t hardware)
{
    if (hardware >= NEUROMORPHIC_HW_COUNT) {
        return "Unknown";
    }
    return NEUROMORPHIC_HW_NAMES[hardware];
}

/**
 * @brief Get human-readable name for quantum backend
 */
const char* platform_tier_quantum_name(quantum_backend_t backend)
{
    if (backend >= QUANTUM_BACKEND_COUNT) {
        return "Unknown";
    }
    return QUANTUM_BACKEND_NAMES[backend];
}
