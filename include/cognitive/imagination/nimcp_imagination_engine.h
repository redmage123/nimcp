/**
 * @file nimcp_imagination_engine.h
 * @brief Imagination Engine - Generative Mental Simulation System
 * @version 1.0.0
 * @date 2026-01-02
 *
 * WHAT: Central engine for generative mental simulation and hypothetical reasoning
 * WHY:  Enable AI to construct, manipulate, and explore hypothetical scenarios,
 *       visual scenes, counterfactual possibilities, and prospective simulations
 * HOW:  Integrates JEPA world model, hippocampal pattern completion, visual/audio
 *       cortex generation, and prefrontal executive control
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * MENTAL IMAGERY AND SIMULATION:
 * ------------------------------
 * 1. Prefrontal-Parietal Control Network:
 *    - Top-down control of imagery generation
 *    - Goal-directed manipulation of mental scenes
 *    - Reference: Pearson (2019) "The human imagination"
 *
 * 2. Visual Cortex Reactivation:
 *    - Imagery activates early visual areas (V1-V4)
 *    - Weaker than perception but uses same neural machinery
 *    - Reference: Kosslyn et al. (2001) "Neural foundations of imagery"
 *
 * 3. Hippocampal Scene Construction:
 *    - Hippocampus binds elements into coherent scenes
 *    - Pattern completion fills in missing details
 *    - Reference: Hassabis & Maguire (2009) "The construction system"
 *
 * 4. Default Mode Network:
 *    - Active during spontaneous imagination
 *    - Integrates memory, self-reference, and simulation
 *    - Reference: Buckner et al. (2008) "The brain's default network"
 *
 * ARCHITECTURE:
 * ```
 *                     ┌─────────────────────────────────────────┐
 *                     │         Prefrontal Cortex               │
 *                     │    (Goals, Executive Control)           │
 *                     └──────────────────┬──────────────────────┘
 *                                        │ Directives
 *                                        ▼
 * ┌──────────────────────────────────────────────────────────────────────────────┐
 * │                         IMAGINATION ENGINE                                    │
 * │                                                                               │
 * │   ┌─────────────────┐  ┌──────────────────┐  ┌─────────────────────────────┐│
 * │   │  Controller     │◄─┤  Scene Generator │──►│  Consistency Evaluator     ││
 * │   │  (Goal→Scene)   │  │  (Latent→Modal)  │  │  (Reality Check)           ││
 * │   └────────┬────────┘  └────────┬─────────┘  └─────────────┬───────────────┘│
 * │            │                    │                          │                 │
 * │            ▼                    ▼                          ▼                 │
 * │   ┌─────────────────────────────────────────────────────────────────────────┐│
 * │   │                    Imagination Workspace                                 ││
 * │   │  (Active scenario buffer, mental canvas, working imagination)           ││
 * │   └─────────────────────────────────────────────────────────────────────────┘│
 * └──────────────────────────────────────────────────────────────────────────────┘
 *          │                       │                          │
 *          ▼                       ▼                          ▼
 * ┌─────────────────┐    ┌─────────────────┐       ┌─────────────────┐
 * │  Memory Bridge  │    │  JEPA Predictor │       │  Cortex Bridge  │
 * │  (Hippocampus)  │    │  (World Model)  │       │  (Visual/Audio) │
 * └─────────────────┘    └─────────────────┘       └─────────────────┘
 * ```
 *
 * INTEGRATION POINTS:
 * - Bio-Async: Asynchronous messaging for scene generation
 * - Immune System: Inflammation modulates vividness/coherence
 * - GPU Acceleration: CUDA kernels for parallel scene generation
 * - Brain Factory: Standard brain component initialization
 * - Neural Substrate: Metabolic constraints on imagination capacity
 * - Thalamic Router: Attention-gated content routing
 * - Training Module: Learning to imagine from experience
 * - Neural Logic: Logical constraints on scene consistency
 * - Parietal Lobe: Spatial/mathematical reasoning, domain-specific simulation
 * - Quantum Reasoning: Grover-inspired search for constraint satisfaction
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_IMAGINATION_ENGINE_H
#define NIMCP_IMAGINATION_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Core utilities */
#include "utils/error/nimcp_error_codes.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/containers/nimcp_darray.h"

/* Workspace */
#include "cognitive/imagination/nimcp_imagination_workspace.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Forward Declarations
 *============================================================================*/

/* Core brain - use forward declarations that match brain.h */
#ifndef NIMCP_BRAIN_T_DEFINED
typedef struct brain_struct* brain_t;
#define NIMCP_BRAIN_T_DEFINED
#endif

/* brain_config_t is defined in brain.h as anonymous struct typedef - no forward decl needed */

/* JEPA and Cognitive */
typedef struct jepa_predictor jepa_predictor_t;
typedef struct jepa_latent jepa_latent_t;
typedef struct global_workspace_struct* global_workspace_t;

/* Brain Regions */
typedef struct prefrontal_adapter prefrontal_adapter_t;
typedef struct hippocampus_adapter hippocampus_adapter_t;
typedef struct visual_cortex_struct visual_cortex_t;
typedef struct audio_cortex_struct audio_cortex_t;

/* Perception/Generation */
typedef struct visual_training_state visual_training_state_t;

/* Integration Systems */
/* bio_module_context_t already defined in nimcp_bio_router.h (via utils/bridge/nimcp_bridge_base.h) */
typedef struct brain_immune_system brain_immune_system_t;
typedef struct neural_substrate neural_substrate_t;
typedef struct thalamic_router thalamic_router_t;
typedef struct training_context training_context_t;
typedef struct neural_logic_gate neural_logic_gate_t;

/* Theory of Mind - matches brain.h typedef */
#ifndef NIMCP_THEORY_OF_MIND_T_DEFINED
typedef struct theory_of_mind_s* theory_of_mind_t;
#define NIMCP_THEORY_OF_MIND_T_DEFINED
#endif

