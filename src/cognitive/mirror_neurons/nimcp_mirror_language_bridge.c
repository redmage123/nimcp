//=============================================================================
// nimcp_mirror_language_bridge.c - Mirror Neuron-Language Layer Bridge
//=============================================================================
/**
 * @file nimcp_mirror_language_bridge.c
 * @brief Implementation of Mirror-Language bidirectional bridge
 *
 * Provides integration between mirror neuron system and language layer
 * (Broca's and Wernicke's areas) for speech mirroring, action-word binding,
 * and embodied semantics.
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/mirror_neurons/nimcp_mirror_language_bridge.h"
#include "cognitive/nimcp_mirror_neurons.h"
#include "core/brain/regions/broca/nimcp_broca_adapter.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_adapter.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(mirror_language_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_mirror_language_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_mirror_language_bridge_mesh_registry = NULL;

nimcp_error_t mirror_language_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_mirror_language_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "mirror_language_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "mirror_language_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_mirror_language_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_mirror_language_bridge_mesh_registry = registry;
    return err;
}

void mirror_language_bridge_mesh_unregister(void) {
    if (g_mirror_language_bridge_mesh_registry && g_mirror_language_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_mirror_language_bridge_mesh_registry, g_mirror_language_bridge_mesh_id);
        g_mirror_language_bridge_mesh_id = 0;
        g_mirror_language_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from mirror_language_bridge module (instance-level) */
static inline void mirror_language_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_mirror_language_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_mirror_language_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_mirror_language_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


//=============================================================================
// Internal Constants
//=============================================================================

#define MAX_PHONEME_TEMPLATES     64
#define MAX_ACTIVE_SIMULATIONS    16
#define MAX_PATHWAY_BUFFER        32
#define ARTICULATORY_FEATURE_DIM  16
#define BINDING_HASH_SIZE         1024

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Phoneme articulatory template
 */
typedef struct {
    uint8_t phoneme_id;
    float features[ARTICULATORY_FEATURE_DIM];
    uint32_t num_features;
    bool is_registered;
} phoneme_template_t;

/**
 * @brief Active simulation entry
 */
typedef struct {
    articulatory_simulation_t simulation;
    uint64_t start_time_ms;
    bool is_active;
} active_simulation_t;

/**
 * @brief Binding hash entry
 */
typedef struct binding_entry {
    action_word_binding_t binding;
    bool is_valid;
    struct binding_entry* next;  /* For hash collision chaining */
} binding_entry_t;

/**
 * @brief Bridge internal structure
 */
struct mirror_language_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    mirror_language_config_t config;

    /* Connected modules */
    mirror_neurons_t mirror;
    broca_adapter_t* broca;
    wernicke_adapter_t* wernicke;
    bio_router_t router;

    /* State */
    ml_bridge_state_t state;
    bool is_initialized;

    /* Phoneme templates */
    phoneme_template_t phoneme_templates[MAX_PHONEME_TEMPLATES];
    uint32_t num_templates;


    /* Phoneme activations */
    phoneme_mirror_activation_t phoneme_activations[MAX_PHONEME_TEMPLATES];

    /* Active simulations */
    active_simulation_t simulations[MAX_ACTIVE_SIMULATIONS];
    uint32_t num_active_simulations;

    /* Action-word bindings (hash table) */
    binding_entry_t* binding_table[BINDING_HASH_SIZE];
    uint32_t num_bindings;

    /* Pathway activation buffer */
    pathway_activation_t pathway_buffer[MAX_PATHWAY_BUFFER];
    uint32_t pathway_head;
    uint32_t pathway_count;

    /* Timing */
    uint64_t last_update_ms;

    /* Statistics */
    mirror_language_stats_t stats;

    /* Callbacks */
    ml_phoneme_callback_t phoneme_callback;
    void* phoneme_callback_data;
    ml_binding_callback_t binding_callback;
    void* binding_callback_data;
    ml_simulation_callback_t simulation_callback;
    void* simulation_callback_data;
    ml_semantic_callback_t semantic_callback;
    void* semantic_callback_data;

    /* Health agent (instance-level) */
    nimcp_health_agent_t* health_agent;
};

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(mirror_language_bridge)

//=============================================================================
// Helper Functions - Time
//=============================================================================

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_current_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

//=============================================================================
// Helper Functions - Hash Table
//=============================================================================

/**
 * @brief Compute hash for action-word pair
 */
static uint32_t binding_hash(uint32_t action_id, uint32_t word_id)
{
    /* Simple hash combining action and word IDs */
    uint32_t hash = action_id * 31 + word_id;
    return hash % BINDING_HASH_SIZE;
}

/**
 * @brief Find binding in hash table
 */
