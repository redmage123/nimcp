/**
 * @file nimcp_optic_nerve.h
 * @brief Optic nerve: retinal ganglion-cell axons, myelinated by
 *        oligodendrocytes, carrying spikes to the LGN with conduction delays.
 *
 * WHAT: One axon_t (core/axon) per retinal ganglion cell, grouped onto
 *       oligodendrocyte_t cells (glial/oligodendrocytes, up to
 *       NIMCP_OLIGO_MAX_AXONS axons each) that set its myelination. A spike
 *       entering at the eye end is delivered at the LGN end after the
 *       axon's propagation delay. Activity-dependent myelination: axons that
 *       fire more are wrapped more by their oligodendrocyte, which shortens
 *       their delay.
 * WHY:  the retina's output must reach the thalamus with the timing a real
 *       optic nerve imposes (a few ms for well-myelinated fast axons, tens of
 *       ms for thin or poorly-myelinated ones), and myelin plasticity is the
 *       one learning mechanism the nerve itself has.
 * HOW:  - axon_t carries morphology, myelination, refractoriness, ATP and
 *         the spikes in flight: axon_initiate_spike() gates and enqueues at
 *         now + axon_get_propagation_delay(); axon_pop_arrivals() delivers.
 *       - oligodendrocyte_set_myelination_level() / _remodel_myelination()
 *         own the myelination level; every change is copied to the axon_t
 *         with axon_set_myelination() so the two never disagree.
 *       - the oligodendrocyte lactate shuttle feeds axon_receive_lactate().
 *
 * Units: time in ms at this API (microseconds internally, as core/axon
 * expects), length in µm, velocity in m/s.
 *
 * Thread-compatible: one nerve per thread, no shared mutable globals.
 */
#ifndef NIMCP_OPTIC_NERVE_H
#define NIMCP_OPTIC_NERVE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct optic_nerve optic_nerve_t;

typedef struct {
    float length_um;            /**< eye -> LGN path length (40000 µm) */
    float initial_myelination;  /**< 0..1 myelination at creation (0.7) */
    uint32_t axons_per_oligo;   /**< axons per oligodendrocyte (<= 50; 40) */
    bool activity_myelination;  /**< enable activity-dependent remodelling */
    float full_myelin_rate_hz;  /**< firing rate that drives an axon to full myelination (60) */
} optic_nerve_config_t;

typedef struct {
    uint32_t n_axons;
    uint32_t n_oligodendrocytes;
    float mean_delay_ms;
    float min_delay_ms;
    float max_delay_ms;
    float mean_velocity_ms;     /**< m/s, from axon_calculate_velocity */
    float mean_myelination;     /**< 0..1 (oligodendrocyte value) */
    float mean_oligo_velocity_ms; /**< m/s, oligodendrocyte's own saltatory model */
    uint64_t spikes_in;         /**< spikes offered at the eye end */
    uint64_t spikes_conducted;  /**< accepted by axon_initiate_spike */
    uint64_t spikes_blocked;    /**< refused (refractory / ATP / damage) */
    uint64_t spikes_delivered;  /**< arrived at the LGN end */
    uint32_t in_flight;         /**< currently travelling */
    float mean_latency_ms;      /**< measured arrival - initiation (all time) */
    double time_ms;             /**< nerve clock */
} optic_nerve_stats_t;

/** Defaults listed in the field comments. */
optic_nerve_config_t optic_nerve_config_default(void);

/**
 * @brief Create a nerve of n_axons axons.
 * @param diameters_um per-axon diameter (NULL = 1.0 µm for all). Parasol
 *        (magnocellular) axons are thicker (~1.5 µm) than midget (~0.8 µm).
 * @param cfg NULL = defaults.
 */
optic_nerve_t* optic_nerve_create(uint32_t n_axons, const float* diameters_um,
                                  const optic_nerve_config_t* cfg);
void optic_nerve_destroy(optic_nerve_t* on);

/**
 * @brief Advance the nerve by dt_ms.
 * @param spikes_in  n_axons bytes, nonzero = the ganglion cell fired at the
 *                   start of this step (NULL = no input).
 * @param arrivals_out n_axons counters, INCREMENTED by spikes arriving at the
 *                   LGN end during this step (NULL = discard).
 * @return number of spikes delivered this step, or -1 on error.
 */
int optic_nerve_step(optic_nerve_t* on, const uint8_t* spikes_in,
                     uint16_t* arrivals_out, float dt_ms);

/**
 * @brief Activity-dependent myelination + metabolic support over dt_s.
 * @param rates_hz per-axon firing rate over the period just simulated.
 *        No-op for remodelling when cfg.activity_myelination is false (the
 *        lactate shuttle still runs).
 */
int optic_nerve_update_myelination(optic_nerve_t* on, const float* rates_hz,
                                   float dt_s);

/** Set myelination (0..1) of one axon, or all when axon < 0. */
int optic_nerve_set_myelination(optic_nerve_t* on, int32_t axon, float level);

float optic_nerve_get_delay_ms(const optic_nerve_t* on, uint32_t axon);
float optic_nerve_get_myelination(const optic_nerve_t* on, uint32_t axon);
uint32_t optic_nerve_num_axons(const optic_nerve_t* on);
int optic_nerve_get_stats(const optic_nerve_t* on, optic_nerve_stats_t* out);

/** Persist / restore myelination (binary, self-describing). */
int optic_nerve_write(const optic_nerve_t* on, FILE* fp);
int optic_nerve_read(optic_nerve_t* on, FILE* fp);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OPTIC_NERVE_H */