/* Curiosity */
typedef struct curiosity_state curiosity_state_t;

/* Sleep */
typedef struct sleep_wake_cycle sleep_wake_cycle_t;

/* GPU */
typedef struct nimcp_gpu_context nimcp_gpu_context_t;

/* Parietal Lobe - Spatial/Mathematical Reasoning */
typedef struct parietal_lobe parietal_lobe_t;
typedef struct spatial_transform spatial_transform_t;
typedef struct number_sense number_sense_t;
typedef struct mathematical_intuition mathematical_intuition_t;
typedef struct scientific_reasoning scientific_reasoning_t;

/* Parietal Domain Submodules */
typedef struct chemistry_context chemistry_context_t;
typedef struct biology_context biology_context_t;
typedef struct physics_context physics_context_t;
typedef struct software_engineering_context software_engineering_context_t;
typedef struct equation_manipulation_context equation_manipulation_context_t;

/* Quantum Reasoning */
typedef struct qreason_kb qreason_kb_t;
typedef struct qreason_qstate qreason_qstate_t;

/* Swarm/Collective Consciousness */
typedef struct swarm_consciousness_ctx swarm_consciousness_ctx_t;

/* Internal Knowledge Graph */
typedef struct brain_kg brain_kg_t;
typedef struct qreason_cnf qreason_cnf_t;

/**
 * @brief Result from quantum-inspired reasoning
 */
typedef struct qreason_result {
    bool satisfied;           /**< Whether constraints were satisfied */
    float probability;        /**< Probability/confidence of result */
    size_t iterations_used;   /**< Number of iterations performed */
    int* assignments;         /**< Variable assignments (caller allocates) */
    size_t n_assignments;     /**< Number of assignments */
} qreason_result_t;

/*============================================================================
 * Constants
 *============================================================================*/

/** @brief Bio-async module ID for imagination engine */
#define BIO_MODULE_IMAGINATION              0x1A01

/** @brief Maximum imagination dimensions */
#define IMAGINATION_MAX_LATENT_DIM          512
#define IMAGINATION_MAX_VISUAL_DIM          16384   /* 128x128 */
#define IMAGINATION_MAX_AUDIO_DIM           8192

/** @brief Default configuration values */
#define IMAGINATION_DEFAULT_VIVIDNESS       0.7f
#define IMAGINATION_DEFAULT_NOISE_LEVEL     0.1f
#define IMAGINATION_DEFAULT_COHERENCE_THRESHOLD 0.5f

/** @brief Imagination mode timeouts (ms) */
#define IMAGINATION_DIRECTED_TIMEOUT_MS     5000
#define IMAGINATION_CREATIVE_TIMEOUT_MS     10000

/*============================================================================
 * Enumerations
 *============================================================================*/

/**
 * @brief Imagination operating modes
 *
 * WHAT: Different modes of imagination with distinct characteristics
 * WHY:  Different cognitive tasks require different imagination styles
 * HOW:  Mode determines control strength, noise level, and evaluation
 */
typedef enum {
    IMAGINATION_MODE_PASSIVE = 0,       /**< Background, undirected daydreaming */
    IMAGINATION_MODE_DIRECTED,          /**< Goal-directed visualization */
    IMAGINATION_MODE_COUNTERFACTUAL,    /**< "What if" scenarios */
    IMAGINATION_MODE_PROSPECTIVE,       /**< Future planning/simulation */
    IMAGINATION_MODE_RETROSPECTIVE,     /**< Memory reconstruction */
    IMAGINATION_MODE_CREATIVE,          /**< Novel combination, REM-like */
    IMAGINATION_MODE_SOCIAL,            /**< Theory of Mind simulation */
    IMAGINATION_MODE_SPATIAL,           /**< Parietal: mental rotation, spatial transforms */
    IMAGINATION_MODE_MATHEMATICAL,      /**< Parietal: numerical/symbolic reasoning */
    IMAGINATION_MODE_SCIENTIFIC,        /**< Parietal: hypothesis testing, causal inference */
    IMAGINATION_MODE_DOMAIN_SIMULATION, /**< Domain-specific simulation (chem/bio/phys) */
    IMAGINATION_MODE_QUANTUM_SEARCH,    /**< Quantum-inspired constraint satisfaction */
    IMAGINATION_MODE_COUNT
} imagination_mode_t;

/**
 * @brief Imagination generation quality levels
 */
typedef enum {
    IMAGINATION_QUALITY_DRAFT = 0,      /**< Fast, low detail */
    IMAGINATION_QUALITY_NORMAL,         /**< Balanced quality/speed */
    IMAGINATION_QUALITY_HIGH,           /**< High detail, slower */
    IMAGINATION_QUALITY_VIVID           /**< Maximum vividness */
} imagination_quality_t;

/**
 * @brief Scene element types
 */
typedef enum {
    ELEMENT_TYPE_OBJECT = 0,            /**< Physical object */
    ELEMENT_TYPE_AGENT,                 /**< Person/agent */
    ELEMENT_TYPE_LOCATION,              /**< Place/setting */
    ELEMENT_TYPE_ACTION,                /**< Event/action */
    ELEMENT_TYPE_EMOTION,               /**< Emotional tone */
    ELEMENT_TYPE_ABSTRACT               /**< Abstract concept */
} imagination_element_type_t;

/*============================================================================
 * Core Structures
 *============================================================================*/

/**
 * @brief Imagination goal specification
 *
 * WHAT: Describes what to imagine
 * WHY:  Provides top-down constraints for directed imagination
 */
typedef struct imagination_goal {
    imagination_mode_t mode;            /**< Imagination mode */
    nimcp_tensor_t* target_features;    /**< Target scene features (optional) */
    nimcp_tensor_t* constraints;        /**< Must-have constraints */
    nimcp_tensor_t* avoid;              /**< Features to avoid */
    float priority;                     /**< Goal priority [0-1] */
    uint64_t deadline_ms;               /**< Optional timeout */
    void* context;                      /**< User context */
} imagination_goal_t;