static binding_entry_t* find_binding(
    const mirror_language_bridge_t* bridge,
    uint32_t action_id,
    uint32_t word_id)
{
    uint32_t hash = binding_hash(action_id, word_id);
    binding_entry_t* entry = bridge->binding_table[hash];

    while (entry) {
        if (entry->is_valid &&
            entry->binding.action_id == action_id &&
            entry->binding.word_id == word_id) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;
}

/**
 * @brief Add binding to hash table
 */
static binding_entry_t* add_binding_entry(
    mirror_language_bridge_t* bridge,
    const action_word_binding_t* binding)
{
    uint32_t hash = binding_hash(binding->action_id, binding->word_id);

    /* Check if already exists */
    binding_entry_t* existing = find_binding(bridge, binding->action_id, binding->word_id);
    if (existing) {
        existing->binding = *binding;
        return existing;
    }

    /* Create new entry */
    binding_entry_t* entry = nimcp_malloc(sizeof(binding_entry_t));
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate entry");

        return NULL;
    }

    entry->binding = *binding;
    entry->is_valid = true;
    entry->next = bridge->binding_table[hash];
    bridge->binding_table[hash] = entry;
    bridge->num_bindings++;

    return entry;
}

/**
 * @brief Remove binding from hash table
 */
static bool remove_binding_entry(
    mirror_language_bridge_t* bridge,
    uint32_t action_id,
    uint32_t word_id)
{
    uint32_t hash = binding_hash(action_id, word_id);
    binding_entry_t* entry = bridge->binding_table[hash];
    binding_entry_t* prev = NULL;

    while (entry) {
        if (entry->is_valid &&
            entry->binding.action_id == action_id &&
            entry->binding.word_id == word_id) {

            if (prev) {
                prev->next = entry->next;
            } else {
                bridge->binding_table[hash] = entry->next;
            }

            nimcp_free(entry);
            entry = NULL;
            bridge->num_bindings--;
            return true;
        }
        prev = entry;
        entry = entry->next;
    }

    return false;
}

//=============================================================================
// Helper Functions - Phoneme Matching
//=============================================================================

/**
 * @brief Compute cosine similarity between feature vectors
 */
static float compute_feature_similarity(
    const float* a,
    const float* b,
    uint32_t n)
{
    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            mirror_language_bridge_heartbeat("mirror_langu_loop",
                             (float)(i + 1) / (float)n);
        }

        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    if (norm_a < 1e-6f || norm_b < 1e-6f) {
        return 0.0f;
    }

    return dot / (sqrtf(norm_a) * sqrtf(norm_b));
}

/**
 * @brief Find best matching phoneme template
 */
static int find_best_phoneme_match(
    const mirror_language_bridge_t* bridge,
    const float* features,
    uint32_t num_features,
    uint8_t* best_phoneme,
    float* confidence)
{
    float best_sim = -1.0f;
    uint8_t best_id = 0;
    uint32_t match_features = num_features < ARTICULATORY_FEATURE_DIM ?
                              num_features : ARTICULATORY_FEATURE_DIM;

    for (uint32_t i = 0; i < bridge->num_templates; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_templates > 256) {
            mirror_language_bridge_heartbeat("mirror_langu_loop",
                             (float)(i + 1) / (float)bridge->num_templates);
        }

        if (!bridge->phoneme_templates[i].is_registered) {
            continue;
        }

        float sim = compute_feature_similarity(
            features,
            bridge->phoneme_templates[i].features,
            match_features
        );

        if (sim > best_sim) {
            best_sim = sim;
            best_id = bridge->phoneme_templates[i].phoneme_id;
        }
    }

    if (best_sim < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_best_phoneme_match: validation failed");
        return -1;
    }

    *best_phoneme = best_id;
    *confidence = (best_sim + 1.0f) / 2.0f;  /* Normalize to [0,1] */
    return 0;
}

//=============================================================================
// Helper Functions - Pathway Activation
//=============================================================================

/**
 * @brief Record pathway activation
 */
static void record_pathway_activation(
    mirror_language_bridge_t* bridge,
    ml_pathway_t pathway,
    float strength,
    uint32_t source_id,
    uint32_t target_id)
{
    uint64_t now_ms = get_current_time_ms();

    pathway_activation_t* activation = &bridge->pathway_buffer[bridge->pathway_head];
    activation->pathway = pathway;
    activation->activation_strength = strength;
    activation->timestamp_ms = now_ms;
    activation->source_id = source_id;
    activation->target_id = target_id;

    bridge->pathway_head = (bridge->pathway_head + 1) % MAX_PATHWAY_BUFFER;
    if (bridge->pathway_count < MAX_PATHWAY_BUFFER) {
        bridge->pathway_count++;
    }

    /* Update stats */
    switch (pathway) {
        case ML_PATH_MIRROR_TO_BROCA:
            bridge->stats.mirror_to_broca_count++;
            break;
        case ML_PATH_MIRROR_TO_WERNICKE:
            bridge->stats.mirror_to_wernicke_count++;
            break;
        case ML_PATH_BROCA_TO_MIRROR:
            bridge->stats.broca_to_mirror_count++;
            break;
        case ML_PATH_WERNICKE_TO_MIRROR:
            bridge->stats.wernicke_to_mirror_count++;
            break;
        default:
            break;
    }
}

//=============================================================================
// Helper Functions - Simulation Management
//=============================================================================

/**
 * @brief Find free simulation slot
 */
