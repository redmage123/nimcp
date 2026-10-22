/**
 * @file nimcp_gpu_stubs_snn_minimal.c
 * @brief Minimal stubs for missing GPU SNN plasticity functions
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Minimal stubs that just return error/void - actual types/params don't matter
// These only need to link, not function

// BCM plasticity stubs
void nimcp_gpu_bcm_params_default(void) {}
bool nimcp_gpu_bcm_update_threshold(void* a, void* b, void* c, void* d, float e) { (void)a;(void)b;(void)c;(void)d;(void)e; return false; }
bool nimcp_gpu_bcm_weight_update_csr(void* a, void* b, void* c, void* d, void* e, void* f) { (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return false; }

// Homeostatic scaling stubs
void nimcp_gpu_scaling_params_default(void) {}
bool nimcp_gpu_homeostatic_compute_scaling(void* a, void* b, void* c, void* d) { (void)a;(void)b;(void)c;(void)d; return false; }
bool nimcp_gpu_homeostatic_apply_scaling(void* a, void* b, void* c) { (void)a;(void)b;(void)c; return false; }
bool nimcp_gpu_homeostatic_scale_csr(void* a, void* b, void* c) { (void)a;(void)b;(void)c; return false; }
bool nimcp_gpu_homeostatic_update_rates(void* a, void* b, void* c, void* d, float e) { (void)a;(void)b;(void)c;(void)d;(void)e; return false; }

// Intrinsic plasticity stubs
void nimcp_gpu_intrinsic_params_default(void) {}

// STDP stubs
void nimcp_gpu_stdp_params_default(void) {}
bool nimcp_gpu_stdp_apply(void* a, void* b, void* c, void* d, void* e) { (void)a;(void)b;(void)c;(void)d;(void)e; return false; }
bool nimcp_gpu_stdp_update_traces(void* a, void* b, void* c, float d, float e) { (void)a;(void)b;(void)c;(void)d;(void)e; return false; }
bool nimcp_gpu_stdp_weight_update_csr(void* a, void* b, void* c, void* d, void* e) { (void)a;(void)b;(void)c;(void)d;(void)e; return false; }

// LIF conductance-based stubs
bool nimcp_gpu_lif_forward_cb(void* a, void* b, float c, float d, float e, float f, float g, float h, float i, float j, float k) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h;(void)i;(void)j;(void)k; return false;
}
bool nimcp_gpu_lif_state_alloc_cb_arrays(void* a, void* b, size_t c) { (void)a;(void)b;(void)c; return false; }
bool nimcp_gpu_lif_state_download_g(void* a, void* b, void* c, void* d, void* e, void* f, size_t g) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g; return false;
}
bool nimcp_gpu_lif_state_upload_g(void* a, void* b, void* c, void* d, void* e, void* f, size_t g) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g; return false;
}
bool nimcp_gpu_lif_state_upload_per_neuron_params(void* a, void* b, void* c, void* d, void* e, void* f, void* g, void* h, size_t i) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h;(void)i; return false;
}

// SNN conductance-based deposit
bool nimcp_gpu_snn_cb_deposit_csr(
    void* ctx,
    const float* d_spike_vector,
    const float* d_depression,
    const float* d_weights,
    const uint32_t* d_flat_col_idx,
    const uint32_t* d_src_pop_idx,
    const uint32_t* d_row_ptr,
    const uint8_t* d_synapse_type,
    float* d_g_ampa,
    float* d_g_nmda,
    float* d_g_gaba_a,
    float* d_g_gaba_b,
    uint32_t dst_pop_offset,
    uint32_t n_neurons_pop)
{
    (void)ctx;(void)d_spike_vector;(void)d_depression;(void)d_weights;
    (void)d_flat_col_idx;(void)d_src_pop_idx;(void)d_row_ptr;(void)d_synapse_type;
    (void)d_g_ampa;(void)d_g_nmda;(void)d_g_gaba_a;(void)d_g_gaba_b;
    (void)dst_pop_offset;(void)n_neurons_pop;
    return false;
}

// SNN current-based isyn
bool nimcp_gpu_snn_isyn_csr(
    void* ctx,
    const float* d_spike_vector,
    const float* d_weights,
    const unsigned int* d_col_indices,
    const unsigned int* d_row_ptr,
    const float* d_external_current,
    float* d_output_isyn,
    size_t n_neurons)
{
    (void)ctx;(void)d_spike_vector;(void)d_weights;(void)d_col_indices;
    (void)d_row_ptr;(void)d_external_current;(void)d_output_isyn;(void)n_neurons;
    return false;
}