/**
 * @brief Scene element for injection/removal
 */
typedef struct imagination_element {
    uint64_t id;                        /**< Element ID */
    imagination_element_type_t type;    /**< Element type */
    nimcp_tensor_t* features;           /**< Element features in latent space */
    float salience;                     /**< Element salience [0-1] */
    float* position;                    /**< Spatial position (if applicable) */
    size_t position_dim;                /**< Position dimensionality */
} imagination_element_t;

/**
 * @brief Scene transformation
 */
typedef struct imagination_transform {
    nimcp_tensor_t* rotation;           /**< Rotation/viewpoint change */
    nimcp_tensor_t* translation;        /**< Spatial shift */
    nimcp_tensor_t* scaling;            /**< Size change */
    float time_delta;                   /**< Temporal progression */
    nimcp_tensor_t* style_transfer;     /**< Style/mood transformation */
} imagination_transform_t;

/**
 * @brief Counterfactual query
 */
typedef struct counterfactual_query {
    nimcp_tensor_t* original_state;     /**< Original memory state */
    nimcp_tensor_t* intervention;       /**< What to change */
    size_t steps_forward;               /**< How far to simulate */
    bool preserve_agents;               /**< Keep agent identities */
} counterfactual_query_t;

/**
 * @brief Imagination scenario (active imagination instance)
 */
typedef struct imagination_scenario {
    scenario_id_t id;                   /**< Unique scenario ID */
    imagination_mode_t mode;            /**< Operating mode */
    imagination_quality_t quality;      /**< Generation quality */

    /* Latent state */
    nimcp_tensor_t* latent_state;       /**< Current scene in latent space */
    nimcp_tensor_t* latent_previous;    /**< Previous state for continuity */

    /* Generated modalities */
    nimcp_tensor_t* visual_buffer;      /**< Generated visual content */
    nimcp_tensor_t* audio_buffer;       /**< Generated audio content */
    nimcp_tensor_t* semantic_buffer;    /**< Semantic/language content */

    /* Quality metrics */
    float vividness;                    /**< Clarity of imagination [0-1] */
    float controllability;              /**< Ease of manipulation [0-1] */
    float coherence;                    /**< Internal consistency [0-1] */
    float reality_distance;             /**< Distance from reality [0-1] */
    float novelty;                      /**< Novelty score [0-1] */

    /* Timing */
    uint64_t start_time_ms;             /**< When scenario started */
    uint64_t duration_ms;               /**< Total duration */
    uint64_t last_step_ms;              /**< Last step timestamp */

    /* Trajectory */
    nimcp_darray_t* trajectory;         /**< Sequence of scene states (nimcp_tensor_t*) */
    size_t trajectory_length;           /**< Current trajectory length */

    /* Goal tracking */
    imagination_goal_t* active_goal;    /**< Current goal (if directed) */
    float goal_progress;                /**< Progress toward goal [0-1] */

    /* Elements */
    nimcp_darray_t* elements;           /**< Active scene elements (imagination_element_t*) */

    /* Status */
    bool is_active;                     /**< Currently running */
    bool is_paused;                     /**< Temporarily paused */
    int error_code;                     /**< Last error (0=none) */
} imagination_scenario_t;

/**
 * @brief Imagination evaluation result
 */
typedef struct imagination_evaluation {
    float coherence;                    /**< Scene coherence [0-1] */
    float plausibility;                 /**< Physical plausibility [0-1] */
    float reality_distance;             /**< Distance from reality */
    float goal_alignment;               /**< Alignment with goal */
    float novelty;                      /**< Novelty score [0-1] */
    bool is_valid;                      /**< Passes all checks */
    char* issues;                       /**< Description of issues (if any) */
} imagination_evaluation_t;

/**
 * @brief Imagination statistics
 */
typedef struct imagination_stats {
    uint64_t scenarios_created;         /**< Total scenarios created */
    uint64_t scenarios_completed;       /**< Completed scenarios */
    uint64_t scenarios_failed;          /**< Failed scenarios */
    uint64_t total_steps;               /**< Total simulation steps */
    uint64_t visual_generations;        /**< Visual generations */
    uint64_t audio_generations;         /**< Audio generations */
    float avg_vividness;                /**< Average vividness */
    float avg_coherence;                /**< Average coherence */
    float avg_duration_ms;              /**< Average scenario duration */
    uint64_t gpu_time_ms;               /**< GPU time used */
    float memory_usage_mb;              /**< Memory usage */
} imagination_stats_t;

/**
 * @brief Imagination engine configuration
 */
typedef struct imagination_engine_config {
    /* Capacity */
    size_t max_concurrent_scenarios;    /**< Max simultaneous scenarios */
    size_t workspace_capacity;          /**< Workspace buffer size */
    size_t latent_dim;                  /**< Latent space dimensionality */

    /* Quality defaults */
    imagination_quality_t default_quality; /**< Default generation quality */
    float default_vividness;            /**< Default vividness target */
    float creativity_noise_level;       /**< REM-like noise injection */
    float coherence_threshold;          /**< Min coherence before reset */

    /* Feature flags */
    bool enable_reality_checking;       /**< Reality/coherence evaluation */
    bool enable_memory_integration;     /**< Hippocampal integration */
    bool enable_prospective_mode;       /**< Future simulation */
    bool enable_counterfactual;         /**< What-if reasoning */
    bool enable_social_simulation;      /**< Theory of Mind integration */

    /* Integration flags */
    bool enable_bio_async;              /**< Bio-async messaging */
    bool enable_immune_modulation;      /**< Immune system effects */
    bool enable_gpu_acceleration;       /**< CUDA acceleration */
    bool enable_thalamic_routing;       /**< Thalamic gating */
    bool enable_substrate_constraints;  /**< Metabolic constraints */
    bool enable_training_feedback;      /**< Training integration */
    bool enable_logic_constraints;      /**< Neural logic integration */
    bool enable_parietal_integration;   /**< Parietal lobe reasoning */
    bool enable_quantum_reasoning;      /**< Quantum-inspired search */
    bool enable_domain_simulation;      /**< Domain-specific simulation */

    /* GPU settings */
    int gpu_device_id;                  /**< GPU device (-1 = auto) */
    size_t gpu_batch_size;              /**< GPU batch size */

    /* Timeouts */
    uint64_t default_timeout_ms;        /**< Default scenario timeout */
    uint64_t step_timeout_ms;           /**< Per-step timeout */

} imagination_engine_config_t;

