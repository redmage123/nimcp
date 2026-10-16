/**
 * @file nimcp_parietal.h
 * @brief Parietal Lobe Orchestrator with Full System Integration
 *
 * The parietal lobe module orchestrates mathematical and scientific reasoning,
 * spatial processing, and pattern recognition. This module provides:
 *
 * CORE CAPABILITIES:
 * - Number sense (Weber-Fechner, subitizing)
 * - Spatial reasoning (mental rotation, coordinate transforms)
 * - Mathematical intuition (pattern detection, symmetry)
 * - Scientific reasoning (hypothesis testing, causal inference)
 * - Equation manipulation (symbolic math, differentiation)
 *
 * FULL SYSTEM INTEGRATION:
 * - Brain region integration (REGION_PARIETAL)
 * - Neural network processing (Hamiltonian-Lagrangian NN)
 * - Immune system bridge (inflammation affects precision)
 * - Thalamic routing (attention gating)
 * - Substrate layer (hardware acceleration)
 * - Free Energy Principle bridge
 * - Sleep/circadian modulation
 * - Working memory integration
 * - Logic gate module integration
 * - Training module integration
 * - Perception module integration
 * - Bio-async messaging
 *
 * BIOLOGICAL BASIS:
 * The parietal cortex, particularly the intraparietal sulcus (IPS) and
 * posterior parietal cortex (PPC), handles spatial cognition, numerical
 * processing, and attention. Dense connections with prefrontal, motor,
 * and visual cortices enable integrated mathematical reasoning.
 *
 * USAGE:
 * ```c
 * parietal_lobe_t* parietal = parietal_create();
 *
 * // Connect to brain module
 * parietal_attach_to_brain(parietal, brain, REGION_PARIETAL);
 *
 * // Process request
 * parietal_request_t req = {.type = PARIETAL_PATTERN_DETECT, ...};
 * parietal_result_t result = parietal_process(parietal, &req);
 *
 * parietal_destroy(parietal);
 * ```
 */

#ifndef NIMCP_PARIETAL_H
#define NIMCP_PARIETAL_H

#include "utils/validation/nimcp_common.h"
#include "cognitive/parietal/nimcp_number_sense.h"
#include "cognitive/parietal/nimcp_spatial_reasoning.h"
#include "cognitive/parietal/nimcp_mathematical_intuition.h"
#include "cognitive/parietal/nimcp_scientific_reasoning.h"
#include "cognitive/parietal/nimcp_equation_manipulation.h"
#include "cognitive/parietal/nimcp_chemistry.h"
#include "cognitive/parietal/nimcp_biology.h"
#include "cognitive/parietal/nimcp_software_engineering.h"
#include "cognitive/parietal/nimcp_electrical_engineering.h"
#include "cognitive/parietal/nimcp_mechanical_engineering.h"
#include "cognitive/parietal/nimcp_civil_engineering.h"
/* NOTE: nimcp_parietal_quantum_bridge.h NOT included here to avoid circular deps */
/* Forward declarations for quantum types are provided below */
#include "cognitive/parietal/nimcp_fep_parietal_bridge.h"
#include "cognitive/parietal/nimcp_financial_investment.h"
#include "cognitive/parietal/nimcp_financial_market.h"
#include "cognitive/parietal/nimcp_financial_bridge.h"
#include "cognitive/parietal/nimcp_financial_neural_bridge.h"
#include "cognitive/parietal/nimcp_financial_investor_archetype.h"
#include "cognitive/imagination/nimcp_imagination_callbacks.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * FORWARD DECLARATIONS (Integration Dependencies)
 * Use guards to avoid conflicts with existing type definitions
 * Match existing type definitions from the codebase (pointer types)
 * ============================================================================ */

/* Brain regions - defined in nimcp_brain_regions.h */
#ifndef NIMCP_BRAIN_MODULE_T_DEFINED
#define NIMCP_BRAIN_MODULE_T_DEFINED
typedef struct brain_module_struct* brain_module_t;
#endif
#ifndef NIMCP_BRAIN_REGION_T_DEFINED
#define NIMCP_BRAIN_REGION_T_DEFINED
typedef struct brain_region_struct* brain_region_t;
#endif

/* Neural network - defined in nimcp_neuralnet.h as pointer */
#ifndef NIMCP_NEURAL_NETWORK_T_DEFINED
#define NIMCP_NEURAL_NETWORK_T_DEFINED
typedef struct neural_network_struct* neural_network_t;
#endif

/* Immune system — forward declaration matching nimcp_code_immune.h */
#ifndef NIMCP_CODE_IMMUNE_SYSTEM_T_DEFINED
#define NIMCP_CODE_IMMUNE_SYSTEM_T_DEFINED
typedef struct code_immune_system code_immune_system_t;
#endif

/* Thalamic routing.
 * The authoritative typedef is in middleware/routing/nimcp_thalamic_router.h
 * as `typedef struct thalamic_router thalamic_router_t;`.  Match that
 * convention here so the two TU includes don't conflict. */
#ifndef NIMCP_THALAMIC_ROUTER_T_DEFINED
#define NIMCP_THALAMIC_ROUTER_T_DEFINED
typedef struct thalamic_router thalamic_router_t;
#endif

/* Substrate layer */
#ifndef NIMCP_SUBSTRATE_INTERFACE_T_DEFINED
#define NIMCP_SUBSTRATE_INTERFACE_T_DEFINED
typedef struct substrate_interface_struct* substrate_interface_t;
#endif

