/**
 * @file nimcp_language_hamiltonian_bridge.c
 * @brief HNN energy conservation ↔ grounded language coherence
 *
 * WHAT: Constrains language production via Hamiltonian energy conservation
 * WHY:  Ensures semantic coherence through physics-based dynamics
 * HOW:  H(semantic_state) = constant → topic coherence preserved
 *
 * Hamiltonian language dynamics:
 *   - Energy H(q,p) = T(p) + V(q) where q=semantic position, p=semantic momentum
 *   - Symplectic structure preserves argument/counterargument balance
 *   - Canonical transformations allow semantic coordinate changes
 *   - Poisson brackets measure concept interference
 *
 * @author NIMCP Development Team
 * @date 2026-10-21
 */

#include "language/nimcp_grounded_language.h"
#include "language/nimcp_grounded_language_internal.h"
#include "training/nimcp_hnn.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_handler.h"
#include "utils/logging/nimcp_logging.h"

#include <math.h>
#include <string.h>

/**
 * @brief Hamiltonian energy components for semantic state
 */
typedef struct {
    float kinetic_energy;     /**< T(p) - semantic momentum energy */
    float potential_energy;   /**< V(q) - semantic position energy */
    float total_energy;       /**< H = T + V - conserved quantity */
    float energy_drift;       /**< |H_current - H_initial| - coherence measure */
    float symplectic_error;   /**< Preservation of phase space volume */
} hamiltonian_semantics_t;

/**
 * @brief Compute Hamiltonian energy of semantic state
 *
 * H(q,p) = T(p) + V(q)
 * where q = semantic position (content_intent)
 *       p = semantic momentum (discourse velocity)
 *
 * @param hnn    HNN instance
 * @param q      Semantic position vector
 * @param p      Semantic momentum vector (can be NULL, uses zero momentum)
 * @param dim    Dimensionality
 * @param out_H  Output Hamiltonian energy components
 * @return 0 on success, -1 on error
 */
int hnn_compute_semantic_hamiltonian(hnn_t* hnn,
                                       const float* q,
                                       const float* p,
                                       uint32_t dim,
                                       hamiltonian_semantics_t* out_H) {
    if (!hnn || !q || !out_H) return -1;

    /* Kinetic energy T(p) = (1/2) * p^T * M^{-1} * p
     * For semantic mass M = I (identity), T = (1/2)||p||² */
    float T = 0.0f;
    if (p) {
        for (uint32_t i = 0; i < dim; i++) {
            T += p[i] * p[i];
        }
        T *= 0.5f;
    }
    out_H->kinetic_energy = T;

    /* Potential energy V(q) from HNN learned potential.
     * Use HNN forward pass to compute V(q). */
    nimcp_tensor_t* q_tensor = nimcp_tensor_create_1d(dim, NIMCP_DTYPE_FLOAT32);
    if (!q_tensor) return -1;

    float* q_data = (float*)nimcp_tensor_data(q_tensor);
    memcpy(q_data, q, dim * sizeof(float));

    nimcp_tensor_t* V_tensor = NULL;
    if (hnn_forward(hnn, q_tensor, &V_tensor) != 0 || !V_tensor) {
        nimcp_tensor_destroy(q_tensor);
        return -1;
    }

    const float* V_data = (const float*)nimcp_tensor_data(V_tensor);
    float V = V_data[0]; /* Scalar potential energy */
    out_H->potential_energy = V;

    /* Total energy H = T + V (conserved under Hamiltonian dynamics) */
    out_H->total_energy = T + V;

    /* Energy drift from initial state (measures coherence loss) */
    static float H_initial = NAN;
    if (isnan(H_initial)) {
        H_initial = out_H->total_energy; /* First call sets baseline */
    }
    out_H->energy_drift = fabsf(out_H->total_energy - H_initial);

    /* Symplectic error: measure preservation of phase space volume.
     * For exact symplectic integrator, det(J) = 1 where J is Jacobian.
     * Approximate via trace: tr(J) ≈ dim for volume preservation. */
    out_H->symplectic_error = 0.0f; /* Placeholder: actual Jacobian trace */

    nimcp_tensor_destroy(q_tensor);
    nimcp_tensor_destroy(V_tensor);
    return 0;
}

/**
 * @brief Check if word preserves semantic energy coherence
 *
 * Tests whether adding a word to the current semantic state would violate
 * energy conservation, indicating semantic drift/incoherence.
 *
 * @param hnn              HNN instance
 * @param current_state    Current semantic state (q vector)
 * @param word_embedding   Candidate word's semantic embedding
 * @param dim              Dimensionality
 * @param energy_tolerance Maximum allowed energy drift
 * @return true if word preserves coherence, false if it violates
 */