/**
 * @brief Imagination Engine - Main structure
 *
 * Uses bridge pattern for integration with NIMCP infrastructure
 */
typedef struct imagination_engine {
    bridge_base_t base;                 /**< MUST be first - bridge pattern */

    /* Configuration */
    imagination_engine_config_t config;

    /* Core components */
    imagination_workspace_t* workspace;

    /* Connections to cognitive systems */
    jepa_predictor_t* world_model;      /**< JEPA for latent prediction */
    global_workspace_t* global_workspace; /**< Conscious broadcast */
    prefrontal_adapter_t* prefrontal;   /**< Executive control */
    hippocampus_adapter_t* hippocampus; /**< Memory integration */
    visual_cortex_t* visual_cortex;     /**< Visual generation */
    audio_cortex_t* audio_cortex;       /**< Audio generation */
    theory_of_mind_t* tom;              /**< Social simulation */
    curiosity_state_t* curiosity;       /**< Novelty/exploration */
    sleep_wake_cycle_t* sleep;          /**< REM creativity */

    /* Integration bridges */
    bio_module_context_t* bio_context;  /**< Bio-async context */
    brain_immune_system_t* immune;      /**< Immune modulation */
    neural_substrate_t* substrate;      /**< Metabolic constraints */
    thalamic_router_t* thalamic;        /**< Attention routing */
    training_context_t* training;       /**< Training feedback */
    neural_logic_gate_t* logic;         /**< Logical constraints */

    /* Parietal Lobe - Spatial/Mathematical Reasoning */
    parietal_lobe_t* parietal;          /**< Parietal lobe for domain reasoning */
    spatial_transform_t* spatial;       /**< Spatial transformations */
    number_sense_t* number_sense;       /**< Numerical intuition */
    mathematical_intuition_t* math_intuition; /**< Mathematical patterns */
    scientific_reasoning_t* scientific; /**< Scientific reasoning */

    /* Parietal Domain Submodules (for domain-specific simulation) */
    chemistry_context_t* chemistry;     /**< Chemistry simulation */
    biology_context_t* biology;         /**< Biology simulation */
    physics_context_t* physics;         /**< Physics simulation */
    software_engineering_context_t* software; /**< Code/algorithm simulation */
    equation_manipulation_context_t* equations; /**< Symbolic math */

    /* Quantum Reasoning */
    qreason_kb_t* quantum_kb;           /**< Quantum knowledge base */
    qreason_qstate_t* quantum_state;    /**< Quantum superposition state */
    bool quantum_enabled;               /**< Quantum reasoning enabled */

    /* Collective Consciousness */
    swarm_consciousness_ctx_t* collective; /**< Swarm consciousness connection */

    /* Internal Knowledge Graph */
    brain_kg_t* kg;                     /**< Brain KG registration */
    uint32_t kg_node_id;                /**< Node ID in brain KG */

    /* GPU acceleration */
    nimcp_gpu_context_t* gpu_ctx;       /**< GPU context */
    bool gpu_available;                 /**< GPU is available */

    /* Brain reference */
    brain_t brain;                      /**< Parent brain (optional) */

    /* Active state */
    nimcp_darray_t* active_scenarios;   /**< Active scenario list (imagination_scenario_t*) */
    imagination_mode_t current_mode;    /**< Current default mode */
    scenario_id_t next_scenario_id;     /**< Next scenario ID */

    /* Statistics */
    imagination_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;

} imagination_engine_t;

/*============================================================================
 * Configuration API
 *============================================================================*/

/**
 * @brief Get default imagination engine configuration
 *
 * @return Default configuration with sensible values
 */
imagination_engine_config_t imagination_engine_default_config(void);

/**
 * @brief Validate configuration
 *
 * @param config Configuration to validate
 * @param error_msg Output error message buffer (optional)
 * @param error_msg_len Error message buffer size
 * @return true if valid
 */
bool imagination_engine_validate_config(
    const imagination_engine_config_t* config,
    char* error_msg,
    size_t error_msg_len);

/*============================================================================
 * Lifecycle API
 *============================================================================*/

/**
 * @brief Create imagination engine
 *
 * WHAT: Allocate and initialize imagination engine
 * WHY:  Central coordination for mental simulation
 * HOW:  Initialize workspace, connect components, setup GPU if available
 *
 * @param config Configuration (NULL for defaults)
 * @return New engine or NULL on failure
 */
imagination_engine_t* imagination_engine_create(
    const imagination_engine_config_t* config);

/**
 * @brief Destroy imagination engine
 *
 * @param engine Engine to destroy (NULL-safe)
 */
void imagination_engine_destroy(imagination_engine_t* engine);

/**
 * @brief Reset engine to initial state
 *
 * @param engine Engine to reset
 * @return 0 on success, negative on error
 */
int imagination_engine_reset(imagination_engine_t* engine);