/* FEP bridge */
#ifndef NIMCP_FEP_BRAIN_T_DEFINED
#define NIMCP_FEP_BRAIN_T_DEFINED
typedef struct fep_brain_struct* fep_brain_t;
#endif

/* Working memory - defined in nimcp_working_memory.h as struct, not pointer */
#ifndef NIMCP_WORKING_MEMORY_T_DEFINED
#define NIMCP_WORKING_MEMORY_T_DEFINED
typedef struct working_memory working_memory_t;
#endif

/* Logic gates */
#ifndef NIMCP_LOGIC_GATE_NETWORK_T_DEFINED
#define NIMCP_LOGIC_GATE_NETWORK_T_DEFINED
typedef struct logic_gate_network_struct* logic_gate_network_t;
#endif

/* Training */
#ifndef NIMCP_TRAINING_ENGINE_T_DEFINED
#define NIMCP_TRAINING_ENGINE_T_DEFINED
typedef struct training_engine_struct* training_engine_t;
#endif

/* Perception */
#ifndef NIMCP_PERCEPTION_SYSTEM_T_DEFINED
#define NIMCP_PERCEPTION_SYSTEM_T_DEFINED
typedef struct perception_system_struct* perception_system_t;
#endif

/* Tensor - defined in nimcp_tensor.h */
#ifndef NIMCP_TENSOR_T_DEFINED
#define NIMCP_TENSOR_T_DEFINED
typedef struct nimcp_tensor_s nimcp_tensor_t;
#endif

/* Sleep modulation - defined in nimcp_medulla.h */
#ifndef NIMCP_SLEEP_SYSTEM_T_DEFINED
#define NIMCP_SLEEP_SYSTEM_T_DEFINED
typedef struct sleep_system_struct* sleep_system_t;
#endif

/* Quantum bridge types - defined in nimcp_parietal_quantum_bridge.h */
/* Forward declarations to avoid circular include */
#ifndef NIMCP_PARIETAL_QUANTUM_BRIDGE_T_DEFINED
#define NIMCP_PARIETAL_QUANTUM_BRIDGE_T_DEFINED
typedef struct parietal_quantum_bridge parietal_quantum_bridge_t;
#endif

#ifndef NIMCP_PARIETAL_OPT_PROBLEM_T_DEFINED
#define NIMCP_PARIETAL_OPT_PROBLEM_T_DEFINED
typedef struct parietal_opt_problem_s parietal_opt_problem_t;
#endif

#ifndef NIMCP_PARIETAL_OPT_RESULT_T_DEFINED
#define NIMCP_PARIETAL_OPT_RESULT_T_DEFINED
typedef struct parietal_opt_result_s parietal_opt_result_t;
#endif

#ifndef NIMCP_PARIETAL_HAMILTONIAN_T_DEFINED
#define NIMCP_PARIETAL_HAMILTONIAN_T_DEFINED
typedef struct parietal_hamiltonian_s parietal_hamiltonian_t;
#endif

#ifndef NIMCP_PARIETAL_VQE_RESULT_T_DEFINED
#define NIMCP_PARIETAL_VQE_RESULT_T_DEFINED
typedef struct parietal_vqe_result_s parietal_vqe_result_t;
#endif

#ifndef NIMCP_PARIETAL_QUANTUM_CONFIG_T_DEFINED
#define NIMCP_PARIETAL_QUANTUM_CONFIG_T_DEFINED
typedef struct parietal_quantum_config_s parietal_quantum_config_t;
#endif

/* Imagination engine types - defined in nimcp_imagination_engine.h */
#ifndef NIMCP_IMAGINATION_ENGINE_T_DEFINED
#define NIMCP_IMAGINATION_ENGINE_T_DEFINED
typedef struct imagination_engine imagination_engine_t;
#endif

#ifndef NIMCP_IMAGINATION_SCENARIO_T_DEFINED
#define NIMCP_IMAGINATION_SCENARIO_T_DEFINED
typedef struct imagination_scenario imagination_scenario_t;
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Bio-async module ID for parietal lobe */
#define BIO_MODULE_PARIETAL             0x0380

/** Maximum pending requests */
#define PARIETAL_MAX_PENDING_REQUESTS   64

/** Maximum result history */
#define PARIETAL_MAX_HISTORY            256

/** Parietal brain region type (matches REGION_PARIETAL = 41) */
#define PARIETAL_BRAIN_REGION_TYPE      41

/* ============================================================================
 * TYPES
 * ============================================================================ */

/** Opaque handle for parietal lobe */
typedef struct parietal_lobe parietal_lobe_t;

/**
 * @brief Request types for parietal processing
 */
