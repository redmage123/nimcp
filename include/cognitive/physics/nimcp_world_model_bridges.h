/**
 * @file nimcp_world_model_bridges.h
 * @brief World Model Bridges — JEPA + Prime Resonance ↔ Simulation Engines
 *
 * WHAT: Connects JEPA latent predictions and prime resonance memory to the
 *       simulation engines, creating a closed loop:
 *       JEPA predicts → simulation verifies → error → prime resonance stores
 *       → consolidation replays → JEPA improves
 *
 * WHY:  LeCun's core insight: predict in LATENT SPACE, not pixel space.
 *       Our simulation engines provide GROUND TRUTH in physical space.
 *       The bridge translates between latent predictions and physical state,
 *       giving JEPA a physics-grounded training signal.
 *
 *       Prime resonance stores moments of high prediction error — the
 *       "surprising" physics events that teach the most. During sleep/
 *       consolidation, these are replayed through the simulation engines
 *       to deepen world understanding.
 *
 * DATA FLOW:
 *   JEPA predicts z_next in latent space
 *     → Bridge decodes z_next to physical state (position, velocity, etc.)
 *     → Simulation engine runs from current state
 *     → Bridge compares predicted vs simulated physical state
 *     → Prediction error in PHYSICAL SPACE (not just latent)
 *     → Error weighted by FEP precision → JEPA training signal
 *     → If error > threshold → prime resonance stores the event
 *     → During consolidation → replay stored events through simulation
 *     → JEPA refines its world model from replay
 */

#ifndef NIMCP_WORLD_MODEL_BRIDGES_H
#define NIMCP_WORLD_MODEL_BRIDGES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct jepa_predictor;
struct jepa_latent;
struct intuitive_physics_engine;
struct world_simulator;
struct scene_graph;
struct entity_tracker;
struct fep_system;

/* ============================================================================
 * Constants
 * ============================================================================ */

#define WMB_MAX_REPLAY_BUFFER   256
#define WMB_LATENT_DIM          128     /* must match JEPA latent_dim */
#define WMB_PHYSICAL_DIM        12      /* pos(3) + vel(3) + orient(4) + mass + radius */
#define WMB_CHEM_DIM            16      /* concentrations + pH + temperature + pressure */
#define WMB_BIO_DIM             8       /* populations + health + energy */
#define WMB_TOTAL_STATE_DIM     (WMB_PHYSICAL_DIM + WMB_CHEM_DIM + WMB_BIO_DIM) /* 36 */
#define WMB_SURPRISE_THRESHOLD  2.0f    /* prediction error > 2σ → store in resonance */

/* ============================================================================
 * Domain
 * ============================================================================ */

typedef enum {
    WMB_DOMAIN_PHYSICS      = 0,
    WMB_DOMAIN_CHEMISTRY    = 1,
    WMB_DOMAIN_BIOLOGY      = 2,
    WMB_DOMAIN_CROSS        = 3,    /* multi-domain event */
    WMB_DOMAIN_COUNT
} wmb_domain_t;

/* ============================================================================
 * Physical State (decoded from / encoded to JEPA latent)
 * ============================================================================ */

typedef struct {
    float position[3];
    float velocity[3];
    float orientation[4];       /* quaternion */
    float mass;
    float radius;
    uint32_t object_id;
    float confidence;
} wmb_physical_state_t;

/* ============================================================================
 * Chemical State (concentrations, pH, temperature, pressure)
 * ============================================================================ */

typedef struct {
    float concentrations[8];    /* top-8 species concentrations (mol/L) */
    float pH;
    float temperature;          /* K */
    float pressure;             /* Pa */
    float total_mass;           /* conservation check */
    float reaction_rate;        /* dominant reaction rate */
    float energy_change;        /* ΔH from reactions (kJ) */
    float equilibrium_shift;    /* deviation from equilibrium */
    float catalyst_activity;    /* [0..1] */
} wmb_chemical_state_t;

/* ============================================================================
 * Biological State (populations, health, energy)
 * ============================================================================ */

typedef struct {
    float populations[4];       /* top-4 species populations */
    float total_biomass;
    float biodiversity;         /* Shannon index */
    float health;               /* organism health [0..1] */
    float energy_available;     /* ATP or metabolic energy */
} wmb_biological_state_t;