/**
 * @brief Copy the active imagined scenario's content vector (Tier 1 Step E).
 *
 * Read-only snapshot for the language cascade's content blend: lets
 * production reflect an active imagined scene. Prefers the scenario's
 * semantic_buffer (language-space) and falls back to latent_state. The copy
 * happens under the engine lock so the data stays valid after return (no
 * borrowed-pointer UAF). Only F32 tensors are copied; non-finite values are
 * zeroed.
 *
 * @param engine        Engine (NULL → returns 0).
 * @param caller_buf    Destination buffer (caller-owned).
 * @param caller_cap    Capacity of caller_buf in floats (>0).
 * @param out_vividness Optional; receives the scenario vividness [0-1].
 * @return Number of floats copied. 0 means idle (no active/usable scenario).
 */
uint32_t imagination_engine_copy_active_vector(
    imagination_engine_t* engine,
    float* caller_buf,
    uint32_t caller_cap,
    float* out_vividness);

/*============================================================================
 * Brain Factory Integration
 *============================================================================*/

/**
 * @brief Initialize imagination engine for brain
 *
 * WHAT: Factory function to create and attach engine to brain
 * WHY:  Standard brain component initialization pattern
 *
 * @param brain Brain to attach to
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, negative on error
 */
int imagination_engine_init_for_brain(
    brain_t brain,
    const imagination_engine_config_t* config);

/**
 * @brief Get imagination engine from brain
 *
 * @param brain Brain instance
 * @return Engine or NULL if not initialized
 */
imagination_engine_t* brain_get_imagination_engine(brain_t brain);

/*============================================================================
 * Connection API - Connect to Cognitive Systems
 *============================================================================*/

/**
 * @brief Connect JEPA world model
 */
int imagination_engine_connect_world_model(
    imagination_engine_t* engine,
    jepa_predictor_t* jepa);

/**
 * @brief Connect hippocampus for memory-based imagination
 */
int imagination_engine_connect_hippocampus(
    imagination_engine_t* engine,
    hippocampus_adapter_t* hipp);

/**
 * @brief Connect visual cortex for visual generation
 */
int imagination_engine_connect_visual(
    imagination_engine_t* engine,
    visual_cortex_t* visual);

/**
 * @brief Connect audio cortex for audio generation
 */
int imagination_engine_connect_audio(
    imagination_engine_t* engine,
    audio_cortex_t* audio);

/**
 * @brief Connect prefrontal cortex for executive control
 */
int imagination_engine_connect_prefrontal(
    imagination_engine_t* engine,
    prefrontal_adapter_t* pfc);

/**
 * @brief Connect global workspace for conscious broadcast
 */
int imagination_engine_connect_global_workspace(
    imagination_engine_t* engine,
    global_workspace_t* gw);

/**
 * @brief Connect Theory of Mind for social simulation
 */
int imagination_engine_connect_tom(
    imagination_engine_t* engine,
    theory_of_mind_t* tom);

/**
 * @brief Connect curiosity for novelty-driven imagination
 */
int imagination_engine_connect_curiosity(
    imagination_engine_t* engine,
    curiosity_state_t* curiosity);

/**
 * @brief Connect sleep system for REM-like creativity
 */
int imagination_engine_connect_sleep(
    imagination_engine_t* engine,
    sleep_wake_cycle_t* sleep);

/*============================================================================
 * Integration Bridges API
 *============================================================================*/

/**
 * @brief Connect bio-async messaging
 */
int imagination_engine_connect_bio_async(
    imagination_engine_t* engine,
    bio_module_context_t* bio_ctx);

/**
 * @brief Connect immune system for modulation
 */
int imagination_engine_connect_immune(
    imagination_engine_t* engine,
    brain_immune_system_t* immune);

/**
 * @brief Connect neural substrate for metabolic constraints
 */
int imagination_engine_connect_substrate(
    imagination_engine_t* engine,
    neural_substrate_t* substrate);

/**
 * @brief Connect thalamic router for attention gating
 */
int imagination_engine_connect_thalamic(
    imagination_engine_t* engine,
    thalamic_router_t* thalamic);

/**
 * @brief Connect training module for learning
 */
int imagination_engine_connect_training(
    imagination_engine_t* engine,
    training_context_t* training);

/**
 * @brief Connect neural logic for constraint checking
 */
int imagination_engine_connect_logic(
    imagination_engine_t* engine,
    neural_logic_gate_t* logic);

/**
 * @brief Initialize GPU acceleration
 */
int imagination_engine_init_gpu(
    imagination_engine_t* engine,
    int device_id);

/*============================================================================
 * Parietal Lobe Integration API
 *============================================================================*/

/**
 * @brief Connect parietal lobe for spatial/mathematical reasoning
 *
 * WHAT: Enable parietal-mediated reasoning in imagination
 * WHY:  Spatial manipulation, numerical intuition, domain knowledge
 */
int imagination_engine_connect_parietal(
    imagination_engine_t* engine,
    parietal_lobe_t* parietal);

/**
 * @brief Connect spatial reasoning for mental rotation/transformation
 */
int imagination_engine_connect_spatial(
    imagination_engine_t* engine,
    spatial_transform_t* spatial);

/**
 * @brief Connect number sense for numerical imagination
 */
int imagination_engine_connect_number_sense(
    imagination_engine_t* engine,
    number_sense_t* number_sense);

/**
 * @brief Connect mathematical intuition
 */
int imagination_engine_connect_math_intuition(
    imagination_engine_t* engine,
    mathematical_intuition_t* math);

/**
 * @brief Connect scientific reasoning
 */
int imagination_engine_connect_scientific(
    imagination_engine_t* engine,
    scientific_reasoning_t* scientific);

/*============================================================================
 * Domain Simulation API
 *============================================================================*/

/**
 * @brief Connect chemistry context for molecular simulation
 */
int imagination_engine_connect_chemistry(
    imagination_engine_t* engine,
    chemistry_context_t* chemistry);

/**
 * @brief Connect biology context for biological simulation
 */