typedef enum {
    /* Number sense */
    PARIETAL_ESTIMATE_QUANTITY = 0,
    PARIETAL_COMPARE_QUANTITIES,
    PARIETAL_APPROXIMATE_ARITHMETIC,

    /* Spatial reasoning */
    PARIETAL_MENTAL_ROTATION,
    PARIETAL_COORDINATE_TRANSFORM,
    PARIETAL_SPATIAL_QUERY,

    /* Mathematical intuition */
    PARIETAL_PATTERN_DETECT,
    PARIETAL_PATTERN_EXTRAPOLATE,
    PARIETAL_SYMMETRY_DETECT,
    PARIETAL_SOLVE_ANALOGY,

    /* Scientific reasoning */
    PARIETAL_HYPOTHESIS_CREATE,
    PARIETAL_HYPOTHESIS_UPDATE,
    PARIETAL_DIMENSIONAL_ANALYSIS,
    PARIETAL_CAUSAL_INFERENCE,

    /* Equation manipulation */
    PARIETAL_PARSE_EXPRESSION,
    PARIETAL_DIFFERENTIATE,
    PARIETAL_SIMPLIFY,
    PARIETAL_EVALUATE,
    PARIETAL_SOLVE_EQUATION,

    /* Physics NN */
    PARIETAL_PHYSICS_PREDICT,
    PARIETAL_LEARN_DYNAMICS,

    /* Engineering Domains */
    PARIETAL_ELECTRICAL_CIRCUIT_ANALYZE,
    PARIETAL_ELECTRICAL_FILTER_DESIGN,
    PARIETAL_ELECTRICAL_STABILITY_ANALYZE,
    PARIETAL_MECHANICAL_STATIC_ANALYZE,
    PARIETAL_MECHANICAL_MODAL_ANALYZE,
    PARIETAL_MECHANICAL_THERMAL_ANALYZE,
    PARIETAL_CIVIL_STRUCTURAL_ANALYZE,
    PARIETAL_CIVIL_FOUNDATION_ANALYZE,
    PARIETAL_CIVIL_HYDRAULIC_ANALYZE,

    /* Quantum-Accelerated Processing */
    PARIETAL_QUANTUM_OPTIMIZE,
    PARIETAL_QUANTUM_TOPOLOGY_OPT,
    PARIETAL_QUANTUM_VQE_COMPUTE,
    PARIETAL_QUANTUM_WALK_SEARCH,
    PARIETAL_QUANTUM_SOLVE_QUBO,

    /* Free Energy Principle Processing */
    PARIETAL_FEP_UPDATE_BELIEFS,        /**< Hierarchical belief update */
    PARIETAL_FEP_PREDICT,               /**< Generate prediction from beliefs */
    PARIETAL_FEP_ACTIVE_INFERENCE,      /**< Select action via active inference */
    PARIETAL_FEP_COMPUTE_SURPRISE,      /**< Compute surprise from observation */
    PARIETAL_FEP_NUMERICAL_INFERENCE,   /**< FEP-based number sense */
    PARIETAL_FEP_SPATIAL_INFERENCE,     /**< FEP-based spatial reasoning */
    PARIETAL_FEP_PHYSICS_INFERENCE,     /**< FEP-based physics prediction */

    /* Financial Analysis */
    PARIETAL_FINANCIAL_PORTFOLIO_ANALYZE,    /**< Portfolio risk/return analysis */
    PARIETAL_FINANCIAL_RISK_ASSESS,          /**< Full risk assessment */
    PARIETAL_FINANCIAL_OPTION_PRICE,         /**< Derivatives pricing */
    PARIETAL_FINANCIAL_VALUATION,            /**< Asset valuation (DCF/DDM/comparables) */
    PARIETAL_FINANCIAL_OPTIMIZE,             /**< Portfolio optimization */
    PARIETAL_FINANCIAL_MARKET_REGIME,        /**< Market regime detection (fuzzy) */
    PARIETAL_FINANCIAL_SENTIMENT,            /**< Sentiment analysis (fuzzy) */
    PARIETAL_FINANCIAL_GARCH_FIT,            /**< GARCH volatility model */
    PARIETAL_FINANCIAL_INDICATOR,            /**< Technical indicator computation */
    PARIETAL_FINANCIAL_SCENARIO,             /**< Scenario/stress test */
    PARIETAL_FINANCIAL_MONTE_CARLO,          /**< Monte Carlo simulation */
    PARIETAL_FINANCIAL_ARCHETYPE_EVAL,       /**< Investor archetype evaluation */
    PARIETAL_FINANCIAL_ARCHETYPE_BLEND,      /**< Multi-archetype blending */

    PARIETAL_REQUEST_TYPE_COUNT
} parietal_request_type_t;

/**
 * @brief Parietal processing request
 */
typedef struct {
    parietal_request_type_t type;       /**< Request type */
    uint64_t request_id;                /**< Unique request ID */
    uint64_t timestamp;                 /**< Request timestamp */
    uint8_t priority;                   /**< Processing priority (0-255) */

    /* Input data (interpretation depends on type) */
    union {
        /* Number sense inputs */
        struct {
            float* values;
            uint32_t num_values;
        } quantity_input;

        struct {
            float magnitude_a;
            float magnitude_b;
        } comparison_input;

        struct {
            float a, b;
            char operation;  /* '+', '-', '*', '/' */
        } arithmetic_input;

        /* Spatial inputs */
        struct {
            spatial_object_t* object_a;
            spatial_object_t* object_b;
        } rotation_input;

        struct {
            vec3_t position;
            observer_pose_t* observer;
            bool ego_to_allocentric;
        } transform_input;

        struct {
            vec3_t query_point;
            float radius;
            uint32_t k;
        } spatial_query_input;

        /* Pattern inputs */
        struct {
            float* sequence;
            uint32_t length;
        } pattern_input;

        struct {
            vec3_t* points;
            uint32_t num_points;
        } symmetry_input;

        struct {
            float a, b, c;
        } analogy_input;

        /* Hypothesis inputs */
        struct {
            char description[256];
            float prior;
        } hypothesis_create_input;

        struct {
            hypothesis_t* hypothesis;
            data_sample_t* samples;
            uint32_t num_samples;
        } hypothesis_update_input;

        /* Equation inputs */
        struct {
            char expression[512];
            char variable[32];
        } equation_input;

        struct {
            expr_node_t* expr;
            variable_binding_t* bindings;
            uint32_t num_bindings;
        } eval_input;

        /* Physics NN inputs */
        struct {
            float* state;
            uint32_t state_dim;
            float dt;
        } physics_input;

    } input;

    /* Routing hints */
    bool use_neural_network;            /**< Route through NN */
    bool use_thalamic_gating;           /**< Apply thalamic attention */
    bool use_substrate_accel;           /**< Use hardware acceleration */
    bool async;                         /**< Asynchronous processing */
} parietal_request_t;

