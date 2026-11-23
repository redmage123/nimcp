/**
 * @file nimcp_retina_circuit.h
 * @brief A small, biologically structured retina built from per-cell objects:
 *        rods, cones (L/M/S), horizontal, bipolar (ON/OFF, midget, diffuse,
 *        rod), amacrine (AII + wide-field spiking) and ganglion cells
 *        (midget ON/OFF, parasol ON/OFF), whose axons form the optic nerve.
 *
 * WHAT: retina_circuit_see() presents one image frame for a duration and
 *       returns what the ganglion cells sent: spike counts per cell and per
 *       type, ON/OFF rate maps, and the spikes that reached the LGN end of the
 *       optic nerve (after conduction delay).
 * WHY:  Athena's visual cortex needs a real front end: light adaptation,
 *       center-surround, ON/OFF split, colour opponency and rod/cone duplexity
 *       all happen in the retina, not in the cortex.
 * HOW:  graded cells (photoreceptors, horizontal, bipolar, AII) are
 *       single-compartment conductance models integrated here; every chemical
 *       synapse goes through the chemistry layer's neurotransmitter adapter
 *       (vesicle release -> cleft -> receptor occupancy), bipolar ribbons
 *       depress through plasticity/stp, ganglion cells and wide-field amacrine
 *       cells spike with core/neuron_models (Izhikevich), ganglion-cell
 *       dendrites/spines are core/dendrite objects, astrocytes
 *       (glial/astrocytes) sense and modulate patches of synapses, and the
 *       optic nerve (nimcp_optic_nerve.h) is axon_t + oligodendrocyte_t.
 *       Light adaptation and myelination persist across frames.
 *
 * Thread-compatible: one retina per thread; the only global touched per
 * call is the health-agent heartbeat.
 */
#ifndef NIMCP_RETINA_CIRCUIT_H
#define NIMCP_RETINA_CIRCUIT_H

#include <stdbool.h>
#include <stdint.h>
#include "perception/nimcp_optic_nerve.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct retina_circuit retina_circuit_t;
struct nimcp_health_agent;

typedef enum {
    RETINA_RGC_MIDGET_ON = 0,
    RETINA_RGC_MIDGET_OFF,
    RETINA_RGC_PARASOL_ON,
    RETINA_RGC_PARASOL_OFF,
    RETINA_RGC_TYPE_COUNT
} retina_rgc_type_t;

typedef enum {
    RETINA_PR_ROD = 0,
    RETINA_PR_L,
    RETINA_PR_M,
    RETINA_PR_S,
    RETINA_PR_TYPE_COUNT
} retina_pr_type_t;

typedef struct {
    uint32_t mosaic_w, mosaic_h;   /**< photoreceptor mosaic (16 x 16) */
    float fovea_radius;            /**< all-cone region, normalized radius (0.5) */
    float peripheral_cone_fraction;/**< cones among peripheral photoreceptors (0.25) */
    uint32_t horizontal_grid;      /**< horizontal cells = grid^2 (8 -> 64) */
    uint32_t diffuse_grid;         /**< diffuse ON/OFF bipolars & parasol RGCs per polarity = grid^2 (7) */
    uint32_t rod_bipolar_grid;     /**< rod bipolars = grid^2 (6) */
    uint32_t aii_grid;             /**< AII amacrines = grid^2 (4) */
    uint32_t wide_amacrine_grid;   /**< wide-field spiking amacrines = grid^2 (4) */
    uint32_t astrocyte_grid;       /**< astrocytes = grid^2 (4) */
    uint32_t map_w, map_h;         /**< ON/OFF output map size (8 x 8) */
    float dt_ms;                   /**< integration step (1.0) */
    float illuminance;             /**< absolute light level: pixel 255 = this many
                                        cone half-saturation units (10 = indoor photopic,
                                        1e-3 = scotopic). Changeable at run time. */
    float display_gamma;           /**< pixel -> linear light (2.2) */
    float rgc_noise;               /**< ganglion-cell input noise s.d. (2.0) */
    bool enable_dendrites;         /**< ganglion-cell inputs through core/dendrite spines */
    bool enable_astrocytes;        /**< astrocyte patches modulate bipolar->RGC synapses */
    bool enable_optic_nerve;       /**< deliver spikes to the LGN end through the nerve */
    optic_nerve_config_t nerve;
    uint64_t seed;
} retina_circuit_config_t;