int imagination_engine_connect_biology(
    imagination_engine_t* engine,
    biology_context_t* biology);

/**
 * @brief Connect physics context for physics simulation
 */
int imagination_engine_connect_physics(
    imagination_engine_t* engine,
    physics_context_t* physics);

/**
 * @brief Connect software engineering for code/algorithm visualization
 */
int imagination_engine_connect_software(
    imagination_engine_t* engine,
    software_engineering_context_t* software);

/**
 * @brief Connect equation manipulation for symbolic math
 */
int imagination_engine_connect_equations(
    imagination_engine_t* engine,
    equation_manipulation_context_t* equations);

/*============================================================================
 * Quantum Reasoning Integration API
 *============================================================================*/

/**
 * @brief Connect quantum reasoning knowledge base
 *
 * WHAT: Enable quantum-inspired reasoning in imagination
 * WHY:  Grover-style search for constraint satisfaction, superposition exploration
 */
int imagination_engine_connect_quantum_kb(
    imagination_engine_t* engine,
    qreason_kb_t* kb);

/**
 * @brief Initialize quantum state for imagination
 *
 * @param engine Engine instance
 * @param num_qubits Number of qubits (dimensions to explore)
 * @return 0 on success
 */
int imagination_engine_init_quantum_state(
    imagination_engine_t* engine,
    size_t num_qubits);

/**
 * @brief Enable/disable quantum reasoning
 */
int imagination_engine_set_quantum_enabled(
    imagination_engine_t* engine,
    bool enabled);

/*============================================================================
 * Collective Consciousness Integration
 *============================================================================*/

/**
 * @brief Connect swarm/collective consciousness
 *
 * WHAT: Enable collective imagination across swarm nodes
 * WHY:  Shared imagination scenarios, distributed counterfactual reasoning,
 *       collective creative recombination, swarm-level prospective simulation
 *
 * @param engine Engine instance
 * @param collective Collective consciousness context
 * @return 0 on success, -1 on failure
 */
int imagination_engine_connect_collective(
    imagination_engine_t* engine,
    swarm_consciousness_ctx_t* collective);

/**
 * @brief Broadcast imagination scenario to collective
 *
 * WHAT: Share current scenario with swarm consciousness
 * WHY:  Enable collective processing and emergent insights
 *
 * @param engine Engine instance
 * @param scenario Scenario to broadcast
 * @return 0 on success, -1 on failure
 */
int imagination_broadcast_to_collective(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario);

/**
 * @brief Receive collective imagination insights
 *
 * WHAT: Gather insights from swarm nodes into scenario
 * WHY:  Integrate collective perspectives into imagination
 *
 * @param engine Engine instance
 * @param scenario Scenario to update
 * @return Number of insights received, -1 on failure
 */
int imagination_receive_collective_insights(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario);

/*============================================================================
 * Internal Knowledge Graph Registration
 *============================================================================*/

/**
 * @brief Register imagination engine with brain's internal KG
 *
 * WHAT: Add imagination engine node and edges to brain KG
 * WHY:  Enable brain self-awareness of imagination capabilities and connections
 *
 * @param engine Engine instance
 * @param kg Brain internal knowledge graph
 * @return 0 on success, -1 on failure
 */
int imagination_engine_register_with_kg(
    imagination_engine_t* engine,
    brain_kg_t* kg);

/*============================================================================
 * Scenario Management API
 *============================================================================*/

/**
 * @brief Begin new imagination scenario
 *
 * WHAT: Start a new imagination scenario with specified mode and goal
 * WHY:  Entry point for all imagination activities
 *
 * @param engine Engine instance
 * @param mode Imagination mode
 * @param goal Goal specification (optional, can be NULL for passive)
 * @return New scenario or NULL on failure
 */
imagination_scenario_t* imagination_begin_scenario(
    imagination_engine_t* engine,
    imagination_mode_t mode,
    const imagination_goal_t* goal);

/**
 * @brief Step scenario forward
 *
 * WHAT: Advance scenario by one step
 * WHY:  Iterative scene evolution
 *
 * @param engine Engine instance
 * @param scenario Scenario to step
 * @return 0 on success, negative on error
 */
int imagination_step_scenario(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario);

/**
 * @brief End and cleanup scenario
 *
 * @param engine Engine instance
 * @param scenario Scenario to end
 * @return 0 on success
 */
int imagination_end_scenario(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario);

/**
 * @brief Pause active scenario
 */
int imagination_pause_scenario(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario);

/**
 * @brief Resume paused scenario
 */
int imagination_resume_scenario(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario);

/*============================================================================
 * Generation API
 *============================================================================*/

/**
 * @brief Generate visual content for scenario
 *
 * WHAT: Decode latent state to visual representation
 * WHY:  Create "mental image" from latent scene
 *
 * @param engine Engine instance
 * @param scenario Active scenario
 * @return 0 on success, negative on error
 */
int imagination_generate_visual(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario);

/**
 * @brief Generate audio content for scenario
 */
int imagination_generate_audio(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario);

/**
 * @brief Generate both visual and audio
 */
int imagination_generate_multimodal(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario);

/*============================================================================
 * Manipulation API
 *============================================================================*/

/**
 * @brief Apply transformation to scene
 *
 * WHAT: Transform current scene (rotate, translate, time-advance)
 * WHY:  Manipulate mental imagery
 *
 * @param engine Engine instance
 * @param scenario Active scenario
 * @param transform Transformation to apply
 * @return 0 on success
 */
int imagination_transform_scene(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario,
    const imagination_transform_t* transform);

/**
 * @brief Inject element into scene
 *
 * @param engine Engine instance
 * @param scenario Active scenario
 * @param element Element to add
 * @return Element ID or 0 on failure
 */
uint64_t imagination_inject_element(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario,
    const imagination_element_t* element);