/**
 * @brief Parietal processing result
 */
typedef struct {
    parietal_request_type_t type;       /**< Result type */
    uint64_t request_id;                /**< Matching request ID */
    uint64_t processing_time_us;        /**< Processing time */
    float confidence;                   /**< Result confidence [0,1] */
    bool success;                       /**< Processing succeeded */
    char error_message[128];            /**< Error if !success */

    /* Output data */
    union {
        number_estimate_t estimate;
        number_comparison_t comparison;
        approx_arithmetic_t arithmetic;
        rotation_result_t rotation;
        vec3_t transformed_position;
        spatial_query_result_t* spatial_results;
        detected_pattern_t pattern;
        symmetry_result_t symmetry;
        analogy_result_t analogy;
        hypothesis_t hypothesis;
        expr_node_t* expression;
        float evaluated_value;

        struct {
            float* predicted_state;
            float energy;
        } physics_output;

    } output;
} parietal_result_t;

/**
 * @brief Parietal configuration
 */
typedef struct {
    /* Submodule configurations */
    number_sense_config_t number_sense;
    spatial_config_t spatial;
    math_intuition_config_t math_intuition;
    scientific_config_t scientific;
    equation_config_t equation;

    /* Integration settings */
    bool enable_neural_network;         /**< Enable NN processing (default: true) */
    bool enable_immune_bridge;          /**< Enable immune modulation (default: true) */
    bool enable_thalamic_routing;       /**< Enable thalamic gating (default: true) */
    bool enable_substrate_accel;        /**< Enable substrate acceleration (default: true) */
    bool enable_fep_bridge;             /**< Enable FEP integration (default: true) */
    bool enable_sleep_modulation;       /**< Enable sleep effects (default: true) */
    bool enable_working_memory;         /**< Enable working memory (default: true) */
    bool enable_logic_gates;            /**< Enable logic gate integration (default: true) */
    bool enable_training;               /**< Enable training integration (default: true) */
    bool enable_perception;             /**< Enable perception integration (default: true) */
    bool enable_bio_async;              /**< Enable bio-async messaging (default: true) */
    bool enable_quantum_bridge;         /**< Enable quantum acceleration (default: true) */
    bool enable_electrical_eng;         /**< Enable electrical engineering (default: true) */
    bool enable_mechanical_eng;         /**< Enable mechanical engineering (default: true) */
    bool enable_civil_eng;              /**< Enable civil engineering (default: true) */
    bool enable_financial;              /**< Enable financial analysis (default: true) */

    /* Neural network settings */
    uint32_t nn_hidden_size;            /**< NN hidden layer size (default: 256) */
    float nn_learning_rate;             /**< NN learning rate (default: 0.001) */
    bool nn_use_hamiltonian;            /**< Use Hamiltonian structure (default: true) */
    bool nn_use_lagrangian;             /**< Use Lagrangian constraints (default: true) */

    /* Quantum bridge settings */
    parietal_quantum_config_t* quantum_config; /**< Quantum bridge configuration (owned pointer) */

    /* FEP bridge settings */
    bool enable_fep_parietal_bridge;    /**< Enable FEP processing (default: true) */
    fep_parietal_config_t fep_parietal_config; /**< FEP-Parietal bridge config */

    /* Engineering module settings */
    ee_config_t electrical_config;      /**< Electrical engineering config */
    me_config_t mechanical_config;      /**< Mechanical engineering config */
    ce_config_t civil_config;           /**< Civil engineering config */

    /* Performance settings */
    uint32_t max_parallel_requests;     /**< Max parallel processing (default: 8) */
    uint32_t request_timeout_ms;        /**< Request timeout (default: 5000) */
} parietal_config_t;

/**
 * @brief Parietal lobe statistics
 */
typedef struct {
    /* Request counts by type */
    uint64_t requests_by_type[PARIETAL_REQUEST_TYPE_COUNT];
    uint64_t total_requests;
    uint64_t failed_requests;

    /* Submodule statistics */
    number_sense_stats_t number_sense;
    spatial_stats_t spatial;
    math_intuition_stats_t math_intuition;
    scientific_stats_t scientific;
    equation_stats_t equation;

    /* Integration statistics */
    uint64_t neural_network_activations;
    uint64_t thalamic_gates_applied;
    uint64_t substrate_accelerations;
    uint64_t immune_modulations;
    uint64_t fep_predictions;

    /* FEP-Parietal statistics */
    uint64_t fep_belief_updates;
    uint64_t fep_active_inferences;
    float avg_fep_free_energy;
    fep_parietal_stats_t fep_parietal_stats;

    /* Quantum statistics */
    uint64_t quantum_optimizations;
    uint64_t quantum_vqe_runs;
    uint64_t quantum_walk_runs;
    float avg_quantum_speedup;

    /* Engineering statistics */
    ee_stats_t electrical_stats;
    me_stats_t mechanical_stats;
    ce_stats_t civil_stats;

    /* Performance */
    float avg_processing_time_us;
    float avg_confidence;
    float current_inflammation;
    float current_fatigue;
} parietal_stats_t;