static int find_free_simulation_slot(const mirror_language_bridge_t* bridge)
{
    for (uint32_t i = 0; i < MAX_ACTIVE_SIMULATIONS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MAX_ACTIVE_SIMULATIONS > 256) {
            mirror_language_bridge_heartbeat("mirror_langu_loop",
                             (float)(i + 1) / (float)MAX_ACTIVE_SIMULATIONS);
        }

        if (!bridge->simulations[i].is_active) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief Start articulatory simulation
 */
static int start_simulation(
    mirror_language_bridge_t* bridge,
    uint8_t phoneme_id,
    float activation)
{
    int slot = find_free_simulation_slot(bridge);
    if (slot < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "start_simulation: validation failed");
        return -1;
    }

    uint64_t now_ms = get_current_time_ms();

    active_simulation_t* sim = &bridge->simulations[slot];
    sim->simulation.phoneme_id = phoneme_id;
    sim->simulation.target_activation = activation;
    sim->simulation.covert_only = true;
    sim->simulation.onset_time_ms = now_ms;
    sim->simulation.duration_ms = 100.0f;  /* Default duration */
    sim->start_time_ms = now_ms;
    sim->is_active = true;

    bridge->num_active_simulations++;
    bridge->stats.articulatory_simulations++;

    /* Invoke callback */
    if (bridge->simulation_callback) {
        bridge->simulation_callback(&sim->simulation, bridge->simulation_callback_data);
    }

    return 0;
}

/**
 * @brief Update active simulations
 */
static void update_simulations(mirror_language_bridge_t* bridge, uint64_t now_ms)
{
    float decay = bridge->config.simulation_decay_rate;

    for (uint32_t i = 0; i < MAX_ACTIVE_SIMULATIONS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MAX_ACTIVE_SIMULATIONS > 256) {
            mirror_language_bridge_heartbeat("mirror_langu_loop",
                             (float)(i + 1) / (float)MAX_ACTIVE_SIMULATIONS);
        }

        if (!bridge->simulations[i].is_active) {
            continue;
        }

        active_simulation_t* sim = &bridge->simulations[i];
        float elapsed_ms = (float)(now_ms - sim->start_time_ms);

        /* Check if simulation is complete */
        if (elapsed_ms >= sim->simulation.duration_ms) {
            sim->is_active = false;
            bridge->num_active_simulations--;
            continue;
        }

        /* Apply decay to activation */
        sim->simulation.target_activation *= (1.0f - decay);
        if (sim->simulation.target_activation < 0.01f) {
            sim->is_active = false;
            bridge->num_active_simulations--;
        }
    }
}

//=============================================================================
// Helper Functions - Broca/Wernicke Integration
//=============================================================================

/**
 * @brief Send simulation request to Broca
 */
static int send_to_broca(
    mirror_language_bridge_t* bridge,
    uint8_t phoneme_id,
    float activation)
{
    if (!bridge->broca) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "send_to_broca: bridge->broca is NULL");
        return -1;
    }

    /* Create motor command for covert rehearsal */
    broca_output_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));

    cmd.phoneme = phoneme_id;
    cmd.position = activation;
    cmd.velocity = 0.0f;  /* Covert - no actual movement */
    cmd.timestamp_ms = (double)(get_current_time_ms());

    /* Note: In a full implementation, this would use bio-async messaging
     * or direct function calls to Broca's speech motor planner */

    return 0;
}

/**
 * @brief Query Wernicke for semantic context
 */
static int query_wernicke_semantics(
    mirror_language_bridge_t* bridge,
    uint32_t action_id,
    semantic_action_context_t* context)
{
    if (!bridge->wernicke) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "query_wernicke_semantics: bridge->wernicke is NULL");
        return -1;
    }

    /* In a full implementation, this would query Wernicke's semantic integrator
     * for concepts related to the action */

    memset(context, 0, sizeof(*context));
    context->concept_id = action_id;  /* Placeholder */
    context->relevance = 0.5f;
    context->semantic_activation = 0.5f;

    return 0;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

mirror_language_config_t mirror_language_default_config(void)
{
    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_defa", 0.0f);


    mirror_language_config_t config;
    memset(&config, 0, sizeof(config));

    config.update_interval_ms = ML_DEFAULT_UPDATE_INTERVAL_MS;
    config.max_phoneme_mirrors = ML_DEFAULT_MAX_PHONEME_MIRRORS;
    config.max_action_words = ML_DEFAULT_MAX_ACTION_WORDS;
    config.binding_capacity = ML_DEFAULT_BINDING_CAPACITY;

    config.phoneme_activation_threshold = ML_DEFAULT_PHONEME_THRESHOLD;
    config.semantic_activation_threshold = ML_DEFAULT_SEMANTIC_THRESHOLD;
    config.binding_strength_init = ML_DEFAULT_BINDING_STRENGTH_INIT;

    config.binding_learning_rate = ML_DEFAULT_BINDING_LEARNING_RATE;
    config.simulation_decay_rate = ML_DEFAULT_SIMULATION_DECAY_RATE;

    config.enable_phoneme_mirroring = true;
    config.enable_action_word_binding = true;
    config.enable_articulatory_simulation = true;
    config.enable_semantic_priming = true;
    config.enable_covert_rehearsal = true;

    config.enable_bio_async = true;

    return config;
}

