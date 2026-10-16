/**
 * @file nimcp_substrate_gpu.c
 * @brief Unified GPU Neural Substrate Context Implementation
 *
 * WHAT: Implementation of substrate GPU context management and high-level operations
 * WHY:  Provides cohesive interface for axon, dendrite, myelin, glial, neuromodulator ops
 * HOW:  Wraps kernel backend substrate ops with context management and convenience functions
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#include "gpu/substrate/nimcp_substrate_gpu.h"
#include "gpu/backend/nimcp_kernel_backend.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>

#define LOG_MODULE "SUBSTRATE_GPU"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(substrate_gpu)

//=============================================================================
// Default Configuration
//=============================================================================

substrate_gpu_config_t substrate_gpu_default_config(void)
{
    substrate_gpu_config_t config = {0};

    // Axon defaults
    config.axon.max_axons = 100000;
    config.axon.max_segments = 10;
    config.axon.refractory_period_ms = 2.0f;
    config.axon.base_velocity_ms = 1.0f;
    config.axon.myelin_multiplier = 100.0f;

    // Dendrite defaults
    config.dendrite.max_dendrites = 100000;
    config.dendrite.max_segments = 20;
    config.dendrite.max_spines = 500000;
    config.dendrite.default_Rm = 20000.0f;   // ohm-cm2
    config.dendrite.default_Cm = 1.0f;       // uF/cm2
    config.dendrite.default_Ra = 200.0f;     // ohm-cm
    config.dendrite.nmda_threshold = 20.0f;
    config.dendrite.tau_calcium_ms = 50.0f;

    // Myelin defaults
    config.myelin.max_sheaths = 100000;
    config.myelin.optimal_g_ratio = 0.6f;
    config.myelin.plasticity_rate = 0.01f;
    config.myelin.max_thickness_um = 2.0f;
    config.myelin.max_internode_um = 200.0f;
    config.myelin.temperature_c = 37.0f;

    // Neuromodulator defaults
    config.neuromod.max_pools = 1000;
    config.neuromod.n_types = 4;  // DA, 5HT, ACh, NE
    config.neuromod.tonic_tau_ms = 1000.0f;
    config.neuromod.phasic_decay = 0.1f;
    config.neuromod.decay_rates = NULL;

    // Glial defaults
    config.glial.max_astrocytes = 50000;
    config.glial.max_microglia = 10000;
    config.glial.max_opcs = 5000;
    config.glial.max_neighbors = 6;
    config.glial.calcium_diffusion_rate = 0.5f;
    config.glial.microglia_threshold = 0.3f;

    // Metabolic defaults
    config.metabolic.n_regions = 100;
    config.metabolic.atp_consumption_rate = 0.1f;
    config.metabolic.atp_recovery_rate = 0.05f;
    config.metabolic.lactate_clearance_rate = 0.1f;

    // General
    config.enable_async_ops = true;
    config.enable_mixed_precision = false;

    return config;
}

//=============================================================================
// Helper: Create tensor helper
//=============================================================================

static nimcp_gpu_tensor_t* create_tensor_1d(nimcp_gpu_context_t* ctx, uint32_t size)
{
    if (!ctx || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_tensor_1d: ctx is NULL");
        return NULL;
    }

    size_t dims[1] = {size};
    nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    if (tensor) {
        nimcp_gpu_zeros(ctx, tensor);
    }
    return tensor;
}

static nimcp_gpu_tensor_t* create_tensor_2d(nimcp_gpu_context_t* ctx, uint32_t dim0, uint32_t dim1)
{
    if (!ctx || dim0 == 0 || dim1 == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_tensor_2d: ctx is NULL");
        return NULL;
    }

    size_t dims[2] = {dim0, dim1};
    nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (tensor) {
        nimcp_gpu_zeros(ctx, tensor);
    }
    return tensor;
}

//=============================================================================
// Context Management
//=============================================================================

substrate_gpu_context_t* substrate_gpu_create(
    nimcp_gpu_context_t* gpu_ctx,
    const substrate_gpu_config_t* config)
{
    if (!gpu_ctx) {
        LOG_ERROR("NULL GPU context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gpu_ctx is NULL");

        return NULL;
    }

    substrate_gpu_context_t* ctx = nimcp_calloc(1, sizeof(substrate_gpu_context_t));
    if (!ctx) {
        LOG_ERROR("Failed to allocate substrate GPU context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "substrate_gpu_create: context allocation failed");
        return NULL;
    }

    ctx->gpu_ctx = gpu_ctx;
    ctx->config = config ? *config : substrate_gpu_default_config();

    // Get substrate operations from kernel backend
    nimcp_kernel_backend_t* backend = nimcp_get_kernel_backend();
    if (backend) {
        ctx->ops = &backend->substrate;
    } else {
        LOG_WARN("Kernel backend not initialized, substrate operations unavailable");
        ctx->ops = NULL;
    }

    ctx->initialized = true;
    LOG_INFO("Substrate GPU context created");

    return ctx;
}

void substrate_gpu_destroy(substrate_gpu_context_t* ctx)
{
    if (!ctx) return;

    // Free axon tensors
    if (ctx->axon.signals) nimcp_gpu_tensor_destroy(ctx->axon.signals);
    if (ctx->axon.velocities) nimcp_gpu_tensor_destroy(ctx->axon.velocities);
    if (ctx->axon.myelination) nimcp_gpu_tensor_destroy(ctx->axon.myelination);
    if (ctx->axon.lengths) nimcp_gpu_tensor_destroy(ctx->axon.lengths);
    if (ctx->axon.delays) nimcp_gpu_tensor_destroy(ctx->axon.delays);
    if (ctx->axon.refractory) nimcp_gpu_tensor_destroy(ctx->axon.refractory);

    // Free dendrite tensors
    if (ctx->dendrite.voltages) nimcp_gpu_tensor_destroy(ctx->dendrite.voltages);
    if (ctx->dendrite.cable_Rm) nimcp_gpu_tensor_destroy(ctx->dendrite.cable_Rm);
    if (ctx->dendrite.cable_Cm) nimcp_gpu_tensor_destroy(ctx->dendrite.cable_Cm);
    if (ctx->dendrite.cable_Ra) nimcp_gpu_tensor_destroy(ctx->dendrite.cable_Ra);
    if (ctx->dendrite.mg_block) nimcp_gpu_tensor_destroy(ctx->dendrite.mg_block);
    if (ctx->dendrite.nmda_current) nimcp_gpu_tensor_destroy(ctx->dendrite.nmda_current);
    if (ctx->dendrite.nmda_spikes) nimcp_gpu_tensor_destroy(ctx->dendrite.nmda_spikes);
    if (ctx->dendrite.calcium) nimcp_gpu_tensor_destroy(ctx->dendrite.calcium);
    if (ctx->dendrite.calcium_decay) nimcp_gpu_tensor_destroy(ctx->dendrite.calcium_decay);
    if (ctx->dendrite.vgcc_current) nimcp_gpu_tensor_destroy(ctx->dendrite.vgcc_current);
    if (ctx->dendrite.bap_signal) nimcp_gpu_tensor_destroy(ctx->dendrite.bap_signal);
    if (ctx->dendrite.bap_attenuation) nimcp_gpu_tensor_destroy(ctx->dendrite.bap_attenuation);
    if (ctx->dendrite.soma_spike) nimcp_gpu_tensor_destroy(ctx->dendrite.soma_spike);

    // Free myelin tensors
    if (ctx->myelin.axon_diameter) nimcp_gpu_tensor_destroy(ctx->myelin.axon_diameter);
    if (ctx->myelin.fiber_diameter) nimcp_gpu_tensor_destroy(ctx->myelin.fiber_diameter);
    if (ctx->myelin.g_ratio) nimcp_gpu_tensor_destroy(ctx->myelin.g_ratio);
    if (ctx->myelin.is_optimal) nimcp_gpu_tensor_destroy(ctx->myelin.is_optimal);
    if (ctx->myelin.internode_length) nimcp_gpu_tensor_destroy(ctx->myelin.internode_length);
    if (ctx->myelin.temperature) nimcp_gpu_tensor_destroy(ctx->myelin.temperature);
    if (ctx->myelin.velocity) nimcp_gpu_tensor_destroy(ctx->myelin.velocity);
    if (ctx->myelin.activity) nimcp_gpu_tensor_destroy(ctx->myelin.activity);
    if (ctx->myelin.ol_signal) nimcp_gpu_tensor_destroy(ctx->myelin.ol_signal);
    if (ctx->myelin.thickness) nimcp_gpu_tensor_destroy(ctx->myelin.thickness);
    if (ctx->myelin.sheath_length) nimcp_gpu_tensor_destroy(ctx->myelin.sheath_length);

    // Free neuromodulator tensors
    if (ctx->neuromod.concentrations) nimcp_gpu_tensor_destroy(ctx->neuromod.concentrations);
    if (ctx->neuromod.decay_rates) nimcp_gpu_tensor_destroy(ctx->neuromod.decay_rates);
    if (ctx->neuromod.tonic_level) nimcp_gpu_tensor_destroy(ctx->neuromod.tonic_level);
    if (ctx->neuromod.total_level) nimcp_gpu_tensor_destroy(ctx->neuromod.total_level);
    if (ctx->neuromod.receptor_density) nimcp_gpu_tensor_destroy(ctx->neuromod.receptor_density);
    if (ctx->neuromod.modulation) nimcp_gpu_tensor_destroy(ctx->neuromod.modulation);

    // Free glial tensors - astrocyte
    if (ctx->glial.astro_calcium) nimcp_gpu_tensor_destroy(ctx->glial.astro_calcium);
    if (ctx->glial.astro_ip3) nimcp_gpu_tensor_destroy(ctx->glial.astro_ip3);
    if (ctx->glial.astro_gaps) nimcp_gpu_tensor_destroy(ctx->glial.astro_gaps);
    if (ctx->glial.astro_wave) nimcp_gpu_tensor_destroy(ctx->glial.astro_wave);
    if (ctx->glial.astro_threshold) nimcp_gpu_tensor_destroy(ctx->glial.astro_threshold);
    if (ctx->glial.astro_glu) nimcp_gpu_tensor_destroy(ctx->glial.astro_glu);
    if (ctx->glial.astro_atp) nimcp_gpu_tensor_destroy(ctx->glial.astro_atp);

    // Free glial tensors - microglia
    if (ctx->glial.micro_damage) nimcp_gpu_tensor_destroy(ctx->glial.micro_damage);
    if (ctx->glial.micro_anti) nimcp_gpu_tensor_destroy(ctx->glial.micro_anti);
    if (ctx->glial.micro_state) nimcp_gpu_tensor_destroy(ctx->glial.micro_state);
    if (ctx->glial.micro_phago) nimcp_gpu_tensor_destroy(ctx->glial.micro_phago);

    // Free glial tensors - OPC
    if (ctx->glial.opc_activity) nimcp_gpu_tensor_destroy(ctx->glial.opc_activity);
    if (ctx->glial.opc_growth) nimcp_gpu_tensor_destroy(ctx->glial.opc_growth);
    if (ctx->glial.opc_diff_state) nimcp_gpu_tensor_destroy(ctx->glial.opc_diff_state);
    if (ctx->glial.opc_myelin_prod) nimcp_gpu_tensor_destroy(ctx->glial.opc_myelin_prod);

    // Free metabolic tensors
    if (ctx->metabolic.atp_levels) nimcp_gpu_tensor_destroy(ctx->metabolic.atp_levels);
    if (ctx->metabolic.oxygen_levels) nimcp_gpu_tensor_destroy(ctx->metabolic.oxygen_levels);
    if (ctx->metabolic.glucose_levels) nimcp_gpu_tensor_destroy(ctx->metabolic.glucose_levels);
    if (ctx->metabolic.capacity) nimcp_gpu_tensor_destroy(ctx->metabolic.capacity);
    if (ctx->metabolic.fatigue) nimcp_gpu_tensor_destroy(ctx->metabolic.fatigue);
    if (ctx->metabolic.lactate_levels) nimcp_gpu_tensor_destroy(ctx->metabolic.lactate_levels);
    if (ctx->metabolic.neural_activity) nimcp_gpu_tensor_destroy(ctx->metabolic.neural_activity);

    nimcp_free(ctx);
    LOG_INFO("Substrate GPU context destroyed");
}

//=============================================================================
// Tensor Initialization
//=============================================================================

int substrate_gpu_init_axons(substrate_gpu_context_t* ctx, uint32_t n_axons)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_init_axons: ctx is NULL");
        return -1;
    }
    if (!ctx->gpu_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_init_axons: gpu_ctx is NULL");
        return -1;
    }
    if (n_axons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_init_axons: n_axons is 0");
        return -1;
    }

    ctx->axon.n_axons = n_axons;
    ctx->axon.signals = create_tensor_1d(ctx->gpu_ctx, n_axons);
    ctx->axon.velocities = create_tensor_1d(ctx->gpu_ctx, n_axons);
    ctx->axon.myelination = create_tensor_1d(ctx->gpu_ctx, n_axons);
    ctx->axon.lengths = create_tensor_1d(ctx->gpu_ctx, n_axons);
    ctx->axon.delays = create_tensor_1d(ctx->gpu_ctx, n_axons);
    ctx->axon.refractory = create_tensor_1d(ctx->gpu_ctx, n_axons);

    if (!ctx->axon.signals || !ctx->axon.velocities || !ctx->axon.myelination ||
        !ctx->axon.lengths || !ctx->axon.delays || !ctx->axon.refractory) {
        LOG_ERROR("Failed to allocate axon tensors");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "substrate_gpu_init_axons: failed to allocate axon tensors");
        return -1;
    }

    // Initialize velocities to base velocity
    nimcp_gpu_fill(ctx->gpu_ctx, ctx->axon.velocities, ctx->config.axon.base_velocity_ms);

    LOG_INFO("Initialized %u axon tensors", n_axons);
    return 0;
}

int substrate_gpu_init_dendrites(
    substrate_gpu_context_t* ctx,
    uint32_t n_dendrites,
    uint32_t n_segments,
    uint32_t n_spines)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_init_dendrites: ctx is NULL");
        return -1;
    }
    if (!ctx->gpu_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_init_dendrites: gpu_ctx is NULL");
        return -1;
    }
    if (n_dendrites == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_init_dendrites: n_dendrites is 0");
        return -1;
    }
    if (n_segments == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_init_dendrites: n_segments is 0");
        return -1;
    }

    ctx->dendrite.n_dendrites = n_dendrites;
    ctx->dendrite.n_segments = n_segments;
    ctx->dendrite.n_spines = n_spines;

    ctx->dendrite.voltages = create_tensor_2d(ctx->gpu_ctx, n_dendrites, n_segments);
    ctx->dendrite.cable_Rm = create_tensor_1d(ctx->gpu_ctx, n_dendrites);
    ctx->dendrite.cable_Cm = create_tensor_1d(ctx->gpu_ctx, n_dendrites);
    ctx->dendrite.cable_Ra = create_tensor_1d(ctx->gpu_ctx, n_dendrites);
    ctx->dendrite.mg_block = create_tensor_1d(ctx->gpu_ctx, n_dendrites);
    ctx->dendrite.nmda_current = create_tensor_2d(ctx->gpu_ctx, n_dendrites, n_segments);
    ctx->dendrite.nmda_spikes = create_tensor_1d(ctx->gpu_ctx, n_dendrites);
    ctx->dendrite.bap_signal = create_tensor_2d(ctx->gpu_ctx, n_dendrites, n_segments);
    ctx->dendrite.bap_attenuation = create_tensor_2d(ctx->gpu_ctx, n_dendrites, n_segments);
    ctx->dendrite.soma_spike = create_tensor_1d(ctx->gpu_ctx, n_dendrites);

    if (n_spines > 0) {
        ctx->dendrite.calcium = create_tensor_1d(ctx->gpu_ctx, n_spines);
        ctx->dendrite.calcium_decay = create_tensor_1d(ctx->gpu_ctx, n_spines);
        ctx->dendrite.vgcc_current = create_tensor_1d(ctx->gpu_ctx, n_spines);
    }

    // Initialize cable parameters with defaults
    nimcp_gpu_fill(ctx->gpu_ctx, ctx->dendrite.cable_Rm, ctx->config.dendrite.default_Rm);
    nimcp_gpu_fill(ctx->gpu_ctx, ctx->dendrite.cable_Cm, ctx->config.dendrite.default_Cm);
    nimcp_gpu_fill(ctx->gpu_ctx, ctx->dendrite.cable_Ra, ctx->config.dendrite.default_Ra);
    nimcp_gpu_fill(ctx->gpu_ctx, ctx->dendrite.mg_block, 1.0f);  // Default Mg2+ concentration

    LOG_INFO("Initialized dendrite tensors: %u dendrites, %u segments, %u spines",
             n_dendrites, n_segments, n_spines);
    return 0;
}

int substrate_gpu_init_myelin(substrate_gpu_context_t* ctx, uint32_t n_axons)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_init_myelin: ctx is NULL");
        return -1;
    }
    if (!ctx->gpu_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_init_myelin: gpu_ctx is NULL");
        return -1;
    }
    if (n_axons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_init_myelin: n_axons is 0");
        return -1;
    }

    ctx->myelin.n_axons = n_axons;
    ctx->myelin.axon_diameter = create_tensor_1d(ctx->gpu_ctx, n_axons);
    ctx->myelin.fiber_diameter = create_tensor_1d(ctx->gpu_ctx, n_axons);
    ctx->myelin.g_ratio = create_tensor_1d(ctx->gpu_ctx, n_axons);
    ctx->myelin.is_optimal = create_tensor_1d(ctx->gpu_ctx, n_axons);
    ctx->myelin.internode_length = create_tensor_1d(ctx->gpu_ctx, n_axons);
    ctx->myelin.temperature = create_tensor_1d(ctx->gpu_ctx, 1);  // Scalar temperature
    ctx->myelin.velocity = create_tensor_1d(ctx->gpu_ctx, n_axons);
    ctx->myelin.activity = create_tensor_1d(ctx->gpu_ctx, n_axons);
    ctx->myelin.ol_signal = create_tensor_1d(ctx->gpu_ctx, n_axons);
    ctx->myelin.thickness = create_tensor_1d(ctx->gpu_ctx, n_axons);
    ctx->myelin.sheath_length = create_tensor_1d(ctx->gpu_ctx, n_axons);

    // Initialize temperature
    nimcp_gpu_fill(ctx->gpu_ctx, ctx->myelin.temperature, ctx->config.myelin.temperature_c);

    LOG_INFO("Initialized %u myelin tensors", n_axons);
    return 0;
}

int substrate_gpu_init_neuromod(
    substrate_gpu_context_t* ctx,
    uint32_t n_pools,
    uint32_t n_types,
    uint32_t n_synapses)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_init_neuromod: ctx is NULL");
        return -1;
    }
    if (!ctx->gpu_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_init_neuromod: gpu_ctx is NULL");
        return -1;
    }
    if (n_pools == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_init_neuromod: n_pools is 0");
        return -1;
    }
    if (n_types == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_init_neuromod: n_types is 0");
        return -1;
    }

    ctx->neuromod.n_pools = n_pools;
    ctx->neuromod.n_types = n_types;
    ctx->neuromod.n_synapses = n_synapses;

    ctx->neuromod.concentrations = create_tensor_2d(ctx->gpu_ctx, n_pools, n_types);
    ctx->neuromod.decay_rates = create_tensor_1d(ctx->gpu_ctx, n_types);
    ctx->neuromod.tonic_level = create_tensor_2d(ctx->gpu_ctx, n_pools, n_types);
    ctx->neuromod.total_level = create_tensor_2d(ctx->gpu_ctx, n_pools, n_types);

    if (n_synapses > 0) {
        ctx->neuromod.receptor_density = create_tensor_2d(ctx->gpu_ctx, n_synapses, n_types);
        ctx->neuromod.modulation = create_tensor_1d(ctx->gpu_ctx, n_synapses);

        // Initialize modulation to 1.0 (no modulation)
        nimcp_gpu_fill(ctx->gpu_ctx, ctx->neuromod.modulation, 1.0f);
    }

    // Set default decay rates (DA, 5HT, ACh, NE) - all types get a default value
    // Note: Full per-type initialization would require host->device copy
    // For now, use a uniform default decay rate
    nimcp_gpu_fill(ctx->gpu_ctx, ctx->neuromod.decay_rates, 0.1f);

    LOG_INFO("Initialized neuromod tensors: %u pools, %u types, %u synapses",
             n_pools, n_types, n_synapses);
    return 0;
}

int substrate_gpu_init_glial(
    substrate_gpu_context_t* ctx,
    uint32_t n_astrocytes,
    uint32_t n_microglia,
    uint32_t n_opcs,
    uint32_t n_neighbors)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_init_glial: ctx is NULL");
        return -1;
    }
    if (!ctx->gpu_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_init_glial: gpu_ctx is NULL");
        return -1;
    }

    ctx->glial.n_astrocytes = n_astrocytes;
    ctx->glial.n_microglia = n_microglia;
    ctx->glial.n_opcs = n_opcs;
    ctx->glial.n_neighbors = n_neighbors;

    // Astrocyte tensors
    if (n_astrocytes > 0) {
        ctx->glial.astro_calcium = create_tensor_1d(ctx->gpu_ctx, n_astrocytes);
        ctx->glial.astro_ip3 = create_tensor_1d(ctx->gpu_ctx, n_astrocytes);
        ctx->glial.astro_gaps = create_tensor_2d(ctx->gpu_ctx, n_astrocytes, n_neighbors);
        ctx->glial.astro_wave = create_tensor_1d(ctx->gpu_ctx, n_astrocytes);
        ctx->glial.astro_threshold = create_tensor_1d(ctx->gpu_ctx, n_astrocytes);
        ctx->glial.astro_glu = create_tensor_1d(ctx->gpu_ctx, n_astrocytes);
        ctx->glial.astro_atp = create_tensor_1d(ctx->gpu_ctx, n_astrocytes);

        // Initialize thresholds
        nimcp_gpu_fill(ctx->gpu_ctx, ctx->glial.astro_threshold, 0.7f);
    }

    // Microglia tensors
    if (n_microglia > 0) {
        ctx->glial.micro_damage = create_tensor_1d(ctx->gpu_ctx, n_microglia);
        ctx->glial.micro_anti = create_tensor_1d(ctx->gpu_ctx, n_microglia);
        ctx->glial.micro_state = create_tensor_1d(ctx->gpu_ctx, n_microglia);
        ctx->glial.micro_phago = create_tensor_1d(ctx->gpu_ctx, n_microglia);
    }

    // OPC tensors
    if (n_opcs > 0) {
        ctx->glial.opc_activity = create_tensor_1d(ctx->gpu_ctx, n_opcs);
        ctx->glial.opc_growth = create_tensor_1d(ctx->gpu_ctx, n_opcs);
        ctx->glial.opc_diff_state = create_tensor_1d(ctx->gpu_ctx, n_opcs);
        ctx->glial.opc_myelin_prod = create_tensor_1d(ctx->gpu_ctx, n_opcs);
    }

    LOG_INFO("Initialized glial tensors: %u astrocytes, %u microglia, %u OPCs",
             n_astrocytes, n_microglia, n_opcs);
    return 0;
}

int substrate_gpu_init_metabolic(substrate_gpu_context_t* ctx, uint32_t n_regions)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_init_metabolic: ctx is NULL");
        return -1;
    }
    if (!ctx->gpu_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_init_metabolic: gpu_ctx is NULL");
        return -1;
    }
    if (n_regions == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_init_metabolic: n_regions is 0");
        return -1;
    }

    ctx->metabolic.n_regions = n_regions;
    ctx->metabolic.atp_levels = create_tensor_1d(ctx->gpu_ctx, n_regions);
    ctx->metabolic.oxygen_levels = create_tensor_1d(ctx->gpu_ctx, n_regions);
    ctx->metabolic.glucose_levels = create_tensor_1d(ctx->gpu_ctx, n_regions);
    ctx->metabolic.capacity = create_tensor_1d(ctx->gpu_ctx, n_regions);
    ctx->metabolic.fatigue = create_tensor_1d(ctx->gpu_ctx, n_regions);
    ctx->metabolic.lactate_levels = create_tensor_1d(ctx->gpu_ctx, n_regions);
    ctx->metabolic.neural_activity = create_tensor_1d(ctx->gpu_ctx, n_regions);

    // Initialize to full capacity
    nimcp_gpu_fill(ctx->gpu_ctx, ctx->metabolic.atp_levels, 1.0f);
    nimcp_gpu_fill(ctx->gpu_ctx, ctx->metabolic.oxygen_levels, 1.0f);
    nimcp_gpu_fill(ctx->gpu_ctx, ctx->metabolic.glucose_levels, 1.0f);

    LOG_INFO("Initialized %u metabolic region tensors", n_regions);
    return 0;
}

//=============================================================================
// Axon Operations
//=============================================================================

int substrate_gpu_axon_propagate(
    substrate_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input_signals,
    float dt)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_axon_propagate: ctx is NULL");
        return -1;
    }
    if (!ctx->ops) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_axon_propagate: ops is NULL");
        return -1;
    }
    if (!ctx->ops->axon_propagate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_axon_propagate: axon_propagate op is NULL");
        return -1;
    }

    const nimcp_gpu_tensor_t* signals = input_signals ? input_signals : ctx->axon.signals;

    nimcp_kernel_error_t err = ctx->ops->axon_propagate(
        ctx->gpu_ctx,
        signals,
        ctx->axon.velocities,
        ctx->axon.myelination,
        ctx->axon.lengths,
        ctx->axon.signals,  // Output to internal buffer
        ctx->axon.delays,
        dt);

    return (err == NIMCP_KERNEL_SUCCESS) ? 0 : -1;
}

int substrate_gpu_axon_refractory(
    substrate_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* spikes,
    float dt)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_axon_refractory: ctx is NULL");
        return -1;
    }
    if (!ctx->ops) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_axon_refractory: ops is NULL");
        return -1;
    }
    if (!ctx->ops->axon_refractory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_axon_refractory: axon_refractory op is NULL");
        return -1;
    }

    const nimcp_gpu_tensor_t* spike_input = spikes ? spikes : ctx->axon.signals;

    nimcp_kernel_error_t err = ctx->ops->axon_refractory(
        ctx->gpu_ctx,
        ctx->axon.refractory,
        spike_input,
        ctx->config.axon.refractory_period_ms,
        dt);

    return (err == NIMCP_KERNEL_SUCCESS) ? 0 : -1;
}

int substrate_gpu_axon_step(
    substrate_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input_spikes,
    float dt)
{
    if (substrate_gpu_axon_propagate(ctx, input_spikes, dt) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_axon_step: validation failed");
        return -1;
    }
    if (substrate_gpu_axon_refractory(ctx, NULL, dt) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_axon_step: validation failed");
        return -1;
    }
    return 0;
}

//=============================================================================
// Dendrite Operations
//=============================================================================

int substrate_gpu_dendrite_cable(
    substrate_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* inputs,
    float dt)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_dendrite_cable: ctx is NULL");
        return -1;
    }
    if (!ctx->ops) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_dendrite_cable: ops is NULL");
        return -1;
    }
    if (!ctx->ops->dendrite_cable_integrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_dendrite_cable: dendrite_cable_integrate op is NULL");
        return -1;
    }

    nimcp_kernel_error_t err = ctx->ops->dendrite_cable_integrate(
        ctx->gpu_ctx,
        inputs ? inputs : ctx->dendrite.nmda_current,
        ctx->dendrite.cable_Rm,
        ctx->dendrite.cable_Cm,
        ctx->dendrite.cable_Ra,
        ctx->dendrite.voltages,
        dt);

    return (err == NIMCP_KERNEL_SUCCESS) ? 0 : -1;
}

int substrate_gpu_dendrite_nmda(substrate_gpu_context_t* ctx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_dendrite_nmda: ctx is NULL");
        return -1;
    }
    if (!ctx->ops) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_dendrite_nmda: ops is NULL");
        return -1;
    }
    if (!ctx->ops->dendrite_nmda) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_dendrite_nmda: dendrite_nmda op is NULL");
        return -1;
    }

    nimcp_kernel_error_t err = ctx->ops->dendrite_nmda(
        ctx->gpu_ctx,
        ctx->dendrite.voltages,
        ctx->dendrite.mg_block,
        ctx->dendrite.nmda_current,
        ctx->dendrite.nmda_spikes,
        ctx->config.dendrite.nmda_threshold);

    return (err == NIMCP_KERNEL_SUCCESS) ? 0 : -1;
}

int substrate_gpu_dendrite_calcium(substrate_gpu_context_t* ctx, float dt)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_dendrite_calcium: ctx is NULL");
        return -1;
    }
    if (!ctx->ops) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_dendrite_calcium: ops is NULL");
        return -1;
    }
    if (!ctx->ops->dendrite_calcium) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_dendrite_calcium: dendrite_calcium op is NULL");
        return -1;
    }
    if (!ctx->dendrite.calcium) return 0;  // No spines initialized

    nimcp_kernel_error_t err = ctx->ops->dendrite_calcium(
        ctx->gpu_ctx,
        ctx->dendrite.nmda_current,
        ctx->dendrite.vgcc_current,
        ctx->dendrite.calcium,
        ctx->dendrite.calcium_decay,
        ctx->config.dendrite.tau_calcium_ms,
        dt);

    return (err == NIMCP_KERNEL_SUCCESS) ? 0 : -1;
}

int substrate_gpu_dendrite_bap(
    substrate_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* soma_spikes,
    float dt)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_dendrite_bap: ctx is NULL");
        return -1;
    }
    if (!ctx->ops) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_dendrite_bap: ops is NULL");
        return -1;
    }
    if (!ctx->ops->dendrite_bap) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_dendrite_bap: dendrite_bap op is NULL");
        return -1;
    }

    nimcp_kernel_error_t err = ctx->ops->dendrite_bap(
        ctx->gpu_ctx,
        soma_spikes ? soma_spikes : ctx->dendrite.soma_spike,
        ctx->dendrite.bap_attenuation,
        ctx->dendrite.bap_signal,
        0.5f,  // bAP velocity (mm/ms)
        dt);

    return (err == NIMCP_KERNEL_SUCCESS) ? 0 : -1;
}

int substrate_gpu_dendrite_step(
    substrate_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* inputs,
    const nimcp_gpu_tensor_t* soma_spikes,
    float dt)
{
    if (substrate_gpu_dendrite_cable(ctx, inputs, dt) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_dendrite_step: validation failed");
        return -1;
    }
    if (substrate_gpu_dendrite_nmda(ctx) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_dendrite_step: validation failed");
        return -1;
    }
    if (substrate_gpu_dendrite_calcium(ctx, dt) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_dendrite_step: validation failed");
        return -1;
    }
    if (substrate_gpu_dendrite_bap(ctx, soma_spikes, dt) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_dendrite_step: validation failed");
        return -1;
    }
    return 0;
}

//=============================================================================
// Myelin Operations
//=============================================================================

int substrate_gpu_myelin_g_ratio(substrate_gpu_context_t* ctx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_myelin_g_ratio: ctx is NULL");
        return -1;
    }
    if (!ctx->ops) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_myelin_g_ratio: ops is NULL");
        return -1;
    }
    if (!ctx->ops->myelin_g_ratio) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_myelin_g_ratio: myelin_g_ratio op is NULL");
        return -1;
    }

    nimcp_kernel_error_t err = ctx->ops->myelin_g_ratio(
        ctx->gpu_ctx,
        ctx->myelin.axon_diameter,
        ctx->myelin.fiber_diameter,
        ctx->myelin.g_ratio,
        ctx->myelin.is_optimal);

    return (err == NIMCP_KERNEL_SUCCESS) ? 0 : -1;
}

int substrate_gpu_myelin_velocity(substrate_gpu_context_t* ctx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_myelin_velocity: ctx is NULL");
        return -1;
    }
    if (!ctx->ops) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_myelin_velocity: ops is NULL");
        return -1;
    }
    if (!ctx->ops->myelin_conduction_velocity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_myelin_velocity: myelin_conduction_velocity op is NULL");
        return -1;
    }

    nimcp_kernel_error_t err = ctx->ops->myelin_conduction_velocity(
        ctx->gpu_ctx,
        ctx->myelin.g_ratio,
        ctx->myelin.internode_length,
        ctx->myelin.temperature,
        ctx->myelin.velocity);

    return (err == NIMCP_KERNEL_SUCCESS) ? 0 : -1;
}

int substrate_gpu_myelin_plasticity(substrate_gpu_context_t* ctx, float dt)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_myelin_plasticity: ctx is NULL");
        return -1;
    }
    if (!ctx->ops) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_myelin_plasticity: ops is NULL");
        return -1;
    }
    if (!ctx->ops->myelin_plasticity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_myelin_plasticity: myelin_plasticity op is NULL");
        return -1;
    }

    nimcp_kernel_error_t err = ctx->ops->myelin_plasticity(
        ctx->gpu_ctx,
        ctx->myelin.activity,
        ctx->myelin.ol_signal,
        ctx->myelin.thickness,
        ctx->myelin.sheath_length,
        ctx->config.myelin.plasticity_rate,
        dt);

    return (err == NIMCP_KERNEL_SUCCESS) ? 0 : -1;
}

int substrate_gpu_myelin_step(substrate_gpu_context_t* ctx, float dt)
{
    if (substrate_gpu_myelin_g_ratio(ctx) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_myelin_step: validation failed");
        return -1;
    }
    if (substrate_gpu_myelin_velocity(ctx) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_myelin_step: validation failed");
        return -1;
    }
    if (substrate_gpu_myelin_plasticity(ctx, dt) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_myelin_step: validation failed");
        return -1;
    }
    return 0;
}

//=============================================================================
// Neuromodulator Operations
//=============================================================================

int substrate_gpu_neuromod_decay(substrate_gpu_context_t* ctx, float dt)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_neuromod_decay: ctx is NULL");
        return -1;
    }
    if (!ctx->ops) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_neuromod_decay: ops is NULL");
        return -1;
    }
    if (!ctx->ops->neuromod_decay) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_neuromod_decay: neuromod_decay op is NULL");
        return -1;
    }

    nimcp_kernel_error_t err = ctx->ops->neuromod_decay(
        ctx->gpu_ctx,
        ctx->neuromod.concentrations,
        ctx->neuromod.decay_rates,
        dt);

    return (err == NIMCP_KERNEL_SUCCESS) ? 0 : -1;
}

int substrate_gpu_neuromod_release(
    substrate_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* sites,
    const nimcp_gpu_tensor_t* types,
    const nimcp_gpu_tensor_t* amounts,
    uint32_t n_events)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_neuromod_release: ctx is NULL");
        return -1;
    }
    if (!ctx->ops) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_neuromod_release: ops is NULL");
        return -1;
    }
    if (!ctx->ops->neuromod_release) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_neuromod_release: neuromod_release op is NULL");
        return -1;
    }
    if (!sites || !types || !amounts || n_events == 0) return 0;

    nimcp_kernel_error_t err = ctx->ops->neuromod_release(
        ctx->gpu_ctx,
        sites,
        types,
        amounts,
        ctx->neuromod.concentrations,
        n_events);

    return (err == NIMCP_KERNEL_SUCCESS) ? 0 : -1;
}

int substrate_gpu_neuromod_effect(substrate_gpu_context_t* ctx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_neuromod_effect: ctx is NULL");
        return -1;
    }
    if (!ctx->ops) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_neuromod_effect: ops is NULL");
        return -1;
    }
    if (!ctx->ops->neuromod_effect) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_neuromod_effect: neuromod_effect op is NULL");
        return -1;
    }
    if (!ctx->neuromod.modulation) return 0;  // No synapses

    nimcp_kernel_error_t err = ctx->ops->neuromod_effect(
        ctx->gpu_ctx,
        ctx->neuromod.concentrations,
        ctx->neuromod.receptor_density,
        ctx->neuromod.modulation);

    return (err == NIMCP_KERNEL_SUCCESS) ? 0 : -1;
}

int substrate_gpu_neuromod_phasic_tonic(
    substrate_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* phasic_input,
    float dt)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_neuromod_phasic_tonic: ctx is NULL");
        return -1;
    }
    if (!ctx->ops) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_neuromod_phasic_tonic: ops is NULL");
        return -1;
    }
    if (!ctx->ops->neuromod_phasic_tonic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_neuromod_phasic_tonic: neuromod_phasic_tonic op is NULL");
        return -1;
    }

    nimcp_kernel_error_t err = ctx->ops->neuromod_phasic_tonic(
        ctx->gpu_ctx,
        phasic_input ? phasic_input : ctx->neuromod.concentrations,
        ctx->neuromod.tonic_level,
        ctx->neuromod.total_level,
        ctx->config.neuromod.tonic_tau_ms,
        ctx->config.neuromod.phasic_decay,
        dt);

    return (err == NIMCP_KERNEL_SUCCESS) ? 0 : -1;
}

int substrate_gpu_neuromod_step(substrate_gpu_context_t* ctx, float dt)
{
    if (substrate_gpu_neuromod_decay(ctx, dt) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_neuromod_step: validation failed");
        return -1;
    }
    if (substrate_gpu_neuromod_phasic_tonic(ctx, NULL, dt) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_neuromod_step: validation failed");
        return -1;
    }
    if (substrate_gpu_neuromod_effect(ctx) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_neuromod_step: validation failed");
        return -1;
    }
    return 0;
}

//=============================================================================
// Glial Operations
//=============================================================================

int substrate_gpu_astrocyte_calcium(substrate_gpu_context_t* ctx, float dt)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_astrocyte_calcium: ctx is NULL");
        return -1;
    }
    if (!ctx->ops) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_astrocyte_calcium: ops is NULL");
        return -1;
    }
    if (!ctx->ops->astrocyte_calcium_wave) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_astrocyte_calcium: astrocyte_calcium_wave op is NULL");
        return -1;
    }
    if (!ctx->glial.astro_calcium) return 0;  // No astrocytes

    nimcp_kernel_error_t err = ctx->ops->astrocyte_calcium_wave(
        ctx->gpu_ctx,
        ctx->glial.astro_ip3,
        ctx->glial.astro_gaps,
        ctx->glial.astro_calcium,
        ctx->glial.astro_wave,
        ctx->config.glial.calcium_diffusion_rate,
        dt);

    return (err == NIMCP_KERNEL_SUCCESS) ? 0 : -1;
}

int substrate_gpu_astrocyte_release(substrate_gpu_context_t* ctx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_astrocyte_release: ctx is NULL");
        return -1;
    }
    if (!ctx->ops) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_astrocyte_release: ops is NULL");
        return -1;
    }
    if (!ctx->ops->astrocyte_release) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_astrocyte_release: astrocyte_release op is NULL");
        return -1;
    }
    if (!ctx->glial.astro_calcium) return 0;

    nimcp_kernel_error_t err = ctx->ops->astrocyte_release(
        ctx->gpu_ctx,
        ctx->glial.astro_calcium,
        ctx->glial.astro_threshold,
        ctx->glial.astro_glu,
        ctx->glial.astro_atp);

    return (err == NIMCP_KERNEL_SUCCESS) ? 0 : -1;
}

int substrate_gpu_microglia_activation(substrate_gpu_context_t* ctx, float dt)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_microglia_activation: ctx is NULL");
        return -1;
    }
    if (!ctx->ops) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_microglia_activation: ops is NULL");
        return -1;
    }
    if (!ctx->ops->microglia_activation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_microglia_activation: microglia_activation op is NULL");
        return -1;
    }
    if (!ctx->glial.micro_damage) return 0;  // No microglia

    nimcp_kernel_error_t err = ctx->ops->microglia_activation(
        ctx->gpu_ctx,
        ctx->glial.micro_damage,
        ctx->glial.micro_anti,
        ctx->glial.micro_state,
        ctx->glial.micro_phago,
        ctx->config.glial.microglia_threshold,
        dt);

    return (err == NIMCP_KERNEL_SUCCESS) ? 0 : -1;
}

int substrate_gpu_opc_differentiation(substrate_gpu_context_t* ctx, float dt)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_opc_differentiation: ctx is NULL");
        return -1;
    }
    if (!ctx->ops) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_opc_differentiation: ops is NULL");
        return -1;
    }
    if (!ctx->ops->oligodendrocyte_differentiation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_opc_differentiation: oligodendrocyte_differentiation op is NULL");
        return -1;
    }
    if (!ctx->glial.opc_activity) return 0;  // No OPCs

    nimcp_kernel_error_t err = ctx->ops->oligodendrocyte_differentiation(
        ctx->gpu_ctx,
        ctx->glial.opc_activity,
        ctx->glial.opc_growth,
        ctx->glial.opc_diff_state,
        ctx->glial.opc_myelin_prod,
        dt);

    return (err == NIMCP_KERNEL_SUCCESS) ? 0 : -1;
}

int substrate_gpu_glial_step(substrate_gpu_context_t* ctx, float dt)
{
    if (substrate_gpu_astrocyte_calcium(ctx, dt) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_glial_step: validation failed");
        return -1;
    }
    if (substrate_gpu_astrocyte_release(ctx) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_glial_step: validation failed");
        return -1;
    }
    if (substrate_gpu_microglia_activation(ctx, dt) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_glial_step: validation failed");
        return -1;
    }
    if (substrate_gpu_opc_differentiation(ctx, dt) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_glial_step: validation failed");
        return -1;
    }
    return 0;
}

//=============================================================================
// Metabolic Operations
//=============================================================================

int substrate_gpu_metabolic_effects(substrate_gpu_context_t* ctx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_metabolic_effects: ctx is NULL");
        return -1;
    }
    if (!ctx->ops) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_metabolic_effects: ops is NULL");
        return -1;
    }
    if (!ctx->ops->metabolic_effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_metabolic_effects: metabolic_effects op is NULL");
        return -1;
    }
    if (!ctx->metabolic.atp_levels) return 0;

    nimcp_kernel_error_t err = ctx->ops->metabolic_effects(
        ctx->gpu_ctx,
        ctx->metabolic.atp_levels,
        ctx->metabolic.oxygen_levels,
        ctx->metabolic.glucose_levels,
        ctx->metabolic.capacity,
        ctx->metabolic.fatigue);

    return (err == NIMCP_KERNEL_SUCCESS) ? 0 : -1;
}

int substrate_gpu_metabolic_update(
    substrate_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* neural_activity,
    float dt)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_metabolic_update: ctx is NULL");
        return -1;
    }
    if (!ctx->ops) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_metabolic_update: ops is NULL");
        return -1;
    }
    if (!ctx->ops->metabolic_update) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_gpu_metabolic_update: metabolic_update op is NULL");
        return -1;
    }
    if (!ctx->metabolic.atp_levels) return 0;

    nimcp_kernel_error_t err = ctx->ops->metabolic_update(
        ctx->gpu_ctx,
        neural_activity ? neural_activity : ctx->metabolic.neural_activity,
        ctx->metabolic.atp_levels,
        ctx->metabolic.lactate_levels,
        ctx->config.metabolic.atp_consumption_rate,
        ctx->config.metabolic.atp_recovery_rate,
        dt);

    return (err == NIMCP_KERNEL_SUCCESS) ? 0 : -1;
}

int substrate_gpu_metabolic_step(
    substrate_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* neural_activity,
    float dt)
{
    if (substrate_gpu_metabolic_effects(ctx) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_metabolic_step: validation failed");
        return -1;
    }
    if (substrate_gpu_metabolic_update(ctx, neural_activity, dt) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_metabolic_step: validation failed");
        return -1;
    }
    return 0;
}

//=============================================================================
// Unified Step
//=============================================================================

int substrate_gpu_full_step(
    substrate_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input_spikes,
    const nimcp_gpu_tensor_t* synaptic_inputs,
    const nimcp_gpu_tensor_t* neural_activity,
    float dt)
{
    if (!ctx) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;

    }

    // Process each subsystem in order
    if (ctx->axon.n_axons > 0) {
        if (substrate_gpu_axon_step(ctx, input_spikes, dt) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_full_step: validation failed");
            return -1;
        }
    }

    if (ctx->dendrite.n_dendrites > 0) {
        if (substrate_gpu_dendrite_step(ctx, synaptic_inputs, NULL, dt) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_full_step: validation failed");
            return -1;
        }
    }

    if (ctx->myelin.n_axons > 0) {
        if (substrate_gpu_myelin_step(ctx, dt) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_full_step: validation failed");
            return -1;
        }
    }

    if (ctx->neuromod.n_pools > 0) {
        if (substrate_gpu_neuromod_step(ctx, dt) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_full_step: validation failed");
            return -1;
        }
    }

    if (ctx->glial.n_astrocytes > 0 || ctx->glial.n_microglia > 0 || ctx->glial.n_opcs > 0) {
        if (substrate_gpu_glial_step(ctx, dt) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_full_step: validation failed");
            return -1;
        }
    }

    if (ctx->metabolic.n_regions > 0) {
        if (substrate_gpu_metabolic_step(ctx, neural_activity, dt) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_gpu_full_step: validation failed");
            return -1;
        }
    }

    return 0;
}

//=============================================================================
// Tensor Accessors
//=============================================================================

substrate_axon_tensors_t* substrate_gpu_get_axon_tensors(substrate_gpu_context_t* ctx)
{
    return ctx ? &ctx->axon : NULL;
}

substrate_dendrite_tensors_t* substrate_gpu_get_dendrite_tensors(substrate_gpu_context_t* ctx)
{
    return ctx ? &ctx->dendrite : NULL;
}

substrate_myelin_tensors_t* substrate_gpu_get_myelin_tensors(substrate_gpu_context_t* ctx)
{
    return ctx ? &ctx->myelin : NULL;
}

substrate_neuromod_tensors_t* substrate_gpu_get_neuromod_tensors(substrate_gpu_context_t* ctx)
{
    return ctx ? &ctx->neuromod : NULL;
}

substrate_glial_tensors_t* substrate_gpu_get_glial_tensors(substrate_gpu_context_t* ctx)
{
    return ctx ? &ctx->glial : NULL;
}

substrate_metabolic_tensors_t* substrate_gpu_get_metabolic_tensors(substrate_gpu_context_t* ctx)
{
    return ctx ? &ctx->metabolic : NULL;
}