/* ============================================================================
 * Unified World State (all three domains)
 * ============================================================================ */

typedef struct {
    wmb_physical_state_t    physics;
    wmb_chemical_state_t    chemistry;
    wmb_biological_state_t  biology;
} wmb_world_state_t;

/* ============================================================================
 * Surprise Event (stored in prime resonance)
 * ============================================================================ */

typedef struct {
    wmb_world_state_t predicted;    /* what JEPA/extrapolation predicted */
    wmb_world_state_t actual;       /* what simulation engines produced */
    float prediction_error;         /* L2 norm of full state difference */
    float physics_error;            /* physics-only error component */
    float chemistry_error;          /* chemistry-only error component */
    float biology_error;            /* biology-only error component */
    float surprise_score;           /* error / expected_variance */
    float timestamp;
    wmb_domain_t domain;            /* which domain was most surprising */
    uint32_t scenario_type;         /* collision=0, reaction=1, extinction=2, ... */
    /* Latent representation for replay */
    float latent_before[WMB_LATENT_DIM];
    float latent_after[WMB_LATENT_DIM];
} wmb_surprise_event_t;

/* ============================================================================
 * Replay Buffer (ring buffer of surprise events for consolidation)
 * ============================================================================ */

typedef struct {
    wmb_surprise_event_t events[WMB_MAX_REPLAY_BUFFER];
    uint32_t    count;
    uint32_t    head;               /* next write position */
    uint32_t    replays_total;
    float       mean_surprise;      /* running mean of surprise scores */
    float       max_surprise;
} wmb_replay_buffer_t;

/* ============================================================================
 * JEPA-Simulation Bridge Config
 * ============================================================================ */

typedef struct {
    float       surprise_threshold;     /* store events with surprise > this */
    float       replay_learning_rate;   /* LR for replay-based JEPA training */
    uint32_t    replay_batch_size;      /* events per consolidation replay */
    bool        enable_prime_resonance; /* store surprises in prime resonance */
    bool        enable_replay;          /* replay during consolidation */
    bool        enable_physical_loss;   /* add physical prediction error to JEPA loss */
    float       physical_loss_weight;   /* λ for physical error term */
} wmb_config_t;

/* ============================================================================
 * Stats
 * ============================================================================ */

typedef struct {
    uint64_t    predictions_made;
    uint64_t    simulations_run;
    uint64_t    surprises_stored;
    uint64_t    replays_executed;
    float       mean_prediction_error;
    float       mean_physical_error;
    float       max_surprise_score;
    float       jepa_improvement_rate;  /* delta(loss) per replay */
    float       fep_free_energy;        /* current variational free energy */
    float       fep_physics_precision;  /* learned precision for physics errors */
    float       fep_chemistry_precision;
    float       fep_biology_precision;
    uint64_t    fep_updates;            /* total FEP belief updates from sim errors */
} wmb_stats_t;

/* ============================================================================
 * Bridge
 * ============================================================================ */

/**
 * @brief Callback fired when a surprise event is stored.
 *
 * The brain's init code registers this to route surprise events
 * to the perirhinal cortex's prime resonance memory system.
 * This avoids tight coupling between the bridge and brain internals.
 *
 * @param event The surprise event (predicted vs actual, error, latent pairs)
 * @param user_data Opaque pointer (typically the brain or perirhinal handle)
 */
typedef void (*wmb_surprise_callback_t)(const wmb_surprise_event_t* event,
                                         void* user_data);

/**
 * @brief Callback fired during consolidation replay.
 *
 * Called for each replayed event so the brain can feed the latent
 * before→after pair to JEPA's train_step().
 *
 * @param latent_before The latent state before the transition [WMB_LATENT_DIM]
 * @param latent_after The latent state after the transition [WMB_LATENT_DIM]
 * @param surprise_score How surprising this event was
 * @param user_data Opaque pointer (typically the brain or JEPA handle)
 */
typedef void (*wmb_replay_callback_t)(const float* latent_before,
                                       const float* latent_after,
                                       float surprise_score,
                                       void* user_data);