mirror_language_bridge_t* mirror_language_bridge_create(
    mirror_neurons_t mirror,
    broca_adapter_t* broca,
    wernicke_adapter_t* wernicke,
    const mirror_language_config_t* config)
{
    if (!mirror) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror is NULL");

        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_create", 0.0f);


    mirror_language_bridge_t* bridge = nimcp_malloc(sizeof(mirror_language_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    memset(bridge, 0, sizeof(*bridge));

    /* Initialize bridge base infrastructure (includes mutex for thread safety) */
    if (bridge_base_init(&bridge->base, 0, "mirror_language") != 0) {
        NIMCP_LOGGING_WARN("Mirror-Language: Failed to initialize bridge base");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = mirror_language_default_config();
    }

    /* Store module references */
    bridge->mirror = mirror;
    bridge->broca = broca;
    bridge->wernicke = wernicke;

    /* Initialize state */
    bridge->state = ML_STATE_IDLE;
    bridge->is_initialized = true;
    bridge->last_update_ms = get_current_time_ms();

    /* Initialize hash table */
    memset(bridge->binding_table, 0, sizeof(bridge->binding_table));

    NIMCP_LOGGING_INFO("Mirror-language bridge created (mirror=%p, broca=%p, wernicke=%p)",
        (void*)mirror, (void*)broca, (void*)wernicke);

    return bridge;
}

void mirror_language_bridge_destroy(mirror_language_bridge_t* bridge)
{
    if (!bridge) {
        return;
    }

    /* Free binding table entries */
    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_destroy", 0.0f);


    for (uint32_t i = 0; i < BINDING_HASH_SIZE; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && BINDING_HASH_SIZE > 256) {
            mirror_language_bridge_heartbeat("mirror_langu_loop",
                             (float)(i + 1) / (float)BINDING_HASH_SIZE);
        }

        binding_entry_t* entry = bridge->binding_table[i];
        while (entry) {
            binding_entry_t* next = entry->next;
            nimcp_free(entry);
            entry = NULL;
            entry = next;
        }
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO("Mirror-language bridge destroyed");
}

int mirror_language_bridge_reset(mirror_language_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Reset state */
    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_reset", 0.0f);


    bridge->state = ML_STATE_IDLE;

    /* Clear phoneme activations */
    memset(bridge->phoneme_activations, 0, sizeof(bridge->phoneme_activations));

    /* Clear simulations */
    memset(bridge->simulations, 0, sizeof(bridge->simulations));
    bridge->num_active_simulations = 0;

    /* Clear pathway buffer */
    bridge->pathway_head = 0;
    bridge->pathway_count = 0;

    /* Reset timing */
    bridge->last_update_ms = get_current_time_ms();

    return 0;
}

//=============================================================================
// Connection Functions
//=============================================================================

int mirror_language_connect_broca(
    mirror_language_bridge_t* bridge,
    broca_adapter_t* broca)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_conn", 0.0f);


    bridge->broca = broca;

    NIMCP_LOGGING_INFO("Connected to Broca's area adapter");

    return 0;
}

int mirror_language_connect_wernicke(
    mirror_language_bridge_t* bridge,
    wernicke_adapter_t* wernicke)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_conn", 0.0f);


    bridge->wernicke = wernicke;

    NIMCP_LOGGING_INFO("Connected to Wernicke's area adapter");

    return 0;
}

int mirror_language_connect_bio_async(
    mirror_language_bridge_t* bridge,
    bio_router_t router)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_conn", 0.0f);


    bridge->router = router;

    NIMCP_LOGGING_INFO("Connected to bio-async router");

    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

int mirror_language_bridge_update(
    mirror_language_bridge_t* bridge,
    uint64_t timestamp_ms)
{
    if (!bridge || !bridge->is_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_language_bridge_update: required parameter is NULL (bridge, bridge->is_initialized)");
        return -1;
    }

    /* Update simulations */
    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_update", 0.0f);


    update_simulations(bridge, timestamp_ms);

    /* Decay phoneme activations */
    float decay = bridge->config.simulation_decay_rate;
    for (uint32_t i = 0; i < MAX_PHONEME_TEMPLATES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MAX_PHONEME_TEMPLATES > 256) {
            mirror_language_bridge_heartbeat("mirror_langu_loop",
                             (float)(i + 1) / (float)MAX_PHONEME_TEMPLATES);
        }

        bridge->phoneme_activations[i].observation_activation *= (1.0f - decay);
        bridge->phoneme_activations[i].simulation_activation *= (1.0f - decay);
    }

    /* Update state based on activity */
    if (bridge->num_active_simulations > 0) {
        bridge->state = ML_STATE_SIMULATING_ARTICULATION;
    } else {
        bridge->state = ML_STATE_IDLE;
    }

    bridge->last_update_ms = timestamp_ms;
    bridge->stats.state = bridge->state;
    bridge->stats.active_simulations = bridge->num_active_simulations;
    bridge->stats.active_bindings = bridge->num_bindings;

    return 0;
}

//=============================================================================
// Mirror -> Broca Pathway
//=============================================================================

