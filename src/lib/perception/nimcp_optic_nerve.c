/**
 * @file nimcp_optic_nerve.c
 * @brief Optic nerve: ganglion-cell axons + oligodendrocytes + delay line.
 *
 * WHAT: Carries retinal ganglion-cell spikes to the LGN end with per-axon
 *       conduction delays set by oligodendrocyte myelination.
 * WHY:  see nimcp_optic_nerve.h — the thalamus must receive retinal spikes
 *       with nerve timing, and myelin plasticity must be able to change it.
 * HOW:  per axon: one axon_t (core/axon), which is itself the delay line:
 *       axon_initiate_spike() gates (refractory / ATP / damage) and enqueues
 *       the spike at now + propagation delay; axon_pop_arrivals() collects it
 *       at the LGN end. Per NIMCP_OLIGO_MAX_AXONS axons: one oligodendrocyte_t
 *       that owns the myelination level; every change is copied to the axon_t
 *       with axon_set_myelination(), so the two objects never disagree.
 *
 * NOTE: the oligodendrocyte's Rushton velocity model and axon_t's
 *       (6 m/s per um x myelination) are separate formulas and do not agree
 *       (e.g. 13 vs 9 m/s at 1.5 um, full myelin). The axon_t delay is the one
 *       spikes obey; the oligodendrocyte's is reported in the stats.
 */

#include "perception/nimcp_optic_nerve.h"
#include "core/axon/nimcp_axon.h"
#include "glial/oligodendrocytes/nimcp_oligodendrocytes.h"
#include "utils/memory/nimcp_memory.h"
#include <math.h>
#include <string.h>

#define ON_MAGIC   0x4F4E5256u   /* "ONRV" */
#define ON_VERSION 1u

struct optic_nerve {
    optic_nerve_config_t cfg;
    uint32_t n_axons;
    axon_t** axons;
    float* diameter_um;
    uint32_t* oligo_of;            /**< axon -> oligodendrocyte index */
    uint64_t* delay_us;            /**< cached propagation delay */
    float* oligo_velocity;         /**< cached oligodendrocyte velocity (m/s) */

    uint32_t n_oligos;
    oligodendrocyte_t** oligos;

    uint64_t now_us;

    uint64_t spikes_in, spikes_conducted, spikes_blocked, spikes_delivered;
    double latency_sum_us;
};

optic_nerve_config_t optic_nerve_config_default(void)
{
    optic_nerve_config_t c;
    memset(&c, 0, sizeof(c));
    c.length_um = 40000.0F;          /* optic nerve + chiasm + tract, ~4 cm */
    c.initial_myelination = 0.7F;
    c.axons_per_oligo = 40;
    c.activity_myelination = true;
    c.full_myelin_rate_hz = 60.0F;
    return c;
}

/* Copy the oligodendrocyte's myelination for axon i into its axon_t and
 * refresh the cached delay. Keeps the two objects consistent. */
static void on_sync_axon(optic_nerve_t* on, uint32_t i)
{
    oligodendrocyte_t* ol = on->oligos[on->oligo_of[i]];
    float m = oligodendrocyte_get_myelination_level(ol, i);
    axon_set_myelination(on->axons[i], m);          /* recalculates conduction */
    float d_ms = axon_get_propagation_delay(on->axons[i]);
    on->delay_us[i] = (uint64_t)llroundf(d_ms * 1000.0F);
    on->oligo_velocity[i] = oligodendrocyte_compute_conduction_velocity(
        ol, i, NIMCP_OLIGO_BASE_VELOCITY_MS);
}