/**
 * @brief Remove element from scene
 *
 * @param engine Engine instance
 * @param scenario Active scenario
 * @param element_id Element to remove
 * @return 0 on success
 */
int imagination_remove_element(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario,
    uint64_t element_id);

/**
 * @brief Blend two scenarios
 *
 * @param engine Engine instance
 * @param scenario_a First scenario
 * @param scenario_b Second scenario
 * @param alpha Blend factor [0=A, 1=B]
 * @return New blended scenario or NULL
 */
imagination_scenario_t* imagination_blend_scenarios(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario_a,
    imagination_scenario_t* scenario_b,
    float alpha);

/*============================================================================
 * Advanced Imagination Modes
 *============================================================================*/

/**
 * @brief Counterfactual reasoning
 *
 * WHAT: "What if X had happened differently?"
 * WHY:  Causal reasoning and learning from alternatives
 *
 * @param engine Engine instance
 * @param memory Original memory to modify
 * @param query Counterfactual query
 * @return Counterfactual scenario or NULL
 */
imagination_scenario_t* imagination_counterfactual(
    imagination_engine_t* engine,
    const nimcp_tensor_t* memory,
    const counterfactual_query_t* query);

/**
 * @brief Prospective simulation (future prediction)
 *
 * WHAT: Simulate future outcomes of actions
 * WHY:  Planning and decision-making
 *
 * @param engine Engine instance
 * @param current_state Current state
 * @param actions Action sequence to simulate
 * @param num_actions Number of actions
 * @param steps_ahead How far to simulate
 * @return Prospective scenario or NULL
 */
imagination_scenario_t* imagination_simulate_future(
    imagination_engine_t* engine,
    const nimcp_tensor_t* current_state,
    const nimcp_tensor_t* actions,
    size_t num_actions,
    size_t steps_ahead);

/**
 * @brief Social simulation (Theory of Mind)
 *
 * WHAT: Simulate another agent's perspective/behavior
 * WHY:  Social cognition, empathy, prediction of others
 *
 * @param engine Engine instance
 * @param agent_id Agent to simulate
 * @param believed_state Our belief about their mental state
 * @return Social simulation scenario or NULL
 */
imagination_scenario_t* imagination_simulate_agent(
    imagination_engine_t* engine,
    uint64_t agent_id,
    const nimcp_tensor_t* believed_state);

/**
 * @brief Creative recombination (dream-like)
 *
 * WHAT: Novel combination of memory elements
 * WHY:  Creativity, insight, problem-solving
 *
 * @param engine Engine instance
 * @param seed_memories Array of memory tensors to recombine
 * @param num_memories Number of seed memories
 * @param creativity_level Noise/recombination level [0-1]
 * @return Creative scenario or NULL
 */
imagination_scenario_t* imagination_creative_recombine(
    imagination_engine_t* engine,
    nimcp_tensor_t** seed_memories,
    size_t num_memories,
    float creativity_level);

/*============================================================================
 * Parietal-Mediated Imagination API
 *============================================================================*/

/**
 * @brief Mental rotation simulation
 *
 * WHAT: Rotate objects in mental space
 * WHY:  Spatial reasoning, visualizing transformations
 *
 * @param engine Engine instance
 * @param scenario Active scenario
 * @param rotation_axis Axis of rotation (3D vector)
 * @param angle_radians Rotation angle
 * @return 0 on success
 */
int imagination_mental_rotate(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario,
    const float* rotation_axis,
    float angle_radians);

/**
 * @brief Numerical/quantity imagination
 *
 * WHAT: Imagine quantities, magnitudes, numerical relationships
 * WHY:  Support mathematical intuition
 *
 * @param engine Engine instance
 * @param quantity Base quantity to imagine
 * @param scale Scale factor
 * @return Scenario with numerical visualization or NULL
 */
imagination_scenario_t* imagination_numerical(
    imagination_engine_t* engine,
    double quantity,
    double scale);

/**
 * @brief Mathematical visualization
 *
 * WHAT: Visualize mathematical concepts/equations
 * WHY:  Support mathematical reasoning and insight
 *
 * @param engine Engine instance
 * @param equation_latent Latent representation of equation
 * @param variables Variables to manipulate
 * @param num_vars Number of variables
 * @return Mathematical imagination scenario or NULL
 */
imagination_scenario_t* imagination_mathematical(
    imagination_engine_t* engine,
    const nimcp_tensor_t* equation_latent,
    const float* variables,
    size_t num_vars);

/**
 * @brief Scientific hypothesis simulation
 *
 * WHAT: Simulate outcomes of scientific hypotheses
 * WHY:  Support scientific reasoning and prediction
 *
 * @param engine Engine instance
 * @param hypothesis Hypothesis representation
 * @param initial_conditions Initial state
 * @param steps Simulation steps
 * @return Scientific simulation scenario or NULL
 */
imagination_scenario_t* imagination_scientific_simulate(
    imagination_engine_t* engine,
    const nimcp_tensor_t* hypothesis,
    const nimcp_tensor_t* initial_conditions,
    size_t steps);

/*============================================================================
 * Domain-Specific Simulation API
 *============================================================================*/

/**
 * @brief Chemistry domain simulation
 *
 * WHAT: Visualize molecular structures, reactions
 * WHY:  Chemistry reasoning and discovery
 */
imagination_scenario_t* imagination_simulate_chemistry(
    imagination_engine_t* engine,
    const nimcp_tensor_t* molecules,
    const nimcp_tensor_t* reaction_conditions);

/**
 * @brief Biology domain simulation
 *
 * WHAT: Visualize biological processes, systems
 * WHY:  Biology reasoning and understanding
 */
imagination_scenario_t* imagination_simulate_biology(
    imagination_engine_t* engine,
    const nimcp_tensor_t* biological_system,
    const nimcp_tensor_t* perturbation);