int mirror_language_observe_speech(
    mirror_language_bridge_t* bridge,
    const speech_observation_t* observation)
{
    if (!bridge || !observation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_language_observe_speech: required parameter is NULL (bridge, observation)");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, observation, sizeof(*observation));

    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_obse", 0.0f);


    bridge->state = ML_STATE_OBSERVING_SPEECH;
    bridge->stats.speech_observations++;

    /* Find matching phoneme */
    uint8_t phoneme_id = observation->phoneme_id;
    float confidence = observation->confidence;

    /* If phoneme not specified, try to match from features */
    if (phoneme_id == 0 && observation->num_features > 0) {
        int ret = find_best_phoneme_match(
            bridge,
            observation->articulatory_features,
            observation->num_features,
            &phoneme_id,
            &confidence
        );
        if (ret < 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_language_observe_speech: validation failed");
            return -1;
        }
    }

    /* Update phoneme activation */
    if (phoneme_id < MAX_PHONEME_TEMPLATES) {
        phoneme_mirror_activation_t* act = &bridge->phoneme_activations[phoneme_id];
        act->phoneme_id = phoneme_id;
        act->observation_activation = confidence;
        act->obs_type = observation->type;
        act->timestamp_ms = observation->timestamp_ms;

        bridge->stats.phoneme_activations++;

        /* Invoke callback */
        if (bridge->phoneme_callback) {
            bridge->phoneme_callback(act, bridge->phoneme_callback_data);
        }
    }

    /* Trigger articulatory simulation if above threshold */
    if (bridge->config.enable_articulatory_simulation &&
        confidence >= bridge->config.phoneme_activation_threshold) {

        start_simulation(bridge, phoneme_id, confidence);

        /* Record pathway activation */
        record_pathway_activation(bridge, ML_PATH_MIRROR_TO_BROCA,
                                  confidence, phoneme_id, phoneme_id);

        /* Send to Broca */
        if (bridge->broca) {
            send_to_broca(bridge, phoneme_id, confidence);
        }
    }

    bridge->state = ML_STATE_IDLE;
    return 0;
}

int mirror_language_request_simulation(
    mirror_language_bridge_t* bridge,
    uint8_t phoneme_id,
    float activation)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_requ", 0.0f);


    if (activation < 0.0f || activation > 1.0f) {
        activation = fmaxf(0.0f, fminf(1.0f, activation));
    }

    return start_simulation(bridge, phoneme_id, activation);
}

int mirror_language_get_phoneme_activation(
    const mirror_language_bridge_t* bridge,
    uint8_t phoneme_id,
    phoneme_mirror_activation_t* activation)
{
    if (!bridge || !activation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_language_get_phoneme_activation: required parameter is NULL (bridge, activation)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_get_", 0.0f);


    if (phoneme_id >= MAX_PHONEME_TEMPLATES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "mirror_language_get_phoneme_activation: capacity exceeded");
        return -1;
    }

    *activation = bridge->phoneme_activations[phoneme_id];
    return 0;
}

//=============================================================================
// Mirror -> Wernicke Pathway
//=============================================================================

int mirror_language_get_action_semantics(
    mirror_language_bridge_t* bridge,
    uint32_t action_id,
    semantic_action_context_t* contexts,
    uint32_t max_contexts,
    uint32_t* num_retrieved)
{
    if (!bridge || !contexts || !num_retrieved) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_language_get_action_semantics: required parameter is NULL (bridge, contexts, num_retrieved)");
        return -1;
    }

    *num_retrieved = 0;

    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_get_", 0.0f);


    bridge->state = ML_STATE_SEMANTIC_RETRIEVAL;
    bridge->stats.semantic_retrievals++;

    /* Query Wernicke for semantics */
    if (bridge->wernicke && max_contexts > 0) {
        int ret = query_wernicke_semantics(bridge, action_id, &contexts[0]);
        if (ret == 0) {
            *num_retrieved = 1;

            /* Record pathway activation */
            record_pathway_activation(bridge, ML_PATH_MIRROR_TO_WERNICKE,
                                      contexts[0].relevance, action_id,
                                      contexts[0].concept_id);

            /* Invoke callback */
            if (bridge->semantic_callback) {
                bridge->semantic_callback(&contexts[0], bridge->semantic_callback_data);
            }
        }
    }

    bridge->state = ML_STATE_IDLE;
    return 0;
}

int mirror_language_get_action_words(
    const mirror_language_bridge_t* bridge,
    uint32_t action_id,
    action_word_binding_t* bindings,
    uint32_t max_bindings,
    uint32_t* num_bindings)
{
    if (!bridge || !bindings || !num_bindings) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_language_get_action_words: required parameter is NULL (bridge, bindings, num_bindings)");
        return -1;
    }

    *num_bindings = 0;

    /* Search all binding table entries for this action */
    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_get_", 0.0f);


    for (uint32_t i = 0; i < BINDING_HASH_SIZE && *num_bindings < max_bindings; i++) {
        binding_entry_t* entry = bridge->binding_table[i];
        while (entry && *num_bindings < max_bindings) {
            if (entry->is_valid && entry->binding.action_id == action_id) {
                bindings[*num_bindings] = entry->binding;
                (*num_bindings)++;
            }
            entry = entry->next;
        }
    }

    return 0;
}