/** Output of one retina_circuit_see() call. Arrays are owned by the retina
 *  and stay valid until the next see / destroy. */
typedef struct {
    uint32_t n_rgc;
    const uint16_t* rgc_spikes;    /**< [n_rgc] soma spikes during this call */
    const uint16_t* lgn_spikes;    /**< [n_rgc] spikes that arrived at the LGN end during
                                        this call (they left the eye up to one nerve delay
                                        earlier, so they include the previous frame's tail) */
    const uint8_t* rgc_type;       /**< [n_rgc] retina_rgc_type_t */
    const float* rgc_x;            /**< [n_rgc] receptive-field centre, 0..1 (image x) */
    const float* rgc_y;            /**< [n_rgc] receptive-field centre, 0..1 (image y) */
    const uint8_t* rgc_center_cone;/**< [n_rgc] retina_pr_type_t of a midget cell's centre cone
                                        (RETINA_PR_TYPE_COUNT for parasol cells) */
    float type_rate_hz[RETINA_RGC_TYPE_COUNT];   /**< mean soma rate per type */
    uint32_t type_count[RETINA_RGC_TYPE_COUNT];
    uint32_t map_w, map_h;
    const float* on_map;           /**< [map_h*map_w] mean ON-cell rate (Hz), row-major */
    const float* off_map;          /**< [map_h*map_w] mean OFF-cell rate (Hz) */
    float rod_activation;          /**< mean rod photocurrent, 0 (dark) .. 1 (saturated) */
    float cone_activation;         /**< mean cone photocurrent, 0..1 */
    float cone_type_activation[RETINA_PR_TYPE_COUNT]; /**< per photoreceptor type, 0..1 */
    float luminance_estimate;      /**< adapted mean light level, illuminance units */
    uint32_t lgn_total;            /**< sum of lgn_spikes */
    float lgn_mean_latency_ms;     /**< mean eye->LGN delay of the spikes delivered this call */
    float duration_ms;
    double sim_time_ms;            /**< retina clock at the end of the call */
    double wall_ms;                /**< wall time of this call */
} retina_output_t;

typedef struct {
    uint32_t n_photoreceptors, n_rods, n_cones, n_cone_type[RETINA_PR_TYPE_COUNT];
    uint32_t n_horizontal;
    uint32_t n_bipolar_on, n_bipolar_off;   /**< ON includes rod bipolars */
    uint32_t n_rod_bipolar;
    uint32_t n_amacrine, n_aii;
    uint32_t n_rgc, n_rgc_type[RETINA_RGC_TYPE_COUNT];
    uint32_t n_astrocytes, n_oligodendrocytes;
    uint32_t n_synapse_sites;      /**< neurotransmitter-adapter release sites */
    uint32_t n_spines;             /**< ganglion-cell dendritic spines */
    uint32_t n_gap_junctions;
} retina_circuit_counts_t;

retina_circuit_config_t retina_circuit_config_default(void);
retina_circuit_t* retina_circuit_create(const retina_circuit_config_t* cfg);
void retina_circuit_destroy(retina_circuit_t* r);

/**
 * @brief Look at a frame for duration_ms.
 * @param pixels row-major w*h*ch uint8 (ch = 3 RGB, 4 RGBA or 1 gray), area-resampled to the mosaic.
 * @param out    filled on success (may be NULL).
 * @return 0 on success, -1 on bad arguments.
 */
int retina_circuit_see(retina_circuit_t* r, const uint8_t* pixels, uint32_t w,
                       uint32_t h, uint32_t ch, float duration_ms, retina_output_t* out);

/** Absolute light level (see config.illuminance). */
int retina_circuit_set_illuminance(retina_circuit_t* r, float illuminance);
int retina_circuit_get_counts(const retina_circuit_t* r, retina_circuit_counts_t* out);
optic_nerve_t* retina_circuit_get_optic_nerve(retina_circuit_t* r); /**< borrowed; NULL if disabled */

/** Persist / restore adaptation state + myelination (binary). */
int retina_circuit_save(const retina_circuit_t* r, const char* path);
int retina_circuit_load(retina_circuit_t* r, const char* path);

/** Self-repair hook (generated by NIMCP_DECLARE_HEALTH_AGENT_ATOMIC). */
void retina_circuit_set_health_agent(struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RETINA_CIRCUIT_H */