optic_nerve_t* optic_nerve_create(uint32_t n_axons, const float* diameters_um,
                                  const optic_nerve_config_t* cfg)
{
    if (n_axons == 0) return NULL;
    optic_nerve_t* on = (optic_nerve_t*)nimcp_calloc(1, sizeof(*on));
    if (!on) return NULL;
    on->cfg = cfg ? *cfg : optic_nerve_config_default();
    if (on->cfg.axons_per_oligo == 0 || on->cfg.axons_per_oligo > NIMCP_OLIGO_MAX_AXONS)
        on->cfg.axons_per_oligo = NIMCP_OLIGO_MAX_AXONS;
    if (on->cfg.length_um <= 0.0F) on->cfg.length_um = 40000.0F;
    if (on->cfg.full_myelin_rate_hz <= 0.0F) on->cfg.full_myelin_rate_hz = 60.0F;
    on->n_axons = n_axons;

    on->axons = (axon_t**)nimcp_calloc(n_axons, sizeof(axon_t*));
    on->diameter_um = (float*)nimcp_calloc(n_axons, sizeof(float));
    on->oligo_of = (uint32_t*)nimcp_calloc(n_axons, sizeof(uint32_t));
    on->delay_us = (uint64_t*)nimcp_calloc(n_axons, sizeof(uint64_t));
    on->oligo_velocity = (float*)nimcp_calloc(n_axons, sizeof(float));
    on->n_oligos = (n_axons + on->cfg.axons_per_oligo - 1) / on->cfg.axons_per_oligo;
    on->oligos = (oligodendrocyte_t**)nimcp_calloc(on->n_oligos, sizeof(oligodendrocyte_t*));
    if (!on->axons || !on->diameter_um || !on->oligo_of || !on->delay_us ||
        !on->oligo_velocity || !on->oligos) {
        optic_nerve_destroy(on);
        return NULL;
    }

    /* Oligodendrocytes sit along the nerve, one per bundle of axons. */
    for (uint32_t k = 0; k < on->n_oligos; k++) {
        float z = on->cfg.length_um * ((float)k + 0.5F) / (float)on->n_oligos;
        on->oligos[k] = oligodendrocyte_create(k, 0.0F, 0.0F, z, on->cfg.axons_per_oligo);
        if (!on->oligos[k]) { optic_nerve_destroy(on); return NULL; }
        oligodendrocyte_set_full_myelin_rate(on->oligos[k], on->cfg.full_myelin_rate_hz);
    }

    for (uint32_t i = 0; i < n_axons; i++) {
        float d = (diameters_um && diameters_um[i] > 0.0F) ? diameters_um[i] : 1.0F;
        on->diameter_um[i] = d;
        /* Eye at z = 0, LGN at z = length; axons spread across a 3 mm bundle. */
        float x = 3000.0F * ((float)(i % 16) / 16.0F - 0.5F);
        float y = 3000.0F * ((float)((i / 16) % 16) / 16.0F - 0.5F);
        const float start[3] = { x, y, 0.0F };
        const float end[3] = { x, y, on->cfg.length_um };
        on->axons[i] = axon_create_with_positions(i, AXON_TYPE_MYELINATED, i, i,
                                                  start, end, d);
        if (!on->axons[i]) { optic_nerve_destroy(on); return NULL; }

        uint32_t k = i / on->cfg.axons_per_oligo;
        on->oligo_of[i] = k;
        if (oligodendrocyte_assign_axon_at(on->oligos[k], i, x, y,
                                           0.5F * on->cfg.length_um, d,
                                           on->cfg.length_um) != NIMCP_SUCCESS) {
            optic_nerve_destroy(on);
            return NULL;
        }
        oligodendrocyte_set_myelination_level(on->oligos[k], i, on->cfg.initial_myelination);
        on_sync_axon(on, i);
    }
    return on;
}

void optic_nerve_destroy(optic_nerve_t* on)
{
    if (!on) return;
    if (on->axons) {
        for (uint32_t i = 0; i < on->n_axons; i++) axon_destroy(on->axons[i]);
        nimcp_free(on->axons);
    }
    if (on->oligos) {
        for (uint32_t k = 0; k < on->n_oligos; k++) oligodendrocyte_destroy(on->oligos[k]);
        nimcp_free(on->oligos);
    }
    nimcp_free(on->diameter_um);
    nimcp_free(on->oligo_of);
    nimcp_free(on->delay_us);
    nimcp_free(on->oligo_velocity);
    nimcp_free(on);
}

int optic_nerve_step(optic_nerve_t* on, const uint8_t* spikes_in,
                     uint16_t* arrivals_out, float dt_ms)
{
    if (!on || dt_ms <= 0.0F) return -1;
    const uint64_t t = on->now_us;
    const uint64_t t_next = t + (uint64_t)llroundf(dt_ms * 1000.0F);
    int delivered = 0;
    axon_spike_event_t ev[8];

    for (uint32_t i = 0; i < on->n_axons; i++) {
        axon_t* ax = on->axons[i];
        /* 1. refractory / ATP bookkeeping; launch a new spike at the eye end
              (the axon enqueues it at t + its propagation delay) */
        axon_step(ax, t, dt_ms);
        if (spikes_in && spikes_in[i]) {
            on->spikes_in++;
            if (axon_initiate_spike(ax, t, 1.0F)) on->spikes_conducted++;
            else on->spikes_blocked++;
        }
        /* 2. deliver everything that reaches the LGN end by the end of the step */
        uint32_t k;
        while ((k = axon_pop_arrivals(ax, t_next, ev, 8)) > 0) {
            for (uint32_t e = 0; e < k; e++) {
                if (arrivals_out && arrivals_out[i] < UINT16_MAX) arrivals_out[i]++;
                on->latency_sum_us += (double)(ev[e].arrival_time - ev[e].initiation_time);
            }
            on->spikes_delivered += k;
            delivered += (int)k;
            if (k < 8) break;
        }
    }
    on->now_us = t_next;
    return delivered;
}