//=============================================================================
// Broca -> Mirror Pathway
//=============================================================================

int mirror_language_notify_production(
    mirror_language_bridge_t* bridge,
    uint8_t phoneme_id,
    float motor_activation)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Update phoneme activation from production */
    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_noti", 0.0f);


    if (phoneme_id < MAX_PHONEME_TEMPLATES) {
        phoneme_mirror_activation_t* act = &bridge->phoneme_activations[phoneme_id];
        act->phoneme_id = phoneme_id;
        act->simulation_activation = motor_activation;
        act->timestamp_ms = get_current_time_ms();
        act->obs_type = SPEECH_OBS_COVERT_REHEARSAL;

        /* Record pathway activation */
        record_pathway_activation(bridge, ML_PATH_BROCA_TO_MIRROR,
                                  motor_activation, phoneme_id, phoneme_id);

        /* Activate mirror neurons for this phoneme */
        if (bridge->mirror) {
            /* Create action for speech production */
            action_t action = mirror_neurons_create_action(
                phoneme_id + 1000,  /* Offset for speech actions */
                "speech_production",
                NULL,
                0,
                0  /* self */
            );
            mirror_neurons_execute_action(bridge->mirror, &action);
        }
    }

    return 0;
}

float mirror_language_process_efference(
    mirror_language_bridge_t* bridge,
    uint8_t phoneme_id,
    const float* predicted_features,
    uint32_t num_features)
{
    if (!bridge || !predicted_features) {
        return -1.0f;
    }
    BRIDGE_BBB_VALIDATE(bridge, predicted_features, num_features * sizeof(float));

    /* Find phoneme template */
    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_proc", 0.0f);


    phoneme_template_t* tmpl = NULL;
    for (uint32_t i = 0; i < bridge->num_templates; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_templates > 256) {
            mirror_language_bridge_heartbeat("mirror_langu_loop",
                             (float)(i + 1) / (float)bridge->num_templates);
        }

        if (bridge->phoneme_templates[i].phoneme_id == phoneme_id &&
            bridge->phoneme_templates[i].is_registered) {
            tmpl = &bridge->phoneme_templates[i];
            break;
        }
    }

    if (!tmpl) {
        return 0.0f;  /* No template - no error */
    }

    /* Compute prediction error as 1 - similarity */
    float sim = compute_feature_similarity(
        predicted_features,
        tmpl->features,
        num_features < tmpl->num_features ? num_features : tmpl->num_features
    );

    /* Normalize similarity from [-1,1] to [0,1] */
    float normalized_sim = (sim + 1.0f) / 2.0f;

    /* Return prediction error */
    return 1.0f - normalized_sim;
}

//=============================================================================
// Wernicke -> Mirror Pathway
//=============================================================================

int mirror_language_prime_from_word(
    mirror_language_bridge_t* bridge,
    uint32_t word_id,
    const char* word_string,
    float activation)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Search bindings for this word */
    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_prim", 0.0f);


    uint32_t num_primed = 0;

    for (uint32_t i = 0; i < BINDING_HASH_SIZE; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && BINDING_HASH_SIZE > 256) {
            mirror_language_bridge_heartbeat("mirror_langu_loop",
                             (float)(i + 1) / (float)BINDING_HASH_SIZE);
        }

        binding_entry_t* entry = bridge->binding_table[i];
        while (entry) {
            if (entry->is_valid && entry->binding.word_id == word_id) {
                /* Prime this action in mirror neurons */
                if (bridge->mirror) {
                    float prime_strength = activation * entry->binding.binding_strength;

                    action_t action = mirror_neurons_create_action(
                        entry->binding.action_id,
                        entry->binding.word_string,
                        NULL,
                        0,
                        0  /* observation from language */
                    );

                    /* Observe the action to prime it */
                    mirror_neurons_observe_action(bridge->mirror, &action);

                    /* Record pathway activation */
                    record_pathway_activation(bridge, ML_PATH_WERNICKE_TO_MIRROR,
                                              prime_strength, word_id,
                                              entry->binding.action_id);

                    num_primed++;
                }
            }
            entry = entry->next;
        }
    }

    /* Also try matching word string */
    if (word_string && word_string[0] != '\0') {
        for (uint32_t i = 0; i < BINDING_HASH_SIZE; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && BINDING_HASH_SIZE > 256) {
                mirror_language_bridge_heartbeat("mirror_langu_loop",
                                 (float)(i + 1) / (float)BINDING_HASH_SIZE);
            }

            binding_entry_t* entry = bridge->binding_table[i];
            while (entry) {
                if (entry->is_valid &&
                    strncmp(entry->binding.word_string, word_string, 64) == 0) {
                    /* Skip if already primed by word_id */
                    if (entry->binding.word_id != word_id && bridge->mirror) {
                        float prime_strength = activation * entry->binding.binding_strength;

                        action_t action = mirror_neurons_create_action(
                            entry->binding.action_id,
                            word_string,
                            NULL,
                            0,
                            0
                        );

                        mirror_neurons_observe_action(bridge->mirror, &action);

                        record_pathway_activation(bridge, ML_PATH_WERNICKE_TO_MIRROR,
                                                  prime_strength, 0,
                                                  entry->binding.action_id);
                        num_primed++;
                    }
                }
                entry = entry->next;
            }
        }
    }

    return num_primed > 0 ? 0 : -1;
}