/**
 * @brief Physics domain simulation
 *
 * WHAT: Visualize physical systems, forces, motion
 * WHY:  Physics intuition and prediction
 */
imagination_scenario_t* imagination_simulate_physics(
    imagination_engine_t* engine,
    const nimcp_tensor_t* physical_state,
    const nimcp_tensor_t* forces,
    float time_delta);

/**
 * @brief Software/algorithm visualization
 *
 * WHAT: Visualize code execution, data structures, algorithms
 * WHY:  Programming intuition and debugging
 */
imagination_scenario_t* imagination_simulate_software(
    imagination_engine_t* engine,
    const nimcp_tensor_t* program_state,
    size_t execution_steps);

/*============================================================================
 * Quantum-Inspired Reasoning API
 *============================================================================*/

/**
 * @brief Quantum superposition exploration
 *
 * WHAT: Explore multiple possibilities in superposition
 * WHY:  Parallel hypothesis exploration
 *
 * @param engine Engine instance
 * @param possibilities Array of possibility tensors
 * @param num_possibilities Number of possibilities
 * @param amplitudes Probability amplitudes for each
 * @return Superposition scenario or NULL
 */
imagination_scenario_t* imagination_quantum_superpose(
    imagination_engine_t* engine,
    nimcp_tensor_t** possibilities,
    size_t num_possibilities,
    const float* amplitudes);

/**
 * @brief Quantum-inspired constraint satisfaction
 *
 * WHAT: Use Grover-style search to find satisfying assignment
 * WHY:  Efficient constraint satisfaction in imagination
 *
 * @param engine Engine instance
 * @param constraints CNF formula representing constraints
 * @param max_iterations Maximum Grover iterations
 * @param result Output result
 * @return 0 on success, negative on error
 */
int imagination_quantum_solve(
    imagination_engine_t* engine,
    const qreason_cnf_t* constraints,
    size_t max_iterations,
    qreason_result_t* result);

/**
 * @brief Collapse quantum superposition to concrete scenario
 *
 * @param engine Engine instance
 * @param scenario Superposition scenario
 * @return Collapsed concrete scenario or NULL
 */
imagination_scenario_t* imagination_quantum_collapse(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario);

/*============================================================================
 * Evaluation API
 *============================================================================*/

/**
 * @brief Evaluate scenario coherence and quality
 *
 * @param engine Engine instance
 * @param scenario Scenario to evaluate
 * @param result Output evaluation result
 * @return 0 on success
 */
int imagination_evaluate(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario,
    imagination_evaluation_t* result);

/**
 * @brief Check if scenario is physically plausible
 */
float imagination_check_plausibility(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario);

/**
 * @brief Measure distance from reality/memory
 */
float imagination_reality_distance(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario);

/*============================================================================
 * Bio-Async Integration
 *============================================================================*/

/**
 * @brief Process pending bio-async messages
 *
 * @param engine Engine instance
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t imagination_process_bio_messages(
    imagination_engine_t* engine,
    uint32_t max_messages);

/**
 * @brief Broadcast imagination state to global workspace
 *
 * @param engine Engine instance
 * @param scenario Scenario to broadcast
 * @param salience Broadcast salience
 * @return 0 on success
 */
int imagination_broadcast_to_workspace(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario,
    float salience);

/*============================================================================
 * Immune System Modulation
 *============================================================================*/

/**
 * @brief Update imagination based on immune state
 *
 * WHAT: Inflammation reduces vividness/coherence
 * WHY:  Biological basis - sickness impairs cognition
 *
 * @param engine Engine instance
 * @return 0 on success
 */
int imagination_update_immune_modulation(imagination_engine_t* engine);

/**
 * @brief Get current immune modulation factor
 *
 * @param engine Engine instance
 * @return Modulation factor [0-1], 1.0 = healthy
 */
float imagination_get_immune_modulation(const imagination_engine_t* engine);

/*============================================================================
 * GPU Acceleration
 *============================================================================*/

/**
 * @brief Check if GPU is available
 */
bool imagination_gpu_available(const imagination_engine_t* engine);

/**
 * @brief Generate visual on GPU
 */
int imagination_generate_visual_gpu(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario);

/**
 * @brief Batch generate multiple scenarios on GPU
 */
int imagination_batch_generate_gpu(
    imagination_engine_t* engine,
    imagination_scenario_t** scenarios,
    size_t num_scenarios);

/*============================================================================
 * Statistics and Debugging
 *============================================================================*/

/**
 * @brief Get engine statistics
 */
int imagination_get_stats(
    const imagination_engine_t* engine,
    imagination_stats_t* stats);

/**
 * @brief Reset statistics
 */
int imagination_reset_stats(imagination_engine_t* engine);

/**
 * @brief Print engine state for debugging
 */
void imagination_print_state(
    const imagination_engine_t* engine,
    bool verbose);

/*============================================================================
 * String Conversion Utilities
 *============================================================================*/

/**
 * @brief Convert imagination mode to string
 */
const char* imagination_mode_to_string(imagination_mode_t mode);

/**
 * @brief Convert quality level to string
 */
const char* imagination_quality_to_string(imagination_quality_t quality);

/*============================================================================
 * MCTS-Based Goal-Directed Imagination
 *============================================================================*/

/**
 * @brief Use MCTS to search for goal-directed imagination path
 *
 * WHAT: Search for optimal sequence of imagination transformations
 * WHY:  Find efficient path from current state to goal state
 * HOW:  Use MCTS with transformation actions to explore state space
 *
 * @param engine The imagination engine
 * @param scenario Active scenario to guide toward goal
 * @param goal Target goal for imagination
 * @param num_iterations MCTS iterations (0 = default 50)
 * @return 0 on success, -1 on error
 */
int imagination_search_goal_mcts(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario,
    const imagination_goal_t* goal,
    uint32_t num_iterations);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_IMAGINATION_ENGINE_H */