/**
 * @brief Callback for async processing
 */
typedef void (*parietal_callback_t)(
    parietal_lobe_t* parietal,
    const parietal_result_t* result,
    void* user_data
);

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Create parietal lobe with default configuration
 * @return Handle or NULL on error
 */
parietal_lobe_t* parietal_create(void);

/**
 * @brief Create parietal lobe with custom configuration
 * @param config Configuration (NULL for defaults)
 * @return Handle or NULL on error
 */
parietal_lobe_t* parietal_create_custom(const parietal_config_t* config);

/**
 * @brief Destroy parietal lobe
 * @param parietal Handle (NULL safe)
 */
void parietal_destroy(parietal_lobe_t* parietal);

/**
 * @brief Get default configuration
 * @return Default configuration struct
 */
parietal_config_t parietal_default_config(void);

/**
 * @brief Validate configuration
 * @param config Configuration to validate
 * @return true if valid
 */
bool parietal_validate_config(const parietal_config_t* config);

/* ============================================================================
 * BRAIN INTEGRATION API
 * ============================================================================ */

/**
 * @brief Attach to brain module
 *
 * Creates brain region and establishes connections.
 *
 * @param parietal Parietal lobe handle
 * @param brain Brain module handle
 * @param num_neurons Number of neurons for region
 * @return 0 on success
 */
int parietal_attach_to_brain(
    parietal_lobe_t* parietal,
    brain_module_t* brain,
    uint32_t num_neurons
);

/**
 * @brief Get attached brain region
 *
 * @param parietal Parietal lobe handle
 * @return Brain region or NULL if not attached
 */
brain_region_t* parietal_get_brain_region(parietal_lobe_t* parietal);

/**
 * @brief Connect to other brain region
 *
 * @param parietal Parietal lobe handle
 * @param target_region_id Target region ID
 * @param connection_strength Connection strength [0,1]
 * @return 0 on success
 */
int parietal_connect_to_region(
    parietal_lobe_t* parietal,
    uint32_t target_region_id,
    float connection_strength
);

/* ============================================================================
 * INTEGRATION BRIDGES API
 * ============================================================================ */

/**
 * @brief Attach immune system bridge
 *
 * @param parietal Parietal lobe handle
 * @param immune Immune system handle
 * @return 0 on success
 */
int parietal_attach_immune(
    parietal_lobe_t* parietal,
    code_immune_system_t* immune
);

/**
 * @brief Attach thalamic router
 *
 * @param parietal Parietal lobe handle
 * @param thalamus Thalamic router handle
 * @return 0 on success
 */
int parietal_attach_thalamus(
    parietal_lobe_t* parietal,
    thalamic_router_t* thalamus
);

/**
 * @brief Attach substrate interface
 *
 * @param parietal Parietal lobe handle
 * @param substrate Substrate interface handle
 * @return 0 on success
 */
int parietal_attach_substrate(
    parietal_lobe_t* parietal,
    substrate_interface_t* substrate
);

/**
 * @brief Attach FEP brain bridge
 *
 * @param parietal Parietal lobe handle
 * @param fep FEP brain handle
 * @return 0 on success
 */
int parietal_attach_fep(
    parietal_lobe_t* parietal,
    fep_brain_t* fep
);

/**
 * @brief Attach working memory
 *
 * @param parietal Parietal lobe handle
 * @param wm Working memory handle
 * @return 0 on success
 */
int parietal_attach_working_memory(
    parietal_lobe_t* parietal,
    working_memory_t* wm
);

/**
 * @brief Attach logic gate network
 *
 * @param parietal Parietal lobe handle
 * @param logic Logic gate network handle
 * @return 0 on success
 */
int parietal_attach_logic_gates(
    parietal_lobe_t* parietal,
    logic_gate_network_t* logic
);

/**
 * @brief Attach training engine
 *
 * @param parietal Parietal lobe handle
 * @param training Training engine handle
 * @return 0 on success
 */
int parietal_attach_training(
    parietal_lobe_t* parietal,
    training_engine_t* training
);

/**
 * @brief Attach perception system
 *
 * @param parietal Parietal lobe handle
 * @param perception Perception system handle
 * @return 0 on success
 */
int parietal_attach_perception(
    parietal_lobe_t* parietal,
    perception_system_t* perception
);

/**
 * @brief Attach sleep system
 *
 * @param parietal Parietal lobe handle
 * @param sleep Sleep system handle
 * @return 0 on success
 */
int parietal_attach_sleep(
    parietal_lobe_t* parietal,
    sleep_system_t* sleep
);

/* ============================================================================
 * WORLD MODEL SIMULATION ENGINE INTEGRATION API
 *
 * Connects grounded simulation engines to the parietal lobe for
 * physics-informed reasoning, hypothesis testing via simulation,
 * and training the physics NN from engine-generated data.
 * ============================================================================ */