typedef struct world_model_bridge {
    /* Connected systems (non-owning) */
    struct jepa_predictor*          jepa;
    struct intuitive_physics_engine* physics;
    struct world_simulator*         world_sim;
    struct scene_graph*             scene;
    struct entity_tracker*          tracker;
    struct chemistry_sim*           chemistry;
    struct biology_sim*             biology;
    struct fep_system*              fep;        /* FEP system for free energy updates */

    /* Encoding/decoding weights (latent ↔ physical) */
    float*      encode_weights;     /* [PHYSICAL_DIM × LATENT_DIM] */
    float*      decode_weights;     /* [LATENT_DIM × PHYSICAL_DIM] */

    /* Replay buffer */
    wmb_replay_buffer_t replay;

    /* FEP precision tracking (learned from prediction error statistics) */
    float       physics_precision;      /* π_phys = 1/E[ε²_phys] */
    float       chemistry_precision;    /* π_chem = 1/E[ε²_chem] */
    float       biology_precision;      /* π_bio = 1/E[ε²_bio] */
    float       physics_error_ema;      /* EMA of physics error² */
    float       chemistry_error_ema;    /* EMA of chemistry error² */
    float       biology_error_ema;      /* EMA of biology error² */

    /* Callbacks — fire on surprise and replay events */
    wmb_surprise_callback_t on_surprise;    /* called when surprise stored */
    void*                   surprise_ctx;   /* user_data for on_surprise */
    wmb_replay_callback_t   on_replay;      /* called during consolidation replay */
    void*                   replay_ctx;     /* user_data for on_replay */

    wmb_config_t config;
    wmb_stats_t  stats;
    bool         initialized;
} world_model_bridge_t;

/* ============================================================================
 * API
 * ============================================================================ */

world_model_bridge_t* wmb_create(const wmb_config_t* config);
void wmb_destroy(world_model_bridge_t* bridge);

/** Connect all subsystems */
void wmb_connect(world_model_bridge_t* bridge,
                  struct jepa_predictor* jepa,
                  struct intuitive_physics_engine* physics,
                  struct world_simulator* world_sim,
                  struct scene_graph* scene,
                  struct entity_tracker* tracker);

/** Register callback for surprise events (routes to prime resonance) */
void wmb_set_surprise_callback(world_model_bridge_t* bridge,
                                 wmb_surprise_callback_t callback,
                                 void* user_data);

/** Register callback for replay events (routes to JEPA training) */
void wmb_set_replay_callback(world_model_bridge_t* bridge,
                               wmb_replay_callback_t callback,
                               void* user_data);

/**
 * @brief Predict-and-verify cycle
 *
 * 1. Get current physical state from simulation
 * 2. Encode to latent → feed to JEPA → get predicted next latent
 * 3. Decode predicted latent to predicted physical state
 * 4. Step simulation to get actual next physical state
 * 5. Compute physical prediction error
 * 6. If error > threshold → store as surprise event
 * 7. Return error for JEPA training
 */
float wmb_predict_and_verify(world_model_bridge_t* bridge, float dt);

/**
 * @brief Replay surprise events through JEPA (call during consolidation/sleep)
 *
 * Samples from the replay buffer, re-presents the before→after latent pairs,
 * and trains JEPA on the correct transitions. This is analogous to
 * hippocampal replay during slow-wave sleep.
 */
uint32_t wmb_replay_consolidation(world_model_bridge_t* bridge,
                                    uint32_t num_replays);

/** Encode physical state to latent vector */
void wmb_encode_physical(const world_model_bridge_t* bridge,
                          const wmb_physical_state_t* physical,
                          float* latent_out);

/** Decode latent vector to physical state */
void wmb_decode_latent(const world_model_bridge_t* bridge,
                        const float* latent,
                        wmb_physical_state_t* physical_out);

/** Store a surprise event in the replay buffer */
void wmb_store_surprise(world_model_bridge_t* bridge,
                          const wmb_surprise_event_t* event);

/** Get the most surprising event in the buffer */
const wmb_surprise_event_t* wmb_most_surprising(const world_model_bridge_t* bridge);

/** Get stats */
wmb_stats_t wmb_get_stats(const world_model_bridge_t* bridge);

/** Default config */
wmb_config_t wmb_default_config(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WORLD_MODEL_BRIDGES_H */