int mirror_language_get_primed_actions(
    const mirror_language_bridge_t* bridge,
    uint32_t word_id,
    uint32_t* action_ids,
    float* activations,
    uint32_t max_actions,
    uint32_t* num_actions)
{
    if (!bridge || !action_ids || !activations || !num_actions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_language_get_primed_actions: required parameter is NULL (bridge, action_ids, activations, num_actions)");
        return -1;
    }

    *num_actions = 0;

    /* Search bindings for this word */
    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_get_", 0.0f);


    for (uint32_t i = 0; i < BINDING_HASH_SIZE && *num_actions < max_actions; i++) {
        binding_entry_t* entry = bridge->binding_table[i];
        while (entry && *num_actions < max_actions) {
            if (entry->is_valid && entry->binding.word_id == word_id) {
                action_ids[*num_actions] = entry->binding.action_id;
                activations[*num_actions] = entry->binding.binding_strength;
                (*num_actions)++;
            }
            entry = entry->next;
        }
    }

    return 0;
}

//=============================================================================
// Action-Word Binding Functions
//=============================================================================

int mirror_language_create_binding(
    mirror_language_bridge_t* bridge,
    uint32_t action_id,
    uint32_t word_id,
    const char* word_string,
    action_word_binding_type_t type,
    float initial_strength)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_crea", 0.0f);


    if (bridge->num_bindings >= bridge->config.binding_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "mirror_language_create_binding: capacity exceeded");
        return -1;  /* At capacity */
    }

    action_word_binding_t binding;
    memset(&binding, 0, sizeof(binding));

    binding.action_id = action_id;
    binding.word_id = word_id;
    binding.type = type;
    binding.binding_strength = initial_strength;
    binding.coactivation_count = 0;
    binding.last_activation_ms = get_current_time_ms();
    binding.is_bidirectional = true;

    if (word_string) {
        strncpy(binding.word_string, word_string, sizeof(binding.word_string) - 1);
    }

    binding_entry_t* entry = add_binding_entry(bridge, &binding);
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entry is NULL");

        return -1;
    }

    bridge->stats.action_word_bindings++;

    /* Invoke callback */
    if (bridge->binding_callback) {
        bridge->binding_callback(&binding, bridge->binding_callback_data);
    }

    return 0;
}

float mirror_language_strengthen_binding(
    mirror_language_bridge_t* bridge,
    uint32_t action_id,
    uint32_t word_id,
    float action_activation,
    float word_activation)
{
    if (!bridge) {
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_stre", 0.0f);


    binding_entry_t* entry = find_binding(bridge, action_id, word_id);
    if (!entry) {
        return -1.0f;
    }

    /* Hebbian learning: delta_w = learning_rate * pre * post */
    float delta = bridge->config.binding_learning_rate *
                  action_activation * word_activation;

    entry->binding.binding_strength += delta;

    /* Clamp to [0, 1] */
    if (entry->binding.binding_strength > 1.0f) {
        entry->binding.binding_strength = 1.0f;
    }

    entry->binding.coactivation_count++;
    entry->binding.last_activation_ms = get_current_time_ms();

    /* Update average binding strength stat */
    /* Note: Full implementation would track all bindings */
    bridge->stats.avg_binding_strength = entry->binding.binding_strength;

    return entry->binding.binding_strength;
}

int mirror_language_get_binding(
    const mirror_language_bridge_t* bridge,
    uint32_t action_id,
    uint32_t word_id,
    action_word_binding_t* binding)
{
    if (!bridge || !binding) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_language_get_binding: required parameter is NULL (bridge, binding)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_get_", 0.0f);


    binding_entry_t* entry = find_binding(bridge, action_id, word_id);
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entry is NULL");

        return -1;
    }

    *binding = entry->binding;
    return 0;
}

int mirror_language_remove_binding(
    mirror_language_bridge_t* bridge,
    uint32_t action_id,
    uint32_t word_id)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_remo", 0.0f);


    if (remove_binding_entry(bridge, action_id, word_id)) {
        return 0;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_language_remove_binding: validation failed");
    return -1;
}

//=============================================================================
// Phoneme Mirroring Functions
//=============================================================================