/**
 * @brief Attach all simulation engines from the brain's world model subsystem
 *
 * Connects the parietal lobe to the brain's intuitive physics, relativistic,
 * electromagnetic, MHD, surface physics, surface chemistry, bulk chemistry,
 * biology, entity tracker, scene graph, and physics prior engines.
 *
 * After attachment, scientific_reasoning can run simulations to test hypotheses,
 * physics_nn can train on engine-generated trajectories, and spatial_reasoning
 * gets access to the scene graph and entity tracker.
 *
 * @param parietal Parietal lobe handle
 * @param brain Internal brain struct (provides all engine pointers)
 * @return 0 on success, -1 on error
 */
struct brain_struct;  /* forward declaration */
int parietal_attach_simulation_engines(parietal_lobe_t* parietal,
                                        struct brain_struct* brain);

/**
 * @brief Run a physics simulation to test a hypothesis
 *
 * Sets up a scenario in the appropriate simulation engine, runs it,
 * and returns the result as a confidence update for the hypothesis.
 *
 * @param parietal Parietal lobe handle
 * @param domain "physics", "chemistry", "biology", "em", "surface", etc.
 * @param scenario Description of the scenario to simulate
 * @param result_confidence Output: [0,1] confidence from simulation
 * @return 0 on success, -1 on error
 */
int parietal_simulate_hypothesis(parietal_lobe_t* parietal,
                                  const char* domain,
                                  const char* scenario,
                                  float* result_confidence);

/**
 * @brief Query the scene graph for spatial relations
 *
 * Delegates to the attached scene graph engine for support/containment
 * queries, which feed into spatial reasoning.
 *
 * @param parietal Parietal lobe handle
 * @param object_a First object ID
 * @param object_b Second object ID
 * @return Relation type (scene_relation_type_t) or -1 if none
 */
int parietal_query_spatial_relation(parietal_lobe_t* parietal,
                                     uint32_t object_a, uint32_t object_b);

/**
 * @brief Train the physics NN from simulation engine data
 *
 * Runs the attached physics engine for N steps, records state transitions,
 * and trains the physics NN on those trajectories. This teaches the NN
 * to internalize physical laws from direct experience.
 *
 * @param parietal Parietal lobe handle
 * @param num_trajectories Number of random scenarios to simulate
 * @param steps_per_trajectory Simulation steps per scenario
 * @return Number of training samples generated, or -1 on error
 */
int parietal_train_physics_nn_from_sim(parietal_lobe_t* parietal,
                                        uint32_t num_trajectories,
                                        uint32_t steps_per_trajectory);

/* ============================================================================
 * IMAGINATION ENGINE INTEGRATION API
 * ============================================================================ */

/**
 * @brief Connect to imagination engine
 *
 * Establishes bidirectional connection between parietal lobe and imagination
 * engine. Enables spatial imagination, mental rotation visualization, and
 * mathematical visualization capabilities.
 *
 * @param parietal Parietal lobe handle
 * @param engine Imagination engine handle
 * @return 0 on success, -1 on error
 */
int parietal_connect_imagination(
    parietal_lobe_t* parietal,
    imagination_engine_t* engine
);

/**
 * @brief Set callback for imagination results
 *
 * Registers a callback to receive imagination scenario results. Called when
 * imagination engine completes spatial or mathematical visualization requests.
 *
 * @param parietal Parietal lobe handle
 * @param cb Result callback function
 * @param user_data User data passed to callback
 * @return 0 on success, -1 on error
 */
int parietal_set_imagination_callback(
    parietal_lobe_t* parietal,
    imagination_result_callback_t cb,
    void* user_data
);

/**
 * @brief Request imagination-based mental rotation
 *
 * Uses imagination engine to perform mental rotation of an object in 3D space.
 * This extends the standard mental rotation with full visualization capabilities.
 *
 * Note: For spatial object comparison rotation, use standard mental rotate functions.
 *
 * @param parietal Parietal lobe handle
 * @param object Tensor representation of object to rotate
 * @param angle_x Rotation angle around X axis (radians)
 * @param angle_y Rotation angle around Y axis (radians)
 * @param angle_z Rotation angle around Z axis (radians)
 * @return Imagination scenario handle or NULL on error
 */
imagination_scenario_t* parietal_imagine_rotation(
    parietal_lobe_t* parietal,
    nimcp_tensor_t* object,
    float angle_x,
    float angle_y,
    float angle_z
);

/**
 * @brief Request spatial transformation through imagination
 *
 * Uses imagination engine to transform a spatial scene using a transformation
 * matrix. Enables complex spatial manipulations with full visualization.
 *
 * @param parietal Parietal lobe handle
 * @param scene Tensor representation of spatial scene
 * @param transform Transformation matrix tensor (4x4 or 3x3)
 * @return Imagination scenario handle or NULL on error
 */
imagination_scenario_t* parietal_spatial_transform(
    parietal_lobe_t* parietal,
    nimcp_tensor_t* scene,
    nimcp_tensor_t* transform
);

/**
 * @brief Request mathematical visualization through imagination
 *
 * Triggers imagination engine to generate visual representation of a
 * mathematical expression. Useful for geometric visualization, function
 * plotting, and abstract mathematical concepts.
 *
 * @param parietal Parietal lobe handle
 * @param expression Mathematical expression string to visualize
 * @return 0 on success, -1 on error
 */
int parietal_request_mathematical_visualization(
    parietal_lobe_t* parietal,
    const char* expression
);

/* ============================================================================
 * PROCESSING API
 * ============================================================================ */

/**
 * @brief Process parietal request (synchronous)
 *
 * @param parietal Parietal lobe handle
 * @param request Request to process
 * @return Processing result
 */