int optic_nerve_update_myelination(optic_nerve_t* on, const float* rates_hz, float dt_s)
{
    if (!on || dt_s <= 0.0F) return -1;
    const uint64_t t = on->now_us;

    if (rates_hz) {
        for (uint32_t i = 0; i < on->n_axons; i++) {
            oligodendrocyte_t* ol = on->oligos[on->oligo_of[i]];
            float r = rates_hz[i] > 0.0F ? rates_hz[i] : 0.0F;
            /* Metabolic demand grows with firing (an AP costs ATP). */
            oligodendrocyte_set_axon_demand(ol, i, 0.2F + r / 20.0F);
            /* activity_score is a firing rate in Hz; the oligodendrocyte maps
               it to a target via its full-myelination rate (set at create) */
            if (on->cfg.activity_myelination)
                oligodendrocyte_track_activity(ol, i, r, t);
        }
    }

    for (uint32_t k = 0; k < on->n_oligos; k++) {
        oligodendrocyte_update_lactate_shuttle(on->oligos[k], dt_s);
        if (on->cfg.activity_myelination && rates_hz)
            oligodendrocyte_remodel_myelination(on->oligos[k], dt_s);
    }
    for (uint32_t i = 0; i < on->n_axons; i++) {
        oligodendrocyte_t* ol = on->oligos[on->oligo_of[i]];
        axon_receive_lactate(on->axons[i], oligodendrocyte_get_axon_lactate(ol, i));
        if (on->cfg.activity_myelination && rates_hz) on_sync_axon(on, i);
    }
    return 0;
}

int optic_nerve_set_myelination(optic_nerve_t* on, int32_t axon, float level)
{
    if (!on) return -1;
    if (level < 0.0F) level = 0.0F;
    if (level > 1.0F) level = 1.0F;
    uint32_t lo = 0, hi = on->n_axons;
    if (axon >= 0) {
        if ((uint32_t)axon >= on->n_axons) return -1;
        lo = (uint32_t)axon;
        hi = lo + 1;
    }
    for (uint32_t i = lo; i < hi; i++) {
        oligodendrocyte_set_myelination_level(on->oligos[on->oligo_of[i]], i, level);
        on_sync_axon(on, i);
    }
    return 0;
}

float optic_nerve_get_delay_ms(const optic_nerve_t* on, uint32_t axon)
{
    if (!on || axon >= on->n_axons) return 0.0F;
    return (float)on->delay_us[axon] / 1000.0F;
}

float optic_nerve_get_myelination(const optic_nerve_t* on, uint32_t axon)
{
    if (!on || axon >= on->n_axons) return 0.0F;
    return oligodendrocyte_get_myelination_level(on->oligos[on->oligo_of[axon]], axon);
}

uint32_t optic_nerve_num_axons(const optic_nerve_t* on)
{
    return on ? on->n_axons : 0;
}

int optic_nerve_get_stats(const optic_nerve_t* on, optic_nerve_stats_t* out)
{
    if (!on || !out) return -1;
    memset(out, 0, sizeof(*out));
    out->n_axons = on->n_axons;
    out->n_oligodendrocytes = on->n_oligos;
    out->min_delay_ms = INFINITY;
    double sd = 0.0, sv = 0.0, sm = 0.0, so = 0.0;
    for (uint32_t i = 0; i < on->n_axons; i++) {
        float d = (float)on->delay_us[i] / 1000.0F;
        sd += d;
        if (d < out->min_delay_ms) out->min_delay_ms = d;
        if (d > out->max_delay_ms) out->max_delay_ms = d;
        sv += axon_calculate_velocity(on->axons[i]);
        sm += optic_nerve_get_myelination(on, i);
        so += on->oligo_velocity[i];
    }
    out->mean_delay_ms = (float)(sd / on->n_axons);
    out->mean_velocity_ms = (float)(sv / on->n_axons);
    out->mean_myelination = (float)(sm / on->n_axons);
    out->mean_oligo_velocity_ms = (float)(so / on->n_axons);
    out->spikes_in = on->spikes_in;
    out->spikes_conducted = on->spikes_conducted;
    out->spikes_blocked = on->spikes_blocked;
    out->spikes_delivered = on->spikes_delivered;
    for (uint32_t i = 0; i < on->n_axons; i++) out->in_flight += axon_spikes_in_flight(on->axons[i]);
    out->mean_latency_ms = on->spikes_delivered
        ? (float)(on->latency_sum_us / (double)on->spikes_delivered / 1000.0) : 0.0F;
    out->time_ms = (double)on->now_us / 1000.0;
    return 0;
}

int optic_nerve_write(const optic_nerve_t* on, FILE* fp)
{
    if (!on || !fp) return -1;
    uint32_t hdr[3] = { ON_MAGIC, ON_VERSION, on->n_axons };
    if (fwrite(hdr, sizeof(hdr), 1, fp) != 1) return -1;
    for (uint32_t i = 0; i < on->n_axons; i++) {
        float m = optic_nerve_get_myelination(on, i);
        if (fwrite(&m, sizeof(m), 1, fp) != 1) return -1;
    }
    return 0;
}

int optic_nerve_read(optic_nerve_t* on, FILE* fp)
{
    if (!on || !fp) return -1;
    uint32_t hdr[3];
    if (fread(hdr, sizeof(hdr), 1, fp) != 1) return -1;
    if (hdr[0] != ON_MAGIC || hdr[1] != ON_VERSION || hdr[2] != on->n_axons) return -1;
    for (uint32_t i = 0; i < on->n_axons; i++) {
        float m;
        if (fread(&m, sizeof(m), 1, fp) != 1 || !isfinite(m)) return -1;
        optic_nerve_set_myelination(on, (int32_t)i, m);
    }
    return 0;
}