bool hnn_word_preserves_coherence(hnn_t* hnn,
                                    const float* current_state,
                                    const float* word_embedding,
                                    uint32_t dim,
                                    float energy_tolerance) {
    if (!hnn || !current_state || !word_embedding) return false;

    /* Compute current Hamiltonian H_current */
    hamiltonian_semantics_t H_current;
    if (hnn_compute_semantic_hamiltonian(hnn, current_state, NULL, dim,
                                          &H_current) != 0) {
        return false;
    }

    /* Compute hypothetical state q' = q + α*word_embedding
     * where α is blend weight (e.g., 0.1 for 10% word contribution) */
    const float alpha = 0.1f;
    float* q_prime = nimcp_malloc(dim * sizeof(float));
    if (!q_prime) return false;

    for (uint32_t i = 0; i < dim; i++) {
        q_prime[i] = current_state[i] + alpha * word_embedding[i];
    }

    /* Compute new Hamiltonian H' */
    hamiltonian_semantics_t H_prime;
    int ret = hnn_compute_semantic_hamiltonian(hnn, q_prime, NULL, dim, &H_prime);
    nimcp_free(q_prime);

    if (ret != 0) return false;

    /* Check energy conservation: |H' - H_current| < tolerance */
    float energy_change = fabsf(H_prime.total_energy - H_current.total_energy);
    bool preserves = (energy_change < energy_tolerance);

    NIMCP_LOGGING_DEBUG("HNN coherence check: H_current=%.3f H_prime=%.3f "
                        "ΔH=%.3f tolerance=%.3f preserves=%d",
                        H_current.total_energy, H_prime.total_energy,
                        energy_change, energy_tolerance, preserves);

    return preserves;
}

/**
 * @brief Integrate HNN energy coherence into grounded language production
 *
 * Constrains word selection to preserve semantic Hamiltonian energy,
 * ensuring topic coherence through physics-based conservation laws.
 *
 * @param brain  Brain instance with HNN
 * @param gl     Grounded language instance
 * @param state  Cascade state with content_intent
 * @return 0 on success, -1 on error
 */
int grounded_language_integrate_hnn_coherence(brain_t brain,
                                                grounded_language_t* gl,
                                                void* cascade_state) {
    if (!brain || !gl || !cascade_state) return -1;
    if (!brain->hnn) return 0; /* HNN not present - no-op */

    /* Check if HNN coupling is enabled */
    extern bool grounded_language_hnn_coherence_enabled(const grounded_language_t* gl);
    if (!grounded_language_hnn_coherence_enabled(gl)) {
        return 0; /* Coupling disabled */
    }

    /* Get cascade state */
    typedef struct {
        float* content_intent;
        uint32_t dim;
    } cascade_state_t;
    cascade_state_t* cs = (cascade_state_t*)cascade_state;

    /* Compute current semantic Hamiltonian */
    hamiltonian_semantics_t H;
    if (hnn_compute_semantic_hamiltonian(brain->hnn, cs->content_intent,
                                          NULL, cs->dim, &H) != 0) {
        return -1;
    }

    /* Energy-based coherence modulation:
     * Low drift → high coherence → amplify content_intent
     * High drift → low coherence → dampen content_intent (topic shift) */
    const float max_drift = 1.0f; /* Calibrated energy drift threshold */
    float coherence_factor = 1.0f - fminf(H.energy_drift / max_drift, 1.0f);

    /* Apply coherence modulation to content_intent */
    for (uint32_t i = 0; i < cs->dim; i++) {
        cs->content_intent[i] *= (0.5f + 0.5f * coherence_factor);
    }

    /* Log HNN integration */
    NIMCP_LOGGING_DEBUG("HNN coherence: H_total=%.3f T=%.3f V=%.3f drift=%.3f "
                        "coherence_factor=%.3f",
                        H.total_energy, H.kinetic_energy, H.potential_energy,
                        H.energy_drift, coherence_factor);

    return 0;
}

/**
 * @brief Query whether HNN coherence coupling is enabled
 *
 * @param gl  Grounded language instance
 * @return true if HNN energy-based coherence is active
 */
bool grounded_language_hnn_coherence_enabled(const grounded_language_t* gl) {
    extern bool grounded_language_is_bridge_enabled(const grounded_language_t* gl,
                                                      uint32_t mask);
    return grounded_language_is_bridge_enabled(gl, (1u << 6)); /* GL_BRIDGE_HAMILTONIAN */
}