parietal_result_t parietal_process(
    parietal_lobe_t* parietal,
    const parietal_request_t* request
);

/**
 * @brief Process parietal request (asynchronous)
 *
 * @param parietal Parietal lobe handle
 * @param request Request to process
 * @param callback Completion callback
 * @param user_data User data for callback
 * @return Request ID or 0 on error
 */
uint64_t parietal_process_async(
    parietal_lobe_t* parietal,
    const parietal_request_t* request,
    parietal_callback_t callback,
    void* user_data
);

/**
 * @brief Poll for async result
 *
 * @param parietal Parietal lobe handle
 * @param request_id Request ID
 * @param result Output result (if ready)
 * @return 1 if result ready, 0 if pending, -1 on error
 */
int parietal_poll_result(
    parietal_lobe_t* parietal,
    uint64_t request_id,
    parietal_result_t* result
);

/**
 * @brief Wait for async result
 *
 * @param parietal Parietal lobe handle
 * @param request_id Request ID
 * @param timeout_ms Timeout in milliseconds
 * @param result Output result
 * @return 0 on success, -1 on timeout/error
 */
int parietal_wait_result(
    parietal_lobe_t* parietal,
    uint64_t request_id,
    uint32_t timeout_ms,
    parietal_result_t* result
);

/* ============================================================================
 * NEURAL NETWORK API (Hamiltonian-Lagrangian NN)
 * ============================================================================ */

/**
 * @brief Train physics neural network
 *
 * @param parietal Parietal lobe handle
 * @param states Training states [num_samples x state_dim]
 * @param derivatives State derivatives [num_samples x state_dim]
 * @param num_samples Number of training samples
 * @param epochs Number of training epochs
 * @return Final training loss
 */
float parietal_train_physics_nn(
    parietal_lobe_t* parietal,
    const float** states,
    const float** derivatives,
    uint32_t num_samples,
    uint32_t epochs
);

/**
 * @brief Predict state evolution using physics NN
 *
 * @param parietal Parietal lobe handle
 * @param initial_state Initial state
 * @param state_dim State dimension
 * @param dt Time step
 * @param steps Number of steps to predict
 * @param predicted_states Output predicted states [steps x state_dim]
 * @return 0 on success
 */
int parietal_predict_dynamics(
    parietal_lobe_t* parietal,
    const float* initial_state,
    uint32_t state_dim,
    float dt,
    uint32_t steps,
    float** predicted_states
);

/**
 * @brief Get Hamiltonian (total energy) for state
 *
 * @param parietal Parietal lobe handle
 * @param state System state
 * @param state_dim State dimension
 * @return Hamiltonian value
 */
float parietal_compute_hamiltonian(
    parietal_lobe_t* parietal,
    const float* state,
    uint32_t state_dim
);

/* ============================================================================
 * CONVENIENCE WRAPPERS API
 * ============================================================================ */

/**
 * @brief Estimate quantity (convenience wrapper)
 */
number_estimate_t parietal_estimate_quantity(
    parietal_lobe_t* parietal,
    const float* values,
    uint32_t num_values
);

/**
 * @brief Detect pattern (convenience wrapper)
 */
detected_pattern_t parietal_detect_pattern(
    parietal_lobe_t* parietal,
    const float* sequence,
    uint32_t length
);

/**
 * @brief Parse and differentiate expression (convenience wrapper)
 */
expr_node_t* parietal_differentiate_expression(
    parietal_lobe_t* parietal,
    const char* expr_string,
    const char* variable
);

/**
 * @brief Mental rotation comparison (convenience wrapper)
 *
 * @deprecated Use parietal_mental_rotate() with imagination engine for advanced visualization
 */
rotation_result_t parietal_mental_rotate_compare(
    parietal_lobe_t* parietal,
    const spatial_object_t* object_a,
    const spatial_object_t* object_b
);

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

/**
 * @brief Set global inflammation level
 *
 * Propagates to all submodules.
 *
 * @param parietal Parietal lobe handle
 * @param level Inflammation level [0,1]
 * @return 0 on success
 */
int parietal_set_inflammation(
    parietal_lobe_t* parietal,
    float level
);

/**
 * @brief Set global fatigue level
 *
 * @param parietal Parietal lobe handle
 * @param level Fatigue level [0,1]
 * @return 0 on success
 */
int parietal_set_fatigue(
    parietal_lobe_t* parietal,
    float level
);

/**
 * @brief Update from sleep system
 *
 * Called automatically if sleep system attached.
 *
 * @param parietal Parietal lobe handle
 * @return 0 on success
 */
int parietal_update_from_sleep(parietal_lobe_t* parietal);

/**
 * @brief Update from immune system
 *
 * Called automatically if immune system attached.
 *
 * @param parietal Parietal lobe handle
 * @return 0 on success
 */
int parietal_update_from_immune(parietal_lobe_t* parietal);

/* ============================================================================
 * STEPPING API
 * ============================================================================ */

/**
 * @brief Step parietal lobe forward in time
 *
 * Processes queued requests and updates neural state.
 *
 * @param parietal Parietal lobe handle
 * @param delta_t Time step in microseconds
 * @return 0 on success
 */
int parietal_step(
    parietal_lobe_t* parietal,
    uint64_t delta_t
);

/**
 * @brief Process all pending requests
 *
 * @param parietal Parietal lobe handle
 * @return Number of requests processed
 */