int mirror_language_register_phoneme(
    mirror_language_bridge_t* bridge,
    uint8_t phoneme_id,
    const float* features,
    uint32_t num_features)
{
    if (!bridge || !features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_language_register_phoneme: required parameter is NULL (bridge, features)");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, features, num_features * sizeof(float));

    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_regi", 0.0f);


    if (bridge->num_templates >= MAX_PHONEME_TEMPLATES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "mirror_language_register_phoneme: capacity exceeded");
        return -1;
    }

    /* Find existing or use new slot */
    phoneme_template_t* tmpl = NULL;
    for (uint32_t i = 0; i < bridge->num_templates; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_templates > 256) {
            mirror_language_bridge_heartbeat("mirror_langu_loop",
                             (float)(i + 1) / (float)bridge->num_templates);
        }

        if (bridge->phoneme_templates[i].phoneme_id == phoneme_id) {
            tmpl = &bridge->phoneme_templates[i];
            break;
        }
    }

    if (!tmpl) {
        tmpl = &bridge->phoneme_templates[bridge->num_templates];
        bridge->num_templates++;
    }

    tmpl->phoneme_id = phoneme_id;
    tmpl->num_features = num_features < ARTICULATORY_FEATURE_DIM ?
                         num_features : ARTICULATORY_FEATURE_DIM;
    memcpy(tmpl->features, features, tmpl->num_features * sizeof(float));
    tmpl->is_registered = true;

    return 0;
}

int mirror_language_match_phoneme(
    const mirror_language_bridge_t* bridge,
    const float* features,
    uint32_t num_features,
    uint8_t* best_phoneme,
    float* confidence)
{
    if (!bridge || !features || !best_phoneme || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_language_match_phoneme: required parameter is NULL (bridge, features, best_phoneme, confidence)");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, features, num_features * sizeof(float));

    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_matc", 0.0f);


    return find_best_phoneme_match(bridge, features, num_features,
                                   best_phoneme, confidence);
}

//=============================================================================
// Callback Registration
//=============================================================================

int mirror_language_set_phoneme_callback(
    mirror_language_bridge_t* bridge,
    ml_phoneme_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_set_", 0.0f);


    bridge->phoneme_callback = callback;
    bridge->phoneme_callback_data = user_data;
    return 0;
}

int mirror_language_set_binding_callback(
    mirror_language_bridge_t* bridge,
    ml_binding_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_set_", 0.0f);


    bridge->binding_callback = callback;
    bridge->binding_callback_data = user_data;
    return 0;
}

int mirror_language_set_simulation_callback(
    mirror_language_bridge_t* bridge,
    ml_simulation_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_set_", 0.0f);


    bridge->simulation_callback = callback;
    bridge->simulation_callback_data = user_data;
    return 0;
}

int mirror_language_set_semantic_callback(
    mirror_language_bridge_t* bridge,
    ml_semantic_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_set_", 0.0f);


    bridge->semantic_callback = callback;
    bridge->semantic_callback_data = user_data;
    return 0;
}

//=============================================================================
// Status and Statistics
//=============================================================================

ml_bridge_state_t mirror_language_get_state(const mirror_language_bridge_t* bridge)
{
    if (!bridge) {
        return ML_STATE_ERROR;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_get_", 0.0f);


    return bridge->state;
}

int mirror_language_get_stats(
    const mirror_language_bridge_t* bridge,
    mirror_language_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_language_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_get_", 0.0f);


    return 0;
}

void mirror_language_reset_stats(mirror_language_bridge_t* bridge)
{
    if (!bridge) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_rese", 0.0f);


    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.state = bridge->state;
    bridge->stats.active_bindings = bridge->num_bindings;
    bridge->stats.active_simulations = bridge->num_active_simulations;
}

int mirror_language_get_config(
    const mirror_language_bridge_t* bridge,
    mirror_language_config_t* config)
{
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_language_get_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    *config = bridge->config;
    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_get_", 0.0f);


    return 0;
}

int mirror_language_set_config(
    mirror_language_bridge_t* bridge,
    const mirror_language_config_t* config)
{
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_language_set_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_set_", 0.0f);


    bridge->config = *config;
    return 0;
}

bool mirror_language_has_broca(const mirror_language_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_has_", 0.0f);


    return bridge->broca != NULL;
}

bool mirror_language_has_wernicke(const mirror_language_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_language_bridge_heartbeat("mirror_langu_mirror_language_has_", 0.0f);


    return bridge->wernicke != NULL;
}

//=============================================================================
// Instance Health Agent Setter (B22 Upgrade)
//=============================================================================

void mirror_language_bridge_set_instance_health_agent(
    mirror_language_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B22 Upgrade)
//=============================================================================

int mirror_language_bridge_training_begin(mirror_language_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_language_bridge_training_begin: NULL argument");
        return -1;
    }
    mirror_language_bridge_heartbeat_instance(bridge->health_agent, "mirror_language_bridge_training_begin", 0.0f);
    return 0;
}

int mirror_language_bridge_training_end(mirror_language_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_language_bridge_training_end: NULL argument");
        return -1;
    }
    mirror_language_bridge_heartbeat_instance(bridge->health_agent, "mirror_language_bridge_training_end", 1.0f);
    return 0;
}

int mirror_language_bridge_training_step(mirror_language_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_language_bridge_training_step: NULL argument");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "mirror_language_bridge_training_step");
    BRIDGE_LGSS_GATE(bridge, "mirror_language_bridge_training_step");
    mirror_language_bridge_heartbeat_instance(bridge->health_agent, "mirror_language_bridge_training_step", progress);

    /* Notify coordinator of step cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}