uint32_t parietal_process_pending(parietal_lobe_t* parietal);

/* ============================================================================
 * DIRECT SUBMODULE ACCESS API
 * ============================================================================ */

/**
 * @brief Get number sense submodule
 */
number_sense_t* parietal_get_number_sense(parietal_lobe_t* parietal);

/**
 * @brief Get spatial reasoning submodule
 */
spatial_reasoning_t* parietal_get_spatial(parietal_lobe_t* parietal);

/**
 * @brief Get mathematical intuition submodule
 */
math_intuition_t* parietal_get_math_intuition(parietal_lobe_t* parietal);

/**
 * @brief Get scientific reasoning submodule
 */
scientific_reasoning_t* parietal_get_scientific(parietal_lobe_t* parietal);

/**
 * @brief Get equation engine submodule
 */
equation_engine_t* parietal_get_equation_engine(parietal_lobe_t* parietal);

/**
 * @brief Get quantum bridge submodule
 */
parietal_quantum_bridge_t* parietal_get_quantum_bridge(parietal_lobe_t* parietal);

/**
 * @brief Get FEP-Parietal bridge submodule
 */
fep_parietal_bridge_t* parietal_get_fep_bridge(parietal_lobe_t* parietal);

/**
 * @brief Get electrical engineering submodule
 */
electrical_eng_t* parietal_get_electrical(parietal_lobe_t* parietal);

/**
 * @brief Get mechanical engineering submodule
 */
mechanical_eng_t* parietal_get_mechanical(parietal_lobe_t* parietal);

/**
 * @brief Get civil engineering submodule
 */
civil_eng_t* parietal_get_civil(parietal_lobe_t* parietal);

/* ============================================================================
 * QUANTUM ACCELERATION API
 * ============================================================================ */

/**
 * @brief Enable/disable quantum acceleration globally
 *
 * @param parietal Parietal lobe handle
 * @param enabled Enable flag
 * @return 0 on success
 */
int parietal_set_quantum_enabled(parietal_lobe_t* parietal, bool enabled);

/**
 * @brief Check if quantum acceleration is available
 *
 * @param parietal Parietal lobe handle
 * @return true if available
 */
bool parietal_quantum_available(const parietal_lobe_t* parietal);

/**
 * @brief Quantum-accelerated optimization (high-level wrapper)
 *
 * @param parietal Parietal lobe handle
 * @param problem Optimization problem
 * @param result Output result
 * @return 0 on success
 */
int parietal_lobe_quantum_optimize(
    parietal_lobe_t* parietal,
    const parietal_opt_problem_t* problem,
    parietal_opt_result_t* result
);

/**
 * @brief Quantum VQE for physics simulation (high-level wrapper)
 *
 * @param parietal Parietal lobe handle
 * @param hamiltonian System Hamiltonian
 * @param result VQE result
 * @return 0 on success
 */
int parietal_lobe_quantum_vqe(
    parietal_lobe_t* parietal,
    const parietal_hamiltonian_t* hamiltonian,
    parietal_vqe_result_t* result
);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

/**
 * @brief Get aggregated statistics
 *
 * @param parietal Parietal lobe handle
 * @param stats Output statistics
 * @return 0 on success
 */
int parietal_get_stats(
    parietal_lobe_t* parietal,
    parietal_stats_t* stats
);

/**
 * @brief Reset all statistics
 *
 * @param parietal Parietal lobe handle
 */
void parietal_reset_stats(parietal_lobe_t* parietal);

/**
 * @brief Get last error message
 * @return Thread-local error message
 */
const char* parietal_get_last_error(void);

/* ============================================================================
 * BIO-ASYNC MESSAGING
 * ============================================================================ */

/**
 * @brief Bio-async message types for parietal lobe
 */
typedef enum {
    BIO_MSG_PARIETAL_ESTIMATE = 0x0380,
    BIO_MSG_PARIETAL_COMPARE,
    BIO_MSG_PARIETAL_ROTATE,
    BIO_MSG_PARIETAL_PATTERN,
    BIO_MSG_PARIETAL_HYPOTHESIS,
    BIO_MSG_PARIETAL_EQUATION,
    BIO_MSG_PARIETAL_PHYSICS,
    BIO_MSG_PARIETAL_RESULT,
    /* Engineering domains */
    BIO_MSG_PARIETAL_ELECTRICAL,
    BIO_MSG_PARIETAL_MECHANICAL,
    BIO_MSG_PARIETAL_CIVIL,
    /* Quantum acceleration */
    BIO_MSG_PARIETAL_QUANTUM_OPT,
    BIO_MSG_PARIETAL_QUANTUM_VQE,
    BIO_MSG_PARIETAL_QUANTUM_WALK,
    /* FEP-based processing */
    BIO_MSG_PARIETAL_FEP_BELIEF_UPDATE,
    BIO_MSG_PARIETAL_FEP_PREDICT,
    BIO_MSG_PARIETAL_FEP_ACTIVE_INFERENCE,
    BIO_MSG_PARIETAL_FEP_SURPRISE,
} bio_msg_parietal_t;

/**
 * @brief Handle bio-async message
 *
 * @param parietal Parietal lobe handle
 * @param msg_type Message type
 * @param payload Message payload
 * @param payload_size Payload size
 * @return 0 on success
 */
int parietal_handle_bio_msg(
    parietal_lobe_t* parietal,
    uint32_t msg_type,
    const void* payload,
    uint32_t payload_size
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PARIETAL_H */
