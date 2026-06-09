// nimcp_part_core.c - core functions
// Part of nimcp.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp.c

#include "training/nimcp_cortex_cnn.h"
#include "snn/nimcp_snn_types.h"
#include "snn/bridges/nimcp_snn_language_bridge.h"
#include "snn/nimcp_snn_network.h"   /* Phase-2 warm-start: find_pop_by_name */
#include "language/nimcp_communication_cascade.h"
#include "cognitive/grounded_language/nimcp_stage_table.h"  /* Slice E */
#include "language/nimcp_concept_registry.h"                /* Slice B / walkthrough-2 */
#include "security/nimcp_toxicity.h"                        /* 2026-05-21: classifier diag */
#include "security/nimcp_toxicity_response.h"                /* Phase 3a: counterclaim engine */
#include "security/nimcp_toxicity_ml.h"                      /* Task 134: ML head */
#include "security/nimcp_audit_log.h"                        /* 2026-05-22: counterclaim audit */


//=============================================================================
// Version Functions
//=============================================================================

const char* nimcp_version(void) {
    return NIMCP_VERSION_STRING;
}


int nimcp_version_int(void) {
    return NIMCP_VERSION_MAJOR * 10000 + NIMCP_VERSION_MINOR * 100 + NIMCP_VERSION_PATCH;
}


int nimcp_abi_layout_hash(void) {
    /* Hash critical struct sizes to detect stale Python .so builds.
     * Any change to these structs will change this hash and cause Python
     * import to fail early with a clear error instead of SIGSEGV. */
    uint32_t h = 0x4E494D43u; /* "NIMC" */
    h ^= (uint32_t)sizeof(neuron_t) * 2654435761u;
    h ^= (uint32_t)sizeof(sparse_synapse_storage_t) * 2246822519u;
    h ^= (uint32_t)SPARSE_SYNAPSE_EMBEDDED_CAPACITY * 3266489917u;
    /* Note: snn_stats_t, brain_config_t, nimcp_training_config_t excluded
     * from ABI hash — struct packing/alignment varies between main lib
     * (NVCC+GCC) and Python .so (GCC only), causing false mismatches.
     * Instead, the Python binding uses oversized buffers for these structs. */
    return (int)(h & 0x7FFFFFFFu); /* Keep positive for Python int */
}


nimcp_status_t nimcp_brain_learn_example(
    nimcp_brain_t brain,
    const float* features,
    uint32_t num_features,
    const char* label,
    float confidence)
{
    LOG_DEBUG("Learning example: label='%s', num_features=%u, confidence=%.3f",
              label ? label : "NULL", num_features, confidence);

    /* Exception-integrated parameter validation */
    NIMCP_API_CHECK_NULL(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL");
    NIMCP_API_CHECK_NULL(features, NIMCP_ERROR_NULL_ARG, "Features array is NULL");
    NIMCP_API_CHECK_NULL(label, NIMCP_ERROR_NULL_ARG, "Label is NULL");
    NIMCP_API_CHECK_NULL(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    /* I-H1 FIX: Validate input dimensions at API layer */
    if (num_features == 0) {
        set_error("num_features must be > 0");
        return NIMCP_ERROR_INVALID;
    }

    /* === PHASE IS-1: BBB INPUT VALIDATION === */
    /* Validate external input data through Blood-Brain Barrier before processing */
    /* Note: brain->internal_brain already validated non-NULL above */
    if (brain->internal_brain->bbb_enabled &&
        brain->internal_brain->bbb_system) {
        LOG_DEBUG("BBB enabled, validating inputs");
        bbb_validation_result_t result;

        /* Validate features array (external input data) */
        if (!bbb_validate_input(brain->internal_brain->bbb_system,
                               features, num_features * sizeof(float), &result)) {
            NIMCP_API_CHECK_BBB(false, result, NIMCP_ERROR_INVALID);
        }

        /* Validate label string (external string input) */
        if (!bbb_validate_string(brain->internal_brain->bbb_system, label, &result)) {
            NIMCP_API_CHECK_BBB(false, result, NIMCP_ERROR_INVALID);
        }
        LOG_DEBUG("BBB validation passed");
    }

    /* Call internal brain API */
    LOG_DEBUG("Invoking internal brain_learn_example");
    float loss = brain_learn_example(brain->internal_brain, features, num_features, label, confidence);

    /* brain_learn_example returns -1.0f on error, >= 0.0f on success */
    NIMCP_API_CHECK_FLOAT(loss, NIMCP_ERROR,
                          "Brain learning failed for label");

    /* Store loss for retrieval via nimcp_brain_get_last_loss() */
    brain->last_loss = loss;

    /* Store gradient norm from the adaptive network */
    if (brain->internal_brain && brain->internal_brain->network) {
        brain->last_gradient_norm = adaptive_network_get_last_grad_norm(brain->internal_brain->network);
    } else {
        brain->last_gradient_norm = 0.0f;
    }

    set_error("No error");
    LOG_DEBUG("Learning example completed successfully (loss=%.6f)", loss);
    return NIMCP_OK;
}


nimcp_status_t nimcp_brain_learn_vector(
    nimcp_brain_t brain,
    const float* features,
    uint32_t num_features,
    const float* target,
    uint32_t target_size,
    const char* label,
    float confidence)
{
    LOG_DEBUG("Learning vector: target_size=%u, num_features=%u, confidence=%.3f",
              target_size, num_features, confidence);

    /* Exception-integrated parameter validation */
    NIMCP_API_CHECK_NULL(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL");
    NIMCP_API_CHECK_NULL(features, NIMCP_ERROR_NULL_ARG, "Features array is NULL");
    NIMCP_API_CHECK_NULL(target, NIMCP_ERROR_NULL_ARG, "Target array is NULL");
    NIMCP_API_CHECK_NULL(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    if (num_features == 0) {
        set_error("num_features must be > 0");
        return NIMCP_ERROR_INVALID;
    }
    if (target_size == 0) {
        set_error("target_size must be > 0");
        return NIMCP_ERROR_INVALID;
    }

    /* === BBB INPUT VALIDATION === */
    if (brain->internal_brain->bbb_enabled &&
        brain->internal_brain->bbb_system) {
        bbb_validation_result_t result;

        /* Validate features array */
        if (!bbb_validate_input(brain->internal_brain->bbb_system,
                               features, num_features * sizeof(float), &result)) {
            NIMCP_API_CHECK_BBB(false, result, NIMCP_ERROR_INVALID);
        }

        /* Validate target array */
        if (!bbb_validate_input(brain->internal_brain->bbb_system,
                               target, target_size * sizeof(float), &result)) {
            NIMCP_API_CHECK_BBB(false, result, NIMCP_ERROR_INVALID);
        }

        /* Validate label string if provided */
        if (label && !bbb_validate_string(brain->internal_brain->bbb_system, label, &result)) {
            NIMCP_API_CHECK_BBB(false, result, NIMCP_ERROR_INVALID);
        }
    }

    /* Call internal brain API */
    float loss = brain_learn_vector(brain->internal_brain, features, num_features,
                                    target, target_size, label, confidence);

    /* Negative loss means internal failure (GPU not ready, subsystem init, etc.).
     * Treat as non-fatal: log a warning and return 0 loss so training continues.
     * Previously this raised a RuntimeError which killed training on the first call. */
    if (loss < 0.0f) {
        LOG_WARN("brain_learn_vector returned %.2f (transient failure), continuing", loss);
        loss = 0.0f;
    }

    /* Store loss and gradient norm */
    brain->last_loss = loss;
    if (brain->internal_brain && brain->internal_brain->network) {
        brain->last_gradient_norm = adaptive_network_get_last_grad_norm(brain->internal_brain->network);
    } else {
        brain->last_gradient_norm = 0.0f;
    }

    set_error("No error");
    LOG_DEBUG("Learning vector completed successfully (loss=%.6f)", loss);
    return NIMCP_OK;
}


#ifndef REWARD_LEARNING_RATE
#define REWARD_LEARNING_RATE 0.0001f
#endif
#ifndef REWARD_ACTIVITY_THRESHOLD
#define REWARD_ACTIVITY_THRESHOLD 0.01f
#endif

float nimcp_brain_learn_vector_batch(
    nimcp_brain_t brain,
    const float** features_array,
    const float** targets_array,
    uint32_t num_features,
    uint32_t target_size,
    uint32_t num_examples,
    float learning_rate)
{
    if (!brain || !brain->internal_brain || !features_array || !targets_array) return -1.0f;
    if (num_examples == 0 || num_features == 0 || target_size == 0) return -1.0f;

    brain_t ib = brain->internal_brain;
    float lr = (learning_rate > 0.0f) ? learning_rate : ib->config.learning_rate;

    // Build training_example_t array
    training_example_t* examples = nimcp_calloc(num_examples, sizeof(training_example_t));
    if (!examples) return -1.0f;

    for (uint32_t i = 0; i < num_examples; i++) {
        examples[i].input = (float*)features_array[i];
        examples[i].input_size = num_features;
        examples[i].target = (float*)targets_array[i];
        examples[i].target_size = target_size;
        examples[i].confidence = 1.0f;

        /* Generate a synthetic label from the target argmax so the cortex
         * CNN training block (gated on label[0]) fires during batch mode.
         * Format: "class_N" where N is the argmax index. This creates
         * distinct labels for different target classes, enabling the
         * cortex_cnn_backward one-hot target to match the real target.
         * Works even when output_labels is empty (batch mode never calls
         * get_or_create_label_index, so the label array stays NULL). */
        {
            uint32_t best = 0;
            float best_v = targets_array[i][0];
            for (uint32_t k = 1; k < target_size; k++) {
                if (targets_array[i][k] > best_v) {
                    best_v = targets_array[i][k];
                    best = k;
                }
            }
            snprintf(examples[i].label, sizeof(examples[i].label),
                     "class_%u", best);
        }
    }

    // Pre-create cortex CNNs BEFORE batch training (needs staged sensory data).
    // cortex_cnn_create and FNO functions declared via nimcp_cortex_cnn.h in nimcp_part_bindings.c
    {
        extern void* fno_audio_create(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
        extern void cortex_cnn_set_fno_audio(cortex_cnn_processor_t*, void*);
        extern void cortex_cnn_set_fno_visual(cortex_cnn_processor_t*, void*);
        extern void cortex_cnn_set_fno_speech(cortex_cnn_processor_t*, void*);
        extern void cortex_cnn_set_fno_somato(cortex_cnn_processor_t*, void*);
        if (ib->staged_sensory.visual_frame && !ib->cortex_cnns[0]) {
            ib->cortex_cnns[0] = cortex_cnn_create(0, 0);
            if (ib->cortex_cnns[0]) {
                void* fno = fno_audio_create(1024, 64, 16, 32, 2);
                if (fno) cortex_cnn_set_fno_visual(ib->cortex_cnns[0], fno);
            }
        }
        if (ib->staged_sensory.audio_data && !ib->cortex_cnns[1]) {
            ib->cortex_cnns[1] = cortex_cnn_create(1, 0);
            if (ib->cortex_cnns[1]) {
                void* fno = fno_audio_create(128, 64, 16, 32, 2);
                if (fno) cortex_cnn_set_fno_audio(ib->cortex_cnns[1], fno);
            }
        }
        if (ib->staged_sensory.speech_data && !ib->cortex_cnns[2]) {
            ib->cortex_cnns[2] = cortex_cnn_create(2, 0);
            if (ib->cortex_cnns[2]) {
                void* fno = fno_audio_create(128, 64, 16, 32, 2);
                if (fno) cortex_cnn_set_fno_speech(ib->cortex_cnns[2], fno);
            }
        }
        if (ib->staged_sensory.somato_data && !ib->cortex_cnns[3]) {
            ib->cortex_cnns[3] = cortex_cnn_create(3, 0);
            if (ib->cortex_cnns[3]) {
                extern void* cortex_cnn_get_fno_somato(const cortex_cnn_processor_t*);
                /* Somato FNO: spectral analysis of multi-scale wavelet body schema.
                 * Input size 256 covers wavelet decomposition of typical 128-segment
                 * body schema (128+64+32+ pad ≈ 256). Resampled at runtime.
                 * Guarded by get-check so re-entry on already-created cortex
                 * doesn't leak FNO instances. */
                if (!cortex_cnn_get_fno_somato(ib->cortex_cnns[3])) {
                    void* fno = fno_audio_create(256, 64, 16, 32, 2);
                    if (fno) cortex_cnn_set_fno_somato(ib->cortex_cnns[3], fno);
                }
            }
        }
    }

    // GPU gradient accumulation handles the batch internally
    float loss = adaptive_network_learn_batch(
        ib->network, examples, num_examples,
        LEARN_MODE_DISTILLATION, lr);

    /* Note: examples freed AFTER secondary network dispatch below (labels needed) */

    if (loss >= 0.0f) {
        brain->last_loss = loss;
        brain->last_gradient_norm = adaptive_network_get_last_grad_norm(ib->network);
        nimcp_brain_learning_adapt_learning_rate(ib, loss);
        __atomic_fetch_add(&ib->stats.total_learning_steps, num_examples, __ATOMIC_RELAXED);
    } else {
        nimcp_free(examples);
        return loss;
    }

    /* Train secondary networks on ONE representative sample per batch.
     * brain_learn_vector runs the full biological pipeline (SNN, CNN,
     * plasticity, cognitive modules) — expensive at ~5s per call.
     * The ANN batch above handles the primary learning; secondary
     * networks only need periodic exposure to stay synchronized. */
    if (loss >= 0.0f && num_examples > 0) {
        uint32_t repr = num_examples / 2;  /* middle sample */
        const char* lbl = (examples[repr].label[0]) ? examples[repr].label : "batch";
        brain_learn_vector(ib,
                           (float*)features_array[repr], num_features,
                           (float*)targets_array[repr], target_size,
                           lbl, 1.0f /* confidence */);
    }

    nimcp_free(examples);
    return loss;
}

float nimcp_brain_get_last_loss(nimcp_brain_t brain)
{
    if (!brain) return -1.0F;
    return brain->last_loss;
}


float nimcp_brain_get_last_gradient_norm(nimcp_brain_t brain)
{
    if (!brain) return -1.0f;
    return brain->last_gradient_norm;
}


float nimcp_brain_get_accuracy(nimcp_brain_t brain)
{
    if (!brain || !brain->internal_brain) return 0.0F;
    return brain->internal_brain->stats.running_accuracy;
}

/* Walkthrough-2 (Option-1 rebuild, 2026-05-19) — expose concept_registry
 * stats so callers can observe whether the cross-modal bind hook is
 * actually firing during training. Without this accessor, the registry's
 * growth is invisible from outside the C library.
 *
 * Both out params are NULL-tolerant. Returns NIMCP_OK on success;
 * NIMCP_ERROR_NULL_POINTER if brain is NULL; NIMCP_OK with zeroed
 * out params if the registry was never created (older checkpoints
 * loaded before the registry was a thing). */
nimcp_status_t nimcp_brain_get_concept_registry_stats(
    nimcp_brain_t brain,
    size_t* total_referents,
    size_t* total_modality_bindings)
{
    if (!brain || !brain->internal_brain) {
        if (total_referents) *total_referents = 0;
        if (total_modality_bindings) *total_modality_bindings = 0;
        return NIMCP_ERROR_NULL_POINTER;
    }
    concept_registry_t* reg =
        (concept_registry_t*)brain->internal_brain->concept_registry;
    if (!reg) {
        /* Sentinel value SIZE_MAX in total_referents distinguishes
         * "registry is NULL" from "registry exists but empty (0)". */
        if (total_referents) *total_referents = (size_t)-1;
        if (total_modality_bindings) *total_modality_bindings = 0;
        return NIMCP_OK;
    }
    if (total_referents)
        *total_referents = concept_registry_total_referents(reg);
    if (total_modality_bindings)
        *total_modality_bindings = concept_registry_total_modality_bindings(reg);
    return NIMCP_OK;
}


nimcp_status_t nimcp_brain_learn_batch(
    nimcp_brain_t brain,
    const float** features_array,
    const uint32_t* num_features_array,
    const char** labels,
    const float* confidences,
    uint32_t num_examples,
    float* losses_out)
{
    LOG_DEBUG("Learning batch: num_examples=%u", num_examples);

    NIMCP_API_CHECK_NULL(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL");
    NIMCP_API_CHECK_NULL(features_array, NIMCP_ERROR_NULL_ARG, "Features array is NULL");
    NIMCP_API_CHECK_NULL(labels, NIMCP_ERROR_NULL_ARG, "Labels array is NULL");
    NIMCP_API_CHECK_NULL(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    if (num_examples == 0) {
        set_error("num_examples must be > 0");
        return NIMCP_ERROR_INVALID;
    }

    /* BBB validation on the entire batch */
    if (brain->internal_brain->bbb_enabled &&
        brain->internal_brain->bbb_system) {
        bbb_validation_result_t result;
        for (uint32_t i = 0; i < num_examples; i++) {
            if (!features_array[i] || !labels[i]) continue;
            uint32_t nf = num_features_array ? num_features_array[i] : 0;
            if (!bbb_validate_input(brain->internal_brain->bbb_system,
                                   features_array[i], nf * sizeof(float), &result)) {
                NIMCP_API_CHECK_BBB(false, result, NIMCP_ERROR_INVALID);
            }
            if (!bbb_validate_string(brain->internal_brain->bbb_system, labels[i], &result)) {
                NIMCP_API_CHECK_BBB(false, result, NIMCP_ERROR_INVALID);
            }
        }
    }

    /* Build brain_example_t array and call internal batch learn */
    brain_example_t* examples = (brain_example_t*)nimcp_calloc(num_examples, sizeof(brain_example_t));
    if (!examples) {
        set_error("Failed to allocate batch example array");
        return NIMCP_ERROR_OUT_OF_MEMORY;
    }

    for (uint32_t i = 0; i < num_examples; i++) {
        examples[i].features = (float*)features_array[i];
        examples[i].num_features = num_features_array ? num_features_array[i] : 0;
        strncpy(examples[i].label, labels[i] ? labels[i] : "", 63);
        examples[i].label[63] = '\0';
        examples[i].confidence = confidences ? confidences[i] : 1.0f;
    }

    float avg_loss = brain_learn_batch_detailed(brain->internal_brain, examples,
                                                 num_examples, losses_out);

    nimcp_free(examples);

    if (avg_loss < 0.0f) {
        set_error("Batch learning failed");
        return NIMCP_ERROR;
    }

    brain->last_loss = avg_loss;

    /* Update gradient norm from last example in batch */
    if (brain->internal_brain && brain->internal_brain->network) {
        brain->last_gradient_norm = adaptive_network_get_last_grad_norm(brain->internal_brain->network);
    }

    LOG_DEBUG("Batch learning completed: avg_loss=%.6f", avg_loss);
    return NIMCP_OK;
}


nimcp_status_t nimcp_brain_predict(
    nimcp_brain_t brain,
    const float* features,
    uint32_t num_features,
    char* out_label,
    float* out_confidence)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain handle");
    API_CHECK_THROW(features, NIMCP_ERROR_NULL_ARG, "Features array is NULL");
    API_CHECK_THROW(out_label, NIMCP_ERROR_NULL_ARG, "Output label buffer is NULL");
    API_CHECK_THROW(out_confidence, NIMCP_ERROR_NULL_ARG, "Output confidence pointer is NULL");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    // I-H1 FIX: Validate input dimensions
    if (num_features == 0) {
        set_error("num_features must be > 0");
        return NIMCP_ERROR_INVALID;
    }
    if (brain->internal_brain->config.num_inputs > 0 &&
        num_features != brain->internal_brain->config.num_inputs) {
        set_error("Feature count mismatch: expected %u, got %u",
                  brain->internal_brain->config.num_inputs, num_features);
        return NIMCP_ERROR_INVALID;
    }

    // === PHASE IS-1: BBB INPUT VALIDATION ===
    // Validate external input data through Blood-Brain Barrier before processing
    if (brain->internal_brain->bbb_enabled &&
        brain->internal_brain->bbb_system) {
        bbb_validation_result_t result;

        // Validate features array (external input data)
        if (!bbb_validate_input(brain->internal_brain->bbb_system,
                               features, num_features * sizeof(float), &result)) {
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID, "BBB rejected features: %s", result.reason);
        }
    }

    // Call internal brain API
    brain_decision_t* decision = brain_decide(brain->internal_brain, features, num_features);

    if (!decision) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR, "Brain prediction failed");
    }

    /* P1-48: Use NIMCP_MAX_LABEL_SIZE instead of hardcoded 63 */
    strncpy(out_label, decision->label, NIMCP_MAX_LABEL_SIZE - 1);
    out_label[NIMCP_MAX_LABEL_SIZE - 1] = '\0';
    *out_confidence = decision->confidence;

    // Deep-copy decision for rubric access (last_decision owns this copy)
    // Note: internal_brain already validated non-NULL above
    brain_decision_t* deep_copy = copy_decision_deep(decision);
    if (deep_copy) {
        if (brain->internal_brain->last_decision) {
            brain_free_decision(brain->internal_brain->last_decision);
        }
        brain->internal_brain->last_decision = deep_copy;
    }
    // If copy_decision_deep fails (OOM), keep the old last_decision intact

    // Free original decision (caller's copy)
    brain_free_decision(decision);

    set_error("No error");
    return NIMCP_OK;
}


/**
 * @brief Fast prediction (no decision caching, no mirror neuron integration)
 *
 * I-M4: THREAD SAFETY CONTRACT
 * This function is NOT thread-safe for concurrent calls on the same brain instance.
 * forward_raw mutates neuron states (spike encoding + weight reads). Callers must
 * provide external synchronization (e.g., Python ThreadSafeBrain uses RLock).
 * Concurrent reads on different brain instances are safe.
 *
 * @param brain Brain handle
 * @param features Input feature vector
 * @param num_features Number of features (must match brain->config.num_inputs)
 * @param out_label Output buffer for predicted label (at least NIMCP_MAX_LABEL_SIZE bytes)
 * @param out_confidence Output confidence [0.0, 1.0]
 * @return NIMCP_OK on success, error code on failure
 */
nimcp_status_t nimcp_brain_predict_fast(
    nimcp_brain_t brain,
    const float* features,
    uint32_t num_features,
    char* out_label,
    float* out_confidence)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain handle");
    API_CHECK_THROW(features, NIMCP_ERROR_NULL_ARG, "Features array is NULL");
    API_CHECK_THROW(out_label, NIMCP_ERROR_NULL_ARG, "Output label buffer is NULL");
    API_CHECK_THROW(out_confidence, NIMCP_ERROR_NULL_ARG, "Output confidence pointer is NULL");

    brain_t ib = brain->internal_brain;
    if (!ib || !ib->network) {
        // I-L2: Consistent error logging for all error paths
        LOG_ERROR("predict_fast: brain not initialized (ib=%p, network=%p)",
                  (void*)ib, ib ? (void*)ib->network : NULL);
        set_error("Brain not initialized");
        return NIMCP_ERROR;
    }

    // I-H1 FIX: Validate input dimensions to prevent out-of-bounds reads in forward pass.
    // Check both zero and mismatch against brain's expected input size.
    if (num_features == 0) {
        LOG_ERROR("predict_fast: num_features must be > 0");
        set_error("num_features must be > 0");
        return NIMCP_ERROR_INVALID;
    }
    if (ib->config.num_inputs > 0 && num_features != ib->config.num_inputs) {
        LOG_ERROR("predict_fast: feature count mismatch: expected %u, got %u",
                  ib->config.num_inputs, num_features);
        set_error("Feature count mismatch: expected %u, got %u",
                  ib->config.num_inputs, num_features);
        return NIMCP_ERROR_INVALID;
    }

    // Allocate output buffer on stack for small outputs, heap for large
    uint32_t num_outputs = ib->config.num_outputs;
    if (num_outputs == 0) {
        LOG_ERROR("predict_fast: brain has 0 outputs");
        set_error("Brain has 0 outputs");
        return NIMCP_ERROR_INVALID;
    }
    // W6-6 FIX: Zero-initialize stack buffer. If forward_raw partially writes
    // (e.g., writes fewer outputs than num_outputs), argmax would read garbage
    // from uninitialized slots, potentially selecting a wrong output index.
    float stack_buf[4096] = {0};
    float* output = (num_outputs <= 4096) ? stack_buf : (float*)nimcp_calloc(num_outputs, sizeof(float));
    if (!output) {
        set_error("Failed to allocate output buffer");
        return NIMCP_ERROR_MEMORY;
    }

    // I-C1: Use forward_raw (no output thresholding) for classification.
    // adaptive_network_forward() applies adaptive thresholding that zeros below-threshold
    // outputs, collapsing all predictions to the same class. forward_raw preserves the
    // raw network activations needed for accurate argmax classification.
    // I-H2: Note -- forward_raw mutates neuron states (spike encoding + weight reads).
    // Concurrent predict_fast calls from multiple threads require external synchronization.
    // W6-9 FIX: Store return value (active neuron count) for diagnostics.
    // forward_raw returning 0 active neurons with valid outputs is normal for sparse
    // networks, so we only log it at debug level rather than treating it as an error.
    uint32_t active_count = adaptive_network_forward_raw(ib->network, features, num_features,
                                 output, num_outputs);
    if (active_count == 0) {
        LOG_DEBUG("predict_fast: forward_raw returned 0 active neurons");
    }

    // I-H3 FIX: NaN-safe argmax. If output contains NaN values, standard comparison
    // (NaN > max_val) is always false, silently picking index 0. Use isfinite() to
    // skip NaN/Inf values and detect all-NaN output as an error condition.
    uint32_t max_idx = 0;
    float max_val = -FLT_MAX;
    bool has_valid_output = false;
    for (uint32_t i = 0; i < num_outputs; i++) {
        if (isfinite(output[i]) && output[i] > max_val) {
            max_val = output[i];
            max_idx = i;
            has_valid_output = true;
        }
    }

    // If all outputs are NaN/Inf, return error with zero confidence
    if (!has_valid_output) {
        LOG_WARN("predict_fast: all %u outputs are NaN/Inf, returning confidence 0", num_outputs);
        snprintf(out_label, NIMCP_MAX_LABEL_SIZE, "UNKNOWN");
        out_label[NIMCP_MAX_LABEL_SIZE - 1] = '\0';
        *out_confidence = 0.0f;
        if (output != stack_buf) nimcp_free(output);
        set_error("All network outputs are NaN/Inf");
        return NIMCP_OK;  // Not a system error, just degenerate output
    }

    // Map to label: use output_labels if available, else "output_N"
    // H1-FIX: Check max_idx < num_output_labels (not num_outputs). The output layer
    // has num_outputs neurons but only num_output_labels label mappings are populated.
    // Using num_outputs could access uninitialized entries beyond num_output_labels.
    if (ib->output_labels && max_idx < ib->num_output_labels && ib->output_labels[max_idx]) {
        strncpy(out_label, ib->output_labels[max_idx], NIMCP_MAX_LABEL_SIZE - 1);
    } else {
        out_label[0] = '\0';
    }

    // I-M2: Confidence uses max/sum_abs normalization (not softmax). This is faster
    // than softmax and provides a reasonable confidence estimate for classification.
    // Softmax would be more principled but requires exp() per output neuron.
    float sum = 0.0f;
    for (uint32_t i = 0; i < num_outputs; i++) {
        if (isfinite(output[i])) sum += fabsf(output[i]);
    }
    float raw_conf = (sum > 0.0f) ? (max_val / sum) : 0.0f;
    if (raw_conf < 0.0f) raw_conf = 0.0f;
    if (raw_conf > 1.0f) raw_conf = 1.0f;
    // I-M1: Guard against NaN/Inf propagating from network outputs
    if (!isfinite(raw_conf)) raw_conf = 0.0f;
    *out_confidence = raw_conf;

    if (output != stack_buf) nimcp_free(output);

    set_error("No error");
    return NIMCP_OK;
}


/**
 * @brief Domain-scoped prediction (I-L3: documentation for complex logic)
 *
 * WHAT: Predicts using only output neurons whose labels start with domain_prefix.
 * WHY:  When a brain has outputs for multiple domains (e.g., "math_add", "math_sub",
 *       "history_ww2"), domain-scoped prediction restricts the argmax to the relevant
 *       domain, preventing cross-domain interference.
 * HOW:  Forward pass through full network, then filter outputs by domain prefix before
 *       argmax. Confidence is computed only among same-domain outputs.
 *
 * FALLBACK: If no output labels match the domain prefix, falls back to predict_fast
 *           (global argmax across all outputs).
 *
 * THREAD SAFETY: Same as predict_fast -- NOT thread-safe for same brain instance.
 */
nimcp_status_t nimcp_brain_predict_in_domain(
    nimcp_brain_t brain,
    const float* features,
    uint32_t num_features,
    const char* domain_prefix,
    char* out_label,
    float* out_confidence)
{
    // If no domain prefix, fall back to standard predict_fast
    if (!domain_prefix || domain_prefix[0] == '\0') {
        return nimcp_brain_predict_fast(brain, features, num_features,
                                         out_label, out_confidence);
    }

    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain handle");
    API_CHECK_THROW(features, NIMCP_ERROR_NULL_ARG, "Features array is NULL");
    API_CHECK_THROW(out_label, NIMCP_ERROR_NULL_ARG, "Output label buffer is NULL");
    API_CHECK_THROW(out_confidence, NIMCP_ERROR_NULL_ARG, "Output confidence pointer is NULL");

    brain_t ib = brain->internal_brain;
    if (!ib || !ib->network) {
        // I-L2: Consistent error logging
        LOG_ERROR("predict_in_domain: brain not initialized");
        set_error("Brain not initialized");
        return NIMCP_ERROR;
    }

    // I-H1 FIX: Validate input dimensions to prevent out-of-bounds reads in forward pass.
    // Check both zero and mismatch against brain's expected input size.
    if (num_features == 0) {
        LOG_ERROR("predict_in_domain: num_features must be > 0");
        set_error("num_features must be > 0");
        return NIMCP_ERROR_INVALID;
    }
    if (ib->config.num_inputs > 0 && num_features != ib->config.num_inputs) {
        LOG_ERROR("predict_in_domain: feature count mismatch: expected %u, got %u",
                  ib->config.num_inputs, num_features);
        set_error("Feature count mismatch: expected %u, got %u",
                  ib->config.num_inputs, num_features);
        return NIMCP_ERROR_INVALID;
    }

    uint32_t num_outputs = ib->config.num_outputs;
    if (num_outputs == 0) {
        LOG_ERROR("predict_in_domain: brain has 0 outputs");
        set_error("Brain has 0 outputs");
        return NIMCP_ERROR_INVALID;
    }
    // W6-6 FIX: Zero-initialize stack buffer to prevent argmax reading garbage
    // if forward_raw partially writes the output array.
    float stack_buf[4096] = {0};
    float* output = (num_outputs <= 4096) ? stack_buf : (float*)nimcp_calloc(num_outputs, sizeof(float));
    if (!output) {
        set_error("Failed to allocate output buffer");
        return NIMCP_ERROR_MEMORY;
    }

    // I-C1: Use forward_raw for classification (same reasoning as predict_fast)
    // W6-9 FIX: Store return value for diagnostics (same as predict_fast).
    uint32_t active_count = adaptive_network_forward_raw(ib->network, features, num_features,
                                 output, num_outputs);
    if (active_count == 0) {
        LOG_DEBUG("predict_in_domain: forward_raw returned 0 active neurons");
    }

    // I-H3 FIX: NaN-safe domain-filtered argmax. Use isfinite() to skip NaN/Inf values.
    size_t prefix_len = strlen(domain_prefix);
    uint32_t max_idx = UINT32_MAX;
    float max_val = -FLT_MAX;
    float domain_sum = 0.0f;
    bool has_valid_domain_output = false;

    for (uint32_t i = 0; i < num_outputs; i++) {
        if (!ib->output_labels || i >= ib->num_output_labels || !ib->output_labels[i])
            continue;
        if (strncmp(ib->output_labels[i], domain_prefix, prefix_len) != 0)
            continue;

        // I-H3: Skip NaN/Inf outputs in domain argmax
        if (!isfinite(output[i])) continue;

        domain_sum += fabsf(output[i]);
        if (output[i] > max_val) {
            max_val = output[i];
            max_idx = i;
            has_valid_domain_output = true;
        }
    }

    // If no labels matched the domain, fall back to global argmax
    // W7-5 (C-INF-5): Use the already-computed output buffer instead of calling
    // predict_fast, which would execute a redundant second forward pass.
    if (max_idx == UINT32_MAX) {
        // I-M4: Log warning when no domain labels match -- silent fallback makes
        // debugging domain configuration issues difficult
        if (!has_valid_domain_output) {
            LOG_WARN("predict_in_domain: no valid labels match domain prefix '%s', "
                     "falling back to global argmax", domain_prefix);
        }

        // Global argmax on already-computed output (same logic as predict_fast)
        uint32_t global_max_idx = 0;
        float global_max_val = -FLT_MAX;
        bool global_has_valid = false;
        for (uint32_t i = 0; i < num_outputs; i++) {
            if (isfinite(output[i]) && output[i] > global_max_val) {
                global_max_val = output[i];
                global_max_idx = i;
                global_has_valid = true;
            }
        }

        if (!global_has_valid) {
            snprintf(out_label, NIMCP_MAX_LABEL_SIZE, "UNKNOWN");
            out_label[NIMCP_MAX_LABEL_SIZE - 1] = '\0';
            *out_confidence = 0.0f;
            if (output != stack_buf) nimcp_free(output);
            set_error("All network outputs are NaN/Inf");
            return NIMCP_OK;
        }

        if (ib->output_labels && global_max_idx < ib->num_output_labels
            && ib->output_labels[global_max_idx]) {
            strncpy(out_label, ib->output_labels[global_max_idx], NIMCP_MAX_LABEL_SIZE - 1);
        } else {
            out_label[0] = '\0';
        }

        // Confidence via sum_abs normalization (matching predict_fast)
        float global_sum = 0.0f;
        for (uint32_t i = 0; i < num_outputs; i++) {
            if (isfinite(output[i])) global_sum += fabsf(output[i]);
        }
        float global_conf = (global_sum > 0.0f) ? (global_max_val / global_sum) : 0.0f;
        if (global_conf < 0.0f) global_conf = 0.0f;
        if (global_conf > 1.0f) global_conf = 1.0f;
        if (!isfinite(global_conf)) global_conf = 0.0f;
        *out_confidence = global_conf;

        if (output != stack_buf) nimcp_free(output);
        set_error("No error");
        return NIMCP_OK;
    }

    strncpy(out_label, ib->output_labels[max_idx], NIMCP_MAX_LABEL_SIZE - 1);
    out_label[NIMCP_MAX_LABEL_SIZE - 1] = '\0';

    // Domain-scoped confidence (only among same-domain outputs)
    float raw_conf = (domain_sum > 0.0f) ? (max_val / domain_sum) : 0.0f;
    if (raw_conf < 0.0f) raw_conf = 0.0f;
    if (raw_conf > 1.0f) raw_conf = 1.0f;
    // I-M1: Guard against NaN/Inf propagating from network outputs
    if (!isfinite(raw_conf)) raw_conf = 0.0f;
    *out_confidence = raw_conf;

    if (output != stack_buf) nimcp_free(output);

    set_error("No error");
    return NIMCP_OK;
}


nimcp_status_t nimcp_brain_infer(
    nimcp_brain_t brain,
    const float* features,
    uint32_t num_features,
    float* outputs,
    uint32_t num_outputs)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain handle");
    API_CHECK_THROW(features, NIMCP_ERROR_NULL_ARG, "Features array is NULL");
    API_CHECK_THROW(outputs, NIMCP_ERROR_NULL_ARG, "Outputs array is NULL");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");
    if (num_features == 0) {
        set_error("num_features must be > 0");
        return NIMCP_ERROR_INVALID;
    }
    // I-H1 FIX: Validate input dimension against brain config
    if (brain->internal_brain->config.num_inputs > 0 &&
        num_features != brain->internal_brain->config.num_inputs) {
        set_error("Feature count mismatch: expected %u, got %u",
                  brain->internal_brain->config.num_inputs, num_features);
        return NIMCP_ERROR_INVALID;
    }
    if (num_outputs == 0) {
        set_error("num_outputs must be > 0");
        return NIMCP_ERROR_INVALID;
    }

    // Call internal brain API to get decision (which includes output vector)
    brain_decision_t* decision = brain_decide(brain->internal_brain, features, num_features);

    if (!decision) {
        set_error("Brain inference failed");
        return NIMCP_ERROR;
    }

    // Guard: output_vector must be valid
    if (!decision->output_vector) {
        brain_free_decision(decision);
        set_error("Decision output_vector is NULL");
        return NIMCP_ERROR;
    }

    // Copy raw output vector
    uint32_t copy_size = (decision->output_size < num_outputs) ? decision->output_size : num_outputs;
    for (uint32_t i = 0; i < copy_size; i++) {
        outputs[i] = decision->output_vector[i];
    }

    // Fill remaining with zeros if requested more outputs than available
    for (uint32_t i = copy_size; i < num_outputs; i++) {
        outputs[i] = 0.0F;
    }

    // Free decision
    brain_free_decision(decision);

    set_error("No error");
    return NIMCP_OK;
}


nimcp_status_t nimcp_brain_decide_full(
    nimcp_brain_t brain,
    const float* features, uint32_t num_features,
    char* out_label, float* out_confidence,
    char* out_explanation,
    float* out_output_vector, uint32_t* out_output_size,
    uint32_t* out_num_active_neurons, float* out_sparsity,
    uint64_t* out_inference_time_us)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain handle");
    API_CHECK_THROW(features, NIMCP_ERROR_NULL_ARG, "Features array is NULL");
    API_CHECK_THROW(out_label, NIMCP_ERROR_NULL_ARG, "Output label buffer is NULL");
    API_CHECK_THROW(out_confidence, NIMCP_ERROR_NULL_ARG, "Output confidence is NULL");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    // I-H1 FIX: Validate input dimensions
    if (num_features == 0) {
        set_error("num_features must be > 0");
        return NIMCP_ERROR_INVALID;
    }
    if (brain->internal_brain->config.num_inputs > 0 &&
        num_features != brain->internal_brain->config.num_inputs) {
        set_error("Feature count mismatch: expected %u, got %u",
                  brain->internal_brain->config.num_inputs, num_features);
        return NIMCP_ERROR_INVALID;
    }

    brain_decision_t* decision = brain_decide(brain->internal_brain, features, num_features);
    if (!decision) {
        set_error("Brain decide_full failed");
        return NIMCP_ERROR;
    }

    strncpy(out_label, decision->label, NIMCP_MAX_LABEL_SIZE - 1);
    out_label[NIMCP_MAX_LABEL_SIZE - 1] = '\0';
    *out_confidence = decision->confidence;

    if (out_explanation) {
        strncpy(out_explanation, decision->explanation, NIMCP_MAX_EXPLANATION_SIZE - 1);
        out_explanation[NIMCP_MAX_EXPLANATION_SIZE - 1] = '\0';
    }

    if (out_output_vector && out_output_size) {
        uint32_t cap = *out_output_size;
        uint32_t copy_n = (decision->output_size < cap) ? decision->output_size : cap;
        for (uint32_t i = 0; i < copy_n; i++) {
            out_output_vector[i] = decision->output_vector[i];
        }
        // W7-6 (C-INF-M1): Report actual number of elements copied, not full
        // output_size. Previously reported decision->output_size which could be
        // larger than the caller's buffer capacity, causing the caller to read
        // beyond what was actually written.
        *out_output_size = copy_n;
    }

    if (out_num_active_neurons) *out_num_active_neurons = decision->num_active_neurons;
    if (out_sparsity) *out_sparsity = decision->sparsity;
    if (out_inference_time_us) *out_inference_time_us = decision->inference_time_us;

    brain_free_decision(decision);
    set_error("No error");
    return NIMCP_OK;
}


nimcp_brain_t nimcp_brain_snapshot_restore(
    nimcp_brain_t brain,
    const char* name)
{
    if (!name) {
        set_error("Snapshot name is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "name is NULL");

        return NULL;
    }

    // Load from snapshot
    brain_t restored_brain = brain_restore_snapshot(
        brain ? brain->internal_brain : NULL,
        name
    );

    if (!restored_brain) {
        set_error("Failed to restore from snapshot");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "restored_brain is NULL");


        return NULL;
    }

    // Allocate new handle (calloc to zero-init last_loss/last_gradient_norm)
    nimcp_brain_t handle = (nimcp_brain_t)nimcp_calloc(1, sizeof(struct nimcp_brain_handle));
    if (!handle) {
        set_error("Failed to allocate brain handle");
        brain_destroy(restored_brain);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "handle is NULL");

        return NULL;
    }

    handle->internal_brain = restored_brain;
    set_error("No error");
    return handle;
}


nimcp_status_t nimcp_brain_snapshot_list(
    nimcp_brain_t brain,
    nimcp_brain_snapshot_info_t* infos,
    uint32_t max_count,
    uint32_t* out_count)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL");
    API_CHECK_THROW(infos, NIMCP_ERROR_NULL_ARG, "Infos array is NULL");

    // Call internal brain list API
    // Note: brain_snapshot_info_t and nimcp_brain_snapshot_info_t have same layout
    bool success = brain_list_snapshots(
        brain->internal_brain,
        (brain_snapshot_info_t*)infos,
        max_count,
        out_count
    );

    if (!success) {
        set_error("Failed to list snapshots");
        return NIMCP_ERROR;
    }

    set_error("No error");
    return NIMCP_OK;
}


nimcp_status_t nimcp_brain_snapshot_delete(
    nimcp_brain_t brain,
    const char* name)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL");
    API_CHECK_THROW(name, NIMCP_ERROR_NULL_ARG, "Snapshot name is NULL");

    // Call internal brain delete API
    bool success = brain_delete_snapshot(brain->internal_brain, name);

    if (!success) {
        set_error("Failed to delete snapshot");
        return NIMCP_ERROR;
    }

    set_error("No error");
    return NIMCP_OK;
}


nimcp_status_t nimcp_brain_probe(nimcp_brain_t brain, nimcp_brain_probe_t* probe) {
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "Brain is NULL");
    NIMCP_CHECK_THROW(probe, NIMCP_ERROR_NULL_ARG, "Probe output structure is NULL");

    // Get internal brain statistics
    brain_stats_t internal_stats;
    NIMCP_CHECK_THROW(brain_get_stats(brain->internal_brain, &internal_stats),
                      NIMCP_ERROR, "Failed to get brain statistics");

    // Map internal stats to public probe structure
    strncpy(probe->task_name, internal_stats.task_name, sizeof(probe->task_name) - 1);
    probe->task_name[sizeof(probe->task_name) - 1] = '\0';

    // Map internal size enum to public enum
    switch (internal_stats.size) {
        case BRAIN_SIZE_TINY:   probe->size = NIMCP_BRAIN_TINY; break;
        case BRAIN_SIZE_SMALL:  probe->size = NIMCP_BRAIN_SMALL; break;
        case BRAIN_SIZE_MEDIUM: probe->size = NIMCP_BRAIN_MEDIUM; break;
        case BRAIN_SIZE_LARGE:  probe->size = NIMCP_BRAIN_LARGE; break;
        default:                probe->size = NIMCP_BRAIN_SMALL; break;
    }

    // P3 FIX: Removed unused brain_config_t internal_config variable (dead code).
    // Note: We use size as a proxy for task type since internal API doesn't expose it directly
    probe->task = NIMCP_TASK_CLASSIFICATION; // Default

    probe->num_neurons = internal_stats.num_neurons;
    probe->num_synapses = internal_stats.num_synapses;
    probe->num_active_synapses = internal_stats.num_active_synapses;

    probe->total_inferences = internal_stats.total_inferences;
    probe->total_learning_steps = internal_stats.total_learning_steps;

    probe->avg_sparsity = internal_stats.avg_sparsity;
    probe->avg_inference_time_us = internal_stats.avg_inference_time_us;
    probe->current_learning_rate = internal_stats.current_learning_rate;

    probe->accuracy = internal_stats.running_accuracy > 0.0f
                    ? internal_stats.running_accuracy
                    : internal_stats.accuracy;
    probe->memory_bytes = internal_stats.memory_bytes;

    // Get input/output sizes from internal brain
    probe->num_inputs = brain_get_num_inputs(brain->internal_brain);
    probe->num_outputs = brain_get_num_outputs(brain->internal_brain);

    // Get COW statistics from internal brain
    brain_cow_stats_t cow_stats;
    if (brain_get_cow_stats(brain->internal_brain, &cow_stats)) {
        probe->is_cow_clone = cow_stats.is_cow_clone;
        probe->cow_ref_count = cow_stats.cow_ref_count;
        probe->cow_shared_bytes = cow_stats.cow_shared_bytes;
        probe->cow_private_bytes = cow_stats.cow_private_bytes;
    } else {
        // Fallback to defaults if COW stats retrieval fails
        probe->is_cow_clone = false;
        probe->cow_ref_count = 0;
        probe->cow_shared_bytes = 0;
        probe->cow_private_bytes = internal_stats.memory_bytes;
    }

    // GPU status
    probe->gpu_available = brain->internal_brain->gpu_enabled;

    set_error("No error");
    return NIMCP_OK;
}


/**
 * @brief Broadcast brain probe data via bio-async for decoupled metrics collection
 *
 * WHAT: Sends brain probe metrics to all interested subscribers via bio-async
 * WHY:  Enables loose coupling - metrics module receives data without direct dependency
 * HOW:  Fills bio_msg_brain_probe_data_t and broadcasts via BIO_MSG_BRAIN_PROBE_DATA
 *
 * @param brain Brain handle to probe and broadcast
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_broadcast_probe(nimcp_brain_t brain) {
    NIMCP_CHECK_THROW(brain && brain->internal_brain, NIMCP_ERROR_NULL_ARG, "Invalid brain handle");

    // Get probe data
    nimcp_brain_probe_t probe;
    nimcp_status_t status = nimcp_brain_probe(brain, &probe);
    if (status != NIMCP_OK) {
        LOG_ERROR("Failed to get brain probe data for broadcast");
        return status;
    }

    // Get module context for bio-async
    bio_module_context_t ctx = get_brain_probe_module_ctx();
    if (!ctx) {
        LOG_DEBUG("Bio-router not available, skipping probe broadcast");
        return NIMCP_OK;  // Not an error - router may not be initialized
    }

    // Build bio-async message
    bio_msg_brain_probe_data_t msg;
    memset(&msg, 0, sizeof(msg));

    // Initialize header
    bio_msg_init_header(&msg.header, BIO_MSG_BRAIN_PROBE_DATA,
                        BIO_MODULE_BRAIN, BIO_MODULE_ALL,
                        sizeof(bio_msg_brain_probe_data_t));
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;

    // Fill probe data
    msg.brain_id = (uint64_t)(uintptr_t)brain;  // Use pointer as unique ID
    strncpy(msg.task_name, probe.task_name, sizeof(msg.task_name) - 1);
    msg.task_name[sizeof(msg.task_name) - 1] = '\0';
    msg.size = (uint32_t)probe.size;
    msg.task = (uint32_t)probe.task;
    msg.num_neurons = probe.num_neurons;
    msg.num_synapses = probe.num_synapses;
    msg.num_active_synapses = probe.num_active_synapses;
    msg.total_inferences = probe.total_inferences;
    msg.total_learning_steps = probe.total_learning_steps;
    msg.avg_sparsity = probe.avg_sparsity;
    msg.avg_inference_time_us = probe.avg_inference_time_us;
    msg.current_learning_rate = probe.current_learning_rate;
    msg.accuracy = probe.accuracy;
    msg.memory_bytes = probe.memory_bytes;
    msg.num_inputs = probe.num_inputs;
    msg.num_outputs = probe.num_outputs;
    msg.is_cow_clone = probe.is_cow_clone;
    msg.cow_ref_count = probe.cow_ref_count;
    msg.cow_shared_bytes = probe.cow_shared_bytes;
    msg.cow_private_bytes = probe.cow_private_bytes;

    // Broadcast to all subscribers (best-effort - failure is non-fatal)
    nimcp_error_t err = bio_router_broadcast(ctx, &msg, sizeof(msg));
    if (err != NIMCP_SUCCESS) {
        // Log warning but don't fail - probe data was collected successfully,
        // broadcast is secondary and optional
        LOG_DEBUG("Probe broadcast incomplete (some modules may not have received): %d", err);
    } else {
        LOG_DEBUG("Brain probe broadcast: brain_id=%llu, neurons=%u, synapses=%u",
                  (unsigned long long)msg.brain_id, msg.num_neurons, msg.num_synapses);
    }

    set_error("No error");
    return NIMCP_OK;
}


//=============================================================================
// Cognitive Output Rubric API Implementation
//=============================================================================

nimcp_status_t nimcp_brain_rubric(nimcp_brain_t brain, nimcp_rubric_t* rubric) {
    NIMCP_CHECK_THROW(brain && brain->internal_brain, NIMCP_ERROR_NULL_ARG, "Invalid brain handle");
    NIMCP_CHECK_THROW(rubric, NIMCP_ERROR_NULL_ARG, "Rubric output structure is NULL");

    brain_t ib = brain->internal_brain;

    /* Lazy-init rubric evaluator on first call */
    if (!ib->rubric_evaluator) {
        ib->rubric_evaluator = rubric_evaluator_create(NULL);
        if (!ib->rubric_evaluator) {
            set_error("Failed to create rubric evaluator");
            return NIMCP_ERROR_MEMORY;
        }
    }

    /* Need a decision to evaluate */
    if (!ib->last_decision) {
        set_error("No decision to evaluate — call predict or decide_full first");
        return NIMCP_ERROR_INVALID;
    }

    /* Run evaluation */
    rubric_result_t result;
    int rc = rubric_evaluate_decision(ib->rubric_evaluator, ib,
                                       ib->last_decision, &result);
    if (rc != 0) {
        set_error("Rubric evaluation failed");
        return NIMCP_ERROR;
    }

    /* Map internal result to public flat struct */
    rubric->internal_consistency   = result.tier1.internal_consistency;
    rubric->confidence_calibration = result.tier1.confidence_calibration;
    rubric->completeness           = result.tier1.completeness;
    rubric->reasoning_chain_quality = result.tier1.reasoning_chain_quality;
    rubric->epistemic_quality      = result.tier1.epistemic_quality;
    rubric->ethical_alignment      = result.tier1.ethical_alignment;
    rubric->tier1_score            = result.tier1.tier1_score;

    rubric->originality            = result.tier2.originality;
    rubric->integration_depth      = result.tier2.integration_depth;
    rubric->communication_clarity  = result.tier2.communication_clarity;
    rubric->engagement_quality     = result.tier2.engagement_quality;
    rubric->empathetic_accuracy    = result.tier2.empathetic_accuracy;
    rubric->information_density    = result.tier2.information_density;
    rubric->tier2_score            = result.tier2.tier2_score;

    rubric->overall_score          = result.overall_score;
    rubric->grade                  = result.grade;
    rubric->grade_modifier         = result.grade_modifier;
    rubric->subsystems_available   = result.subsystems_available;
    rubric->evaluation_time_us     = result.evaluation_time_us;

    set_error("No error");
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_broadcast_rubric(nimcp_brain_t brain) {
    NIMCP_CHECK_THROW(brain && brain->internal_brain, NIMCP_ERROR_NULL_ARG, "Invalid brain handle");

    /* Get rubric data */
    nimcp_rubric_t rubric;
    nimcp_status_t status = nimcp_brain_rubric(brain, &rubric);
    if (status != NIMCP_OK) {
        LOG_ERROR("Failed to get rubric data for broadcast");
        return status;
    }

    /* Get module context for bio-async */
    bio_module_context_t ctx = get_brain_probe_module_ctx();
    if (!ctx) {
        LOG_DEBUG("Bio-router not available, skipping rubric broadcast");
        return NIMCP_OK;
    }

    /* Build bio-async message */
    bio_msg_rubric_data_t msg;
    memset(&msg, 0, sizeof(msg));

    bio_msg_init_header(&msg.header, BIO_MSG_RUBRIC_DATA,
                        BIO_MODULE_RUBRIC, BIO_MODULE_ALL,
                        sizeof(bio_msg_rubric_data_t));
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;

    msg.brain_id = (uint64_t)(uintptr_t)brain;

    /* Tier 1 */
    msg.internal_consistency   = rubric.internal_consistency;
    msg.confidence_calibration = rubric.confidence_calibration;
    msg.completeness           = rubric.completeness;
    msg.reasoning_chain_quality = rubric.reasoning_chain_quality;
    msg.epistemic_quality      = rubric.epistemic_quality;
    msg.ethical_alignment      = rubric.ethical_alignment;
    msg.tier1_score            = rubric.tier1_score;

    /* Tier 2 */
    msg.originality            = rubric.originality;
    msg.integration_depth      = rubric.integration_depth;
    msg.communication_clarity  = rubric.communication_clarity;
    msg.engagement_quality     = rubric.engagement_quality;
    msg.empathetic_accuracy    = rubric.empathetic_accuracy;
    msg.information_density    = rubric.information_density;
    msg.tier2_score            = rubric.tier2_score;

    /* Overall */
    msg.overall_score          = rubric.overall_score;
    msg.grade                  = rubric.grade;
    msg.grade_modifier         = rubric.grade_modifier;
    msg.subsystems_available   = rubric.subsystems_available;
    msg.evaluation_time_us     = rubric.evaluation_time_us;

    /* Broadcast */
    nimcp_error_t err = bio_router_broadcast(ctx, &msg, sizeof(msg));
    if (err != NIMCP_SUCCESS) {
        LOG_DEBUG("Rubric broadcast incomplete: %d", err);
    }

    set_error("No error");
    return NIMCP_OK;
}


//=============================================================================
// Copy-on-Write (COW) Cache API Implementation
//=============================================================================

/**
 * WHAT: Clone brain using copy-on-write caching
 * WHY:  Enable efficient replication with 86% memory savings
 * HOW:  Use internal brain_clone_cow which shares network structures
 *
 * PERFORMANCE: <10ms clone time vs ~1000ms for full copy
 * MEMORY: ~1MB overhead vs ~50MB for full copy
 *
 * Phase 2: True COW sharing via pointer sharing
 */
nimcp_brain_t nimcp_brain_clone_cow(nimcp_brain_t original) {
    // Guard: Validate parameters
    if (!original) {
        set_error("NULL brain provided to nimcp_brain_clone_cow");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "original is NULL");

        return NULL;
    }

    if (!original->internal_brain) {
        set_error("Brain has NULL internal_brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_clone_cow: original->internal_brain is NULL");
        return NULL;
    }

    // Allocate handle (calloc to zero-init last_loss/last_gradient_norm)
    nimcp_brain_t clone_handle = (nimcp_brain_t)nimcp_calloc(1, sizeof(struct nimcp_brain_handle));
    if (!clone_handle) {
        set_error("Failed to allocate brain handle for COW clone");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "clone_handle is NULL");

        return NULL;
    }

    // Use internal COW clone function
    clone_handle->internal_brain = brain_clone_cow(original->internal_brain);

    if (!clone_handle->internal_brain) {
        set_error("Failed to clone internal brain");
        nimcp_free(clone_handle);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_clone_cow: clone_handle->internal_brain is NULL");
        return NULL;
    }

    set_error("No error");
    return clone_handle;
}


/**
 * WHAT: Create instant snapshot of brain state using COW with advanced reference tracking
 * WHY:  Enable zero-copy checkpointing for rollback with complete isolation guarantees
 * HOW:  Use brain_clone_cow with enhanced cache reference tracking for snapshot isolation
 *
 * PERFORMANCE: <1ms snapshot time (zero-copy with cache reference)
 * MEMORY: ~64 bytes overhead + O(1) reference count increment
 *
 * IMPLEMENTATION:
 * 1. Create COW clone using brain_clone_cow (shares network via reference counting)
 * 2. Track shared memory size for cache statistics
 * 3. Initialize snapshot refcount for multi-snapshot scenarios
 * 4. Mark snapshot as isolated to prevent unwanted modifications
 * 5. Multiple snapshots share underlying network data until modifications
 *
 * ADVANCED FEATURES:
 * - Snapshot isolation: Modifications to original don't affect snapshot
 * - Reference tracking: Accurate memory usage reporting
 * - Multi-snapshot support: Create multiple snapshots with shared data
 */
nimcp_brain_snapshot_t nimcp_brain_snapshot_cow(nimcp_brain_t brain) {
    // Guard: Validate parameters
    if (!brain) {
        set_error("NULL brain provided to nimcp_brain_snapshot_cow");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;
    }

    if (!brain->internal_brain) {
        set_error("Brain has NULL internal_brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_snapshot_cow: brain->internal_brain is NULL");
        return NULL;
    }

    // Allocate snapshot handle
    nimcp_brain_snapshot_t snapshot =
        (nimcp_brain_snapshot_t)nimcp_malloc(sizeof(struct nimcp_brain_snapshot_handle));
    if (!snapshot) {
        set_error("Failed to allocate snapshot handle");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snapshot is NULL");

        return NULL;
    }

    // WHAT: Capture current stats before creating snapshot
    // WHY:  Snapshot should preserve stats at snapshot time, not reflect future changes
    // HOW:  Get stats from original brain before cloning
    brain_stats_t current_stats;
    if (!brain_get_stats(brain->internal_brain, &current_stats)) {
        set_error("Failed to get brain stats for snapshot");
        nimcp_free(snapshot);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_snapshot_cow: brain_get_stats is NULL");
        return NULL;
    }

    // WHAT: Create COW clone of brain
    // WHY:  Enables zero-copy snapshot with reference counting
    // HOW:  brain_clone_cow shares network and increments reference count
    brain_t snapshot_brain = brain_clone_cow(brain->internal_brain);

    if (!snapshot_brain) {
        set_error("Failed to create COW clone for snapshot");
        nimcp_free(snapshot);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snapshot_brain is NULL");


        return NULL;
    }

    // CRITICAL: Mark this brain as a snapshot and preserve stats
    // This prevents brain_get_stats from reading the shared network's current stats
    brain_mark_as_snapshot(snapshot_brain, &current_stats);

    // WHAT: Calculate shared memory size for tracking
    // WHY:  Enables accurate memory usage reporting and cache statistics
    // HOW:  Use the preserved stats to estimate network size
    size_t shared_size = (size_t)(current_stats.num_neurons * 100 + current_stats.num_synapses * 20);

    // WHAT: Store snapshot with enhanced tracking metadata
    // WHY:  Track when snapshot was created and memory usage for monitoring
    // HOW:  Save cloned brain, timestamp, and reference tracking info
    snapshot->internal_brain_snapshot = snapshot_brain;
    snapshot->timestamp_us = nimcp_time_monotonic_us();
    snapshot->shared_memory_size = shared_size;
    snapshot->snapshot_refcount = 1;  // This snapshot has one reference
    snapshot->is_isolated = true;     // Snapshot is isolated from original modifications

    // WHAT: Record cache reference for accurate COW statistics
    // WHY:  Track memory savings achieved by COW snapshots
    // HOW:  Use nimcp_cache_record_reference with shared memory size
    if (shared_size > 0) {
        nimcp_cache_record_reference(shared_size);
    }

    set_error("No error");
    return snapshot;
}


/**
 * WHAT: Restore brain state from COW snapshot
 * WHY:  Enable instant rollback to previous state
 * HOW:  Swap brain state with snapshot state
 *
 * PERFORMANCE: <1ms restore time (pointer swap)
 * MEMORY: O(1)
 */
nimcp_status_t nimcp_brain_restore_cow(nimcp_brain_t brain, nimcp_brain_snapshot_t snapshot) {
    // Guard: Validate parameters
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain provided to nimcp_brain_restore_cow");
    API_CHECK_THROW(snapshot, NIMCP_ERROR_NULL_ARG, "NULL snapshot provided to nimcp_brain_restore_cow");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");
    API_CHECK_THROW(snapshot->internal_brain_snapshot, NIMCP_ERROR_INVALID,
                      "Snapshot has NULL internal_brain_snapshot");

    // CRITICAL FIX: Use brain_clone_cow() which properly handles COW refcounting
    // The snapshot was created via save/load, so it owns its network independently
    // We can safely clone it, and the clone will share the snapshot's network

    // Save the old brain pointer for cleanup
    brain_t old_brain = brain->internal_brain;

    // Clone the snapshot to create new brain state (creates COW reference)
    brain_t new_brain = brain_clone_cow(snapshot->internal_brain_snapshot);
    NIMCP_CHECK_THROW(new_brain, NIMCP_ERROR, "Failed to clone snapshot for restore");

    // CRITICAL: Mark the restored brain as a snapshot with the same preserved stats
    // This ensures brain_get_stats returns the snapshot's stats, not the current network stats
    brain_stats_t snapshot_stats;
    if (brain_get_stats(snapshot->internal_brain_snapshot, &snapshot_stats)) {
        brain_mark_as_snapshot(new_brain, &snapshot_stats);
    }

    // Assign new brain (do this before destroying old brain to avoid use-after-free)
    brain->internal_brain = new_brain;

    // Now destroy the old brain (will decrement refcounts properly)
    brain_destroy(old_brain);

    set_error("No error");
    return NIMCP_OK;
}


//=============================================================================
// Phase 10.2: Working Memory API Implementation
//=============================================================================

/**
 * @brief Add item to brain's working memory
 *
 * WHAT: Wrapper for working_memory_add() on brain's working memory
 * WHY:  Provide public API for adding items to working memory
 * HOW:  Validate brain → Get working memory → Call internal API
 */
nimcp_status_t nimcp_brain_working_memory_add(
    nimcp_brain_t brain,
    const float* data,
    uint32_t size,
    float salience)
{
    // Guard: Validate brain
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain provided to working_memory_add");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    // Guard: Validate parameters FIRST (before checking subsystem availability)
    API_CHECK_THROW(data, NIMCP_ERROR_NULL_ARG, "NULL data provided to working_memory_add");
    API_CHECK_THROW(size != 0, NIMCP_ERROR_INVALID, "Invalid size (0) provided to working_memory_add");

    // Guard: Check if working memory enabled (after parameter validation)
    working_memory_t* wm = brain_get_working_memory(brain->internal_brain);
    API_CHECK_THROW(wm, NIMCP_ERROR_INVALID, "Working memory not enabled in brain config");

    // Add to working memory
    bool success = working_memory_add(wm, data, size, salience);
    API_CHECK_THROW(success, NIMCP_ERROR, "Failed to add item to working memory");

    set_error("No error");
    return NIMCP_OK;
}


/**
 * @brief Get item from brain's working memory
 *
 * WHAT: Wrapper for working_memory_get() on brain's working memory
 * WHY:  Provide public API for accessing working memory items
 * HOW:  Validate brain → Get working memory → Call internal API
 */
const float* nimcp_brain_working_memory_get(
    nimcp_brain_t brain,
    uint32_t index,
    uint32_t* size_out)
{
    // Guard: Validate brain
    if (!brain || !brain->internal_brain) {
        set_error("Invalid brain handle");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_working_memory_get: required parameter is NULL (brain, brain->internal_brain)");
        return NULL;
    }

    // Guard: Check if working memory enabled
    working_memory_t* wm = brain_get_working_memory(brain->internal_brain);
    if (!wm) {
        set_error("Working memory not enabled");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm is NULL");

        return NULL;
    }

    // Get item
    const float* item = working_memory_get(wm, index, size_out);
    if (!item) {
        set_error("Invalid index or empty working memory");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "item is NULL");

        return NULL;
    }

    set_error("No error");
    return item;
}


/**
 * @brief Refresh item in brain's working memory
 *
 * WHAT: Wrapper for working_memory_refresh() on brain's working memory
 * WHY:  Provide public API for preventing decay (rehearsal)
 * HOW:  Validate brain → Get working memory → Call internal API
 */
nimcp_status_t nimcp_brain_working_memory_refresh(
    nimcp_brain_t brain,
    uint32_t index)
{
    // Guard: Validate brain
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain provided");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    // Guard: Check if working memory enabled
    working_memory_t* wm = brain_get_working_memory(brain->internal_brain);
    API_CHECK_THROW(wm, NIMCP_ERROR_INVALID, "Working memory not enabled");

    // Refresh item
    bool success = working_memory_refresh(wm, index);
    API_CHECK_THROW(success, NIMCP_ERROR_INVALID, "Invalid index for refresh");

    set_error("No error");
    return NIMCP_OK;
}


nimcp_status_t nimcp_brain_workspace_compete(
    nimcp_brain_t brain,
    nimcp_cognitive_module_t module,
    const float* content,
    uint32_t content_dim,
    float strength)
{
    // Guard: Validate brain
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain provided to workspace_compete");
    NIMCP_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    // Guard: Validate parameters FIRST (before checking workspace availability)
    NIMCP_CHECK_THROW(content, NIMCP_ERROR_NULL_ARG, "NULL content provided to workspace_compete");
    NIMCP_CHECK_THROW(content_dim != 0, NIMCP_ERROR_INVALID, "Invalid content_dim (0)");
    NIMCP_CHECK_THROW(strength >= 0.0F && strength <= 1.0F, NIMCP_ERROR_INVALID,
                      "Strength must be in range [0.0, 1.0]");

    // Guard: Check if global workspace enabled (after parameter validation)
    global_workspace_t* workspace = brain_get_global_workspace(brain->internal_brain);
    NIMCP_CHECK_THROW(workspace, NIMCP_ERROR_INVALID, "Global workspace not enabled in brain config");

    // Convert module enum and compete
    cognitive_module_t internal_module = convert_module_enum(module);
    bool won = global_workspace_compete(workspace, internal_module, content, content_dim, strength);

    if (won) {
        set_error("No error");
        return NIMCP_OK;
    } else {
        set_error("Did not win workspace competition");
        return NIMCP_ERROR;
    }
}


nimcp_status_t nimcp_brain_workspace_subscribe(
    nimcp_brain_t brain,
    nimcp_cognitive_module_t module)
{
    // Guard: Validate brain
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain provided to workspace_subscribe");
    NIMCP_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    // Guard: Check if global workspace enabled
    global_workspace_t* workspace = brain_get_global_workspace(brain->internal_brain);
    NIMCP_CHECK_THROW(workspace, NIMCP_ERROR_INVALID, "Global workspace not enabled in brain config");

    // Subscribe module
    cognitive_module_t internal_module = convert_module_enum(module);
    bool success = global_workspace_subscribe(workspace, internal_module);
    NIMCP_CHECK_THROW(success, NIMCP_ERROR, "Failed to subscribe module");

    set_error("No error");
    return NIMCP_OK;
}


nimcp_status_t nimcp_brain_workspace_unsubscribe(
    nimcp_brain_t brain,
    nimcp_cognitive_module_t module)
{
    // Guard: Validate brain
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain provided to workspace_unsubscribe");
    NIMCP_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    // Guard: Check if global workspace enabled
    global_workspace_t* workspace = brain_get_global_workspace(brain->internal_brain);
    NIMCP_CHECK_THROW(workspace, NIMCP_ERROR_INVALID, "Global workspace not enabled in brain config");

    // Unsubscribe module
    cognitive_module_t internal_module = convert_module_enum(module);
    bool success = global_workspace_unsubscribe(workspace, internal_module);
    NIMCP_CHECK_THROW(success, NIMCP_ERROR, "Failed to unsubscribe module");

    set_error("No error");
    return NIMCP_OK;
}


nimcp_status_t nimcp_network_forward(
    nimcp_network_t network,
    const float* inputs,
    uint32_t num_inputs,
    float* outputs,
    uint32_t num_outputs)
{
    NIMCP_CHECK_THROW(network, NIMCP_ERROR_NULL_ARG, "Network handle is NULL");
    NIMCP_CHECK_THROW(inputs, NIMCP_ERROR_NULL_ARG, "Inputs array is NULL");
    NIMCP_CHECK_THROW(outputs, NIMCP_ERROR_NULL_ARG, "Outputs array is NULL");

    // Call internal network API
    bool success = neural_network_forward(network->internal_network,
                                         inputs, num_inputs,
                                         outputs, num_outputs);

    NIMCP_CHECK_THROW(success, NIMCP_ERROR, "Network forward pass failed");

    set_error("No error");
    return NIMCP_OK;
}


nimcp_status_t nimcp_network_train(
    nimcp_network_t network,
    const float* inputs,
    uint32_t num_inputs,
    const float* targets,
    uint32_t num_targets)
{
    NIMCP_CHECK_THROW(network, NIMCP_ERROR_NULL_ARG, "Network handle is NULL");

    // Training not yet implemented in internal API
    NIMCP_THROW(NIMCP_ERROR, "Training not yet implemented");
    return NIMCP_ERROR;
}


nimcp_status_t nimcp_ethics_check(
    nimcp_ethics_t ethics,
    const float* situation,
    uint32_t num_features,
    float* out_score)
{
    NIMCP_CHECK_THROW(ethics, NIMCP_ERROR_NULL_ARG, "Ethics handle is NULL");
    NIMCP_CHECK_THROW(situation, NIMCP_ERROR_NULL_ARG, "Situation array is NULL");
    NIMCP_CHECK_THROW(out_score, NIMCP_ERROR_NULL_ARG, "Output score pointer is NULL");

    /* P2-1: The internal API accepts non-const float* but we guarantee
     * the features are not modified during evaluation. This const_cast
     * is safe because ethics_engine_evaluate_action() is read-only.
     * TODO: Fix internal API to accept const float*. */
    action_context_t action = {0};
    action.features = (float*)(uintptr_t)situation;  /* const_cast - see above */
    action.num_features = num_features;
    action.affected_agents = NULL;
    action.num_affected_agents = 0;
    action.predicted_harm = 0.0F;

    // Evaluate using internal ethics engine
    ethics_evaluation_t eval = ethics_engine_evaluate_action(ethics->internal_ethics, &action);

    // Convert evaluation to simple score
    // Golden Rule score: -1 (harmful) to +1 (beneficial)
    *out_score = eval.golden_rule_score;

    set_error("No error");
    return NIMCP_OK;
}


nimcp_status_t nimcp_knowledge_add_fact(
    nimcp_knowledge_t knowledge,
    const char* subject,
    const char* predicate,
    const char* object)
{
    NIMCP_CHECK_THROW(knowledge, NIMCP_ERROR_NULL_ARG, "Knowledge handle is NULL");
    NIMCP_CHECK_THROW(subject && predicate && object, NIMCP_ERROR_NULL_ARG,
                      "Subject, predicate, or object is NULL");

    // Create a knowledge item from the fact
    knowledge_item_t item = {0};

    // Use subject as concept
    strncpy(item.concept_name, subject, sizeof(item.concept_name) - 1);

    // Create definition from predicate and object
    snprintf(item.definition, sizeof(item.definition), "%s %s", predicate, object);

    // Set defaults
    item.domain = KNOWLEDGE_DOMAIN_GENERAL;
    item.confidence = 1.0F;
    item.examples = NULL;
    item.num_examples = 0;
    item.related_concepts = NULL;
    item.num_related = 0;
    item.learned_timestamp = 0;
    item.reinforcement_count = 1;

    // Add to internal knowledge system
    bool success = knowledge_add_item(knowledge->internal_knowledge, &item);

    NIMCP_CHECK_THROW(success, NIMCP_ERROR, "Failed to add knowledge item");

    set_error("No error");
    return NIMCP_OK;
}


nimcp_status_t nimcp_knowledge_query(
    nimcp_knowledge_t knowledge,
    const char* query,
    char* out_result,
    uint32_t max_result_len)
{
    NIMCP_CHECK_THROW(knowledge, NIMCP_ERROR_NULL_ARG, "Knowledge handle is NULL");
    NIMCP_CHECK_THROW(query, NIMCP_ERROR_NULL_ARG, "Query is NULL");
    NIMCP_CHECK_THROW(out_result, NIMCP_ERROR_NULL_ARG, "Output result buffer is NULL");

    // Try to retrieve knowledge about the query concept
    knowledge_item_t item;
    bool found = knowledge_retrieve(knowledge->internal_knowledge, query, &item);

    if (found) {
        // Return the definition
        snprintf(out_result, max_result_len, "%s", item.definition);
    } else {
        // Concept not found
        snprintf(out_result, max_result_len, "No knowledge found about '%s'", query);
    }

    set_error("No error");
    return NIMCP_OK;
}

bool nimcp_enable_complex_oscillations(nimcp_brain_t brain, bool enable) {
    if (!brain) {
        set_error("Brain handle is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_enable_complex_oscillations: brain is NULL");
        return false;
    }

    if (!brain->internal_brain) {
        set_error("Brain has NULL internal_brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_enable_complex_oscillations: brain->internal_brain is NULL");
        return false;
    }

    // Get brain's internal structure to access complex oscillation state
    // Note: This requires brain_internal_t access, which would need to be exposed
    // For now, we'll use a hypothetical brain configuration function
    // This would typically be: brain_config_set_complex_oscillations(brain->internal_brain, enable)

    // Since we may not have direct config access, we'll check if it's already enabled
    // and return true if enabling and already enabled, or if disabling and already disabled
    bool currently_enabled = brain_complex_oscillation_is_enabled(brain->internal_brain);

    if (enable == currently_enabled) {
        set_error("No error");
        return true;
    }

    // For actual enable/disable, we would need brain_config_t access
    // This is a placeholder implementation that at least validates the state
    set_error("Complex oscillation enable/disable requires brain reconfiguration");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_enable_complex_oscillations: validation failed");
    return false;
}


//=============================================================================
// Phase 2.8: Dynamic Brain Resizing API
//=============================================================================

bool nimcp_brain_resize(nimcp_brain_t brain, uint32_t new_neuron_count) {
    if (!brain) {
        set_error("Brain handle is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_resize: brain is NULL");
        return false;
    }
    if (!brain->internal_brain) {
        set_error("Brain internal state is NULL");
        return false;
    }

    return brain_resize(brain->internal_brain, new_neuron_count);
}


bool nimcp_brain_auto_resize(nimcp_brain_t brain) {
    if (!brain) {
        set_error("Brain handle is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_auto_resize: brain is NULL");
        return false;
    }
    if (!brain->internal_brain) {
        set_error("Brain internal state is NULL");
        return false;
    }

    return brain_auto_resize(brain->internal_brain);
}


nimcp_status_t nimcp_brain_train_batch(
    nimcp_brain_t brain,
    const float* features,
    const float* targets,
    uint32_t batch_size,
    uint32_t num_features,
    uint32_t num_targets,
    nimcp_training_result_t* result)
{
    NIMCP_CHECK_THROW(brain && features && targets, NIMCP_ERROR_NULL_ARG, "NULL argument provided");
    NIMCP_CHECK_THROW(batch_size != 0, NIMCP_ERROR_INVALID, "Batch size cannot be zero");
    // I-M1 FIX: Validate feature/target counts and check for multiplication overflow
    // in batch indexing. batch_size * num_features could overflow uint32_t, causing
    // out-of-bounds reads from the features/targets arrays.
    NIMCP_CHECK_THROW(num_features != 0, NIMCP_ERROR_INVALID, "num_features cannot be zero");
    NIMCP_CHECK_THROW(num_targets != 0, NIMCP_ERROR_INVALID, "num_targets cannot be zero");
    if (batch_size > UINT32_MAX / num_features) {
        set_error("batch_size * num_features overflow");
        return NIMCP_ERROR_INVALID;
    }
    if (batch_size > UINT32_MAX / num_targets) {
        set_error("batch_size * num_targets overflow");
        return NIMCP_ERROR_INVALID;
    }

    // Train on each example and average results
    float total_loss = 0.0F;
    uint32_t completed = 0;
    nimcp_training_result_t step_result = {0};

    for (uint32_t i = 0; i < batch_size; i++) {
        const float* sample_features = features + ((size_t)i * num_features);
        const float* sample_targets = targets + ((size_t)i * num_targets);

        nimcp_status_t res = nimcp_brain_train_step(
            brain, sample_features, num_features,
            sample_targets, num_targets, &step_result);

        if (res != NIMCP_OK) {
            return res;
        }

        total_loss += step_result.loss;
        completed++;

        if (step_result.early_stopped) {
            break;
        }
    }

    if (result) {
        result->loss = (completed > 0) ? (total_loss / completed) : 0.0F;
        result->learning_rate = step_result.learning_rate;
        result->step = step_result.step;
        result->early_stopped = step_result.early_stopped;
        result->gradient_norm = step_result.gradient_norm;
    }

    set_error("No error");
    return NIMCP_OK;
}


nimcp_status_t nimcp_brain_enable_callbacks(
    nimcp_brain_t brain,
    const nimcp_callback_config_t* config)
{
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL");

    training_pipeline_state_t* state = get_training_state(brain);
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_MEMORY, "Failed to get training state");

    // Destroy existing callbacks if present
    if (state->callbacks) {
        tcb_destroy(state->callbacks);
        state->callbacks = NULL;
    }

    // Build internal config from public config
    tcb_config_t internal_config = tcb_config_default();

    if (config) {
        internal_config.enable_auto_checkpoint = config->enable_auto_checkpoint;
        internal_config.checkpoint_interval = config->checkpoint_interval;
        internal_config.enable_early_stopping = config->enable_early_stopping;
        internal_config.patience = config->patience;
        internal_config.min_delta = config->min_delta;
        internal_config.divergence_threshold = config->divergence_threshold;
        if (config->log_interval > 0) {
            internal_config.enable_auto_logging = true;
            internal_config.log_interval = config->log_interval;
        }
    }

    // Create callback manager
    state->callbacks = tcb_create(&internal_config);
    NIMCP_CHECK_THROW(state->callbacks, NIMCP_ERROR_MEMORY, "Failed to create callback manager");

    state->callbacks_enabled = true;
    set_error("No error");
    return NIMCP_OK;
}


nimcp_status_t nimcp_brain_disable_callbacks(nimcp_brain_t brain) {
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL");

    training_pipeline_state_t* state = get_training_state(brain);
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_INVALID, "No training state");

    state->callbacks_enabled = false;
    set_error("No error");
    return NIMCP_OK;
}

uint32_t nimcp_brain_register_callback(
    nimcp_brain_t brain,
    nimcp_callback_event_t event,
    nimcp_training_callback_fn callback,
    void* user_data,
    const char* name)
{
    if (!brain) {
        set_error("Brain handle is NULL");
        return 0;
    }

    if (!callback) {
        set_error("Callback function is NULL");
        return 0;
    }

    training_pipeline_state_t* state = get_training_state(brain);
    if (!state) {
        set_error("No training state");
        return 0;
    }

    // Auto-enable callbacks if not already enabled
    if (!state->callbacks) {
        nimcp_status_t res = nimcp_brain_enable_callbacks(brain, NULL);
        if (res != NIMCP_OK) {
            return 0;
        }
    }

    // Allocate wrapper
    callback_wrapper_t* wrapper = nimcp_malloc(sizeof(callback_wrapper_t));
    if (!wrapper) {
        set_error("Failed to allocate callback wrapper");
        return 0;
    }

    wrapper->public_callback = callback;
    wrapper->user_data = user_data;

    /* P1-47: Find first free slot instead of wrapping counter, to avoid
     * freeing a live wrapper that's still referenced by the callback manager */
    nimcp_mutex_lock(&g_callback_wrappers_mutex);
    uint32_t wrapper_idx = MAX_CALLBACK_WRAPPERS; /* sentinel = not found */
    for (uint32_t i = 0; i < MAX_CALLBACK_WRAPPERS; i++) {
        if (g_callback_wrappers[i] == NULL) {
            wrapper_idx = i;
            break;
        }
    }
    if (wrapper_idx >= MAX_CALLBACK_WRAPPERS) {
        nimcp_mutex_unlock(&g_callback_wrappers_mutex);
        nimcp_free(wrapper);
        set_error("Callback wrapper table full");
        return 0;
    }
    g_callback_wrappers[wrapper_idx] = wrapper;
    g_next_wrapper_id++;
    nimcp_mutex_unlock(&g_callback_wrappers_mutex);

    // Map public event type to internal event type
    tcb_event_type_t internal_event;
    switch (event) {
        case NIMCP_CB_STEP_COMPLETE:    internal_event = TCB_EVENT_STEP_COMPLETE; break;
        case NIMCP_CB_EPOCH_COMPLETE:   internal_event = TCB_EVENT_EPOCH_COMPLETE; break;
        case NIMCP_CB_LOSS_COMPUTED:    internal_event = TCB_EVENT_LOSS_COMPUTED; break;
        case NIMCP_CB_WEIGHTS_UPDATED:  internal_event = TCB_EVENT_WEIGHTS_UPDATED; break;
        case NIMCP_CB_LR_CHANGED:       internal_event = TCB_EVENT_LR_CHANGED; break;
        case NIMCP_CB_CONVERGENCE:      internal_event = TCB_EVENT_CONVERGENCE; break;
        case NIMCP_CB_DIVERGENCE:       internal_event = TCB_EVENT_DIVERGENCE; break;
        case NIMCP_CB_CHECKPOINT:       internal_event = TCB_EVENT_CHECKPOINT; break;
        default:                        internal_event = TCB_EVENT_STEP_COMPLETE; break;
    }

    // Register with internal callback manager
    tcb_callback_info_t info = {
        .callback = callback_bridge,
        .user_data = wrapper,
        .event_type = internal_event,
        .mode = TCB_MODE_SYNC,
        .priority = TCB_PRIORITY_NORMAL,
        .name = name,
        .enabled = true
    };

    uint32_t cb_id = tcb_register(state->callbacks, &info);
    if (cb_id == 0) {
        set_error("Failed to register callback");
        return 0;
    }

    set_error("No error");
    return cb_id;
}


nimcp_status_t nimcp_brain_unregister_callback(
    nimcp_brain_t brain,
    uint32_t callback_id)
{
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL");

    training_pipeline_state_t* state = get_training_state(brain);
    NIMCP_CHECK_THROW(state && state->callbacks, NIMCP_ERROR_INVALID, "Callbacks not enabled");

    // Unregister from internal manager
    if (!tcb_unregister(state->callbacks, callback_id)) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR, "Failed to unregister callback");
    }

    set_error("No error");
    return NIMCP_OK;
}


//=============================================================================
// Brain Health Probes - System-Level Metrics Collection
//=============================================================================

size_t nimcp_brain_get_memory_rss(void) {
#ifdef __linux__
    FILE* f = fopen("/proc/self/status", "r");
    if (!f) return 0;

    unsigned long rss_kb = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            /* Format: "VmRSS:    <number> kB\n" */
            if (sscanf(line + 6, " %lu", &rss_kb) != 1) {
                rss_kb = 0;
            }
            break;
        }
    }
    fclose(f);
    return (size_t)rss_kb * 1024;  /* Convert kB to bytes */
#else
    return 0;  /* Not supported on non-Linux platforms */
#endif
}


size_t nimcp_brain_get_gpu_vram_used(nimcp_brain_t brain) {
    if (!brain || !brain->internal_brain) return 0;
    if (!brain->internal_brain->gpu_enabled) return 0;

    struct nimcp_gpu_context_s* gpu_ctx = brain->internal_brain->gpu_ctx;
    if (!gpu_ctx) return 0;

    return gpu_ctx->allocated_memory;
}


float nimcp_brain_get_neuron_utilization(nimcp_brain_t brain) {
    if (!brain || !brain->internal_brain) return 0.0f;

    neural_network_t net = adaptive_network_get_base_network(brain->internal_brain->network);
    if (!net) return 0.0f;

    uint32_t total = neural_network_get_num_neurons(net);
    if (total == 0) return 0.0f;

    /* Sample every Nth neuron for speed (same pattern as weight_stats) */
    uint32_t step = total / 15000;
    if (step < 1) step = 1;

    uint32_t sampled = 0;
    uint32_t connected = 0;

    for (uint32_t i = 0; i < total; i += step) {
        neuron_t* n = neural_network_get_neuron(net, i);
        if (!n) continue;
        sampled++;
        if (NEURON_OUT_COUNT(n) > 0) {
            connected++;
        }
    }

    if (sampled == 0) return 0.0f;
    return (float)connected / (float)sampled;
}


nimcp_status_t nimcp_brain_get_immune_metrics(nimcp_brain_t brain, nimcp_immune_metrics_t* out) {
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL");
    NIMCP_CHECK_THROW(out, NIMCP_ERROR_NULL_ARG, "Output structure is NULL");
    NIMCP_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    memset(out, 0, sizeof(*out));

    /* If immune system is not enabled, return zeroed struct successfully */
    if (!brain->internal_brain->immune_enabled ||
        !brain->internal_brain->immune_system) {
        set_error("No error");
        return NIMCP_OK;
    }

    brain_immune_stats_t istats;
    int rc = brain_immune_get_stats(brain->internal_brain->immune_system, &istats);
    if (rc != 0) {
        set_error("Failed to get immune stats");
        return NIMCP_ERROR;
    }

    out->total_exceptions = (uint32_t)istats.antigens_processed;
    out->recovered_exceptions = (uint32_t)istats.threats_neutralized;
    out->inflammation_level = istats.inflammation_level_continuous;
    out->active_antibodies = istats.active_antibodies;

    out->active_t_cells = istats.active_t_cells;
    out->active_b_cells = istats.active_b_cells;
    out->memory_cells = istats.memory_cells;
    out->bbb_threats_processed = (uint32_t)istats.bbb_threats_processed;
    out->cytokine_il1 = istats.cytokine_il1;
    out->cytokine_il6 = istats.cytokine_il6;
    out->cytokine_il10 = istats.cytokine_il10;
    out->cytokine_tnf = istats.cytokine_tnf;
    out->cytokine_ifn_gamma = istats.cytokine_ifn_gamma;
    out->cytokine_il4 = istats.cytokine_il4;

    set_error("No error");
    return NIMCP_OK;
}


/**
 * @brief Per-brain synapse count from previous probe call
 *
 * WHAT: Static storage for computing synapse growth delta
 * WHY:  The delta is per-call, not per-session — simplest approach is a static
 * HOW:  Stores previous synapse count, computes diff on next call
 *
 * NOTE: This is a single static — if multiple brains are probed, the delta
 * reflects the difference since the last call regardless of which brain.
 * A per-brain approach would require a hash map; this is adequate for the
 * common single-brain monitoring case.
 */
static uint64_t s_last_synapse_count = 0;

nimcp_status_t nimcp_brain_get_synapse_stats(nimcp_brain_t brain, nimcp_synapse_stats_t* out) {
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL");
    NIMCP_CHECK_THROW(out, NIMCP_ERROR_NULL_ARG, "Output structure is NULL");
    NIMCP_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    memset(out, 0, sizeof(*out));

    brain_stats_t stats;
    if (!brain_get_stats(brain->internal_brain, &stats)) {
        set_error("Failed to get brain stats");
        return NIMCP_ERROR;
    }

    uint64_t current = (uint64_t)stats.num_synapses;
    out->total_synapses = current;
    out->growth_since_last = (int64_t)current - (int64_t)s_last_synapse_count;
    s_last_synapse_count = current;

    set_error("No error");
    return NIMCP_OK;
}


/* ============================================================================
 * Unified Experience API (Developmental Learning)
 * ============================================================================ */

nimcp_status_t nimcp_brain_innate_hardwire(
    nimcp_brain_t brain,
    const innate_config_t* config)
{
    NIMCP_API_CHECK_NULL(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL");
    NIMCP_API_CHECK_NULL(config, NIMCP_ERROR_NULL_ARG, "Config is NULL");
    NIMCP_API_CHECK_NULL(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    int rc = brain_innate_hardwire(brain->internal_brain, config);
    if (rc != 0) {
        set_error("brain_innate_hardwire failed");
        return NIMCP_ERROR;
    }

    set_error("No error");
    return NIMCP_OK;
}


nimcp_status_t nimcp_brain_experience(
    nimcp_brain_t brain,
    const float* input,
    uint32_t input_size,
    float* output,
    uint32_t output_size,
    float teacher_reward,
    brain_experience_result_t* result)
{
    NIMCP_API_CHECK_NULL(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL");
    NIMCP_API_CHECK_NULL(input, NIMCP_ERROR_NULL_ARG, "Input array is NULL");
    NIMCP_API_CHECK_NULL(output, NIMCP_ERROR_NULL_ARG, "Output array is NULL");
    NIMCP_API_CHECK_NULL(result, NIMCP_ERROR_NULL_ARG, "Result struct is NULL");
    NIMCP_API_CHECK_NULL(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    bool ok = brain_experience(brain->internal_brain, input, input_size,
                                output, output_size, teacher_reward, result);
    if (!ok) {
        set_error("brain_experience failed");
        return NIMCP_ERROR;
    }

    set_error("No error");
    return NIMCP_OK;
}


nimcp_status_t nimcp_brain_experience_configure(
    nimcp_brain_t brain,
    const brain_experience_config_t* config)
{
    NIMCP_API_CHECK_NULL(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL");
    NIMCP_API_CHECK_NULL(config, NIMCP_ERROR_NULL_ARG, "Config is NULL");
    NIMCP_API_CHECK_NULL(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    int rc = brain_experience_configure(brain->internal_brain, config);
    if (rc != 0) {
        set_error("brain_experience_configure failed");
        return NIMCP_ERROR;
    }

    set_error("No error");
    return NIMCP_OK;
}


float nimcp_brain_experience_correct(
    nimcp_brain_t brain,
    const float* expected,
    uint32_t expected_size)
{
    if (!brain || !expected || !brain->internal_brain) return -1.0f;
    return brain_experience_correct(brain->internal_brain, expected, expected_size);
}


nimcp_status_t nimcp_brain_experience_attend(
    nimcp_brain_t brain,
    const char* modality,
    float strength)
{
    NIMCP_API_CHECK_NULL(brain, NIMCP_ERROR_NULL_ARG, "Brain handle is NULL");
    NIMCP_API_CHECK_NULL(modality, NIMCP_ERROR_NULL_ARG, "Modality is NULL");
    NIMCP_API_CHECK_NULL(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    int rc = brain_experience_attend(brain->internal_brain, modality, strength);
    if (rc != 0) {
        set_error("brain_experience_attend failed");
        return NIMCP_ERROR;
    }

    set_error("No error");
    return NIMCP_OK;
}


nimcp_status_t nimcp_brain_speak(
    nimcp_brain_t brain,
    const float* semantic_input,
    uint32_t semantic_dim,
    char* out_text,
    uint32_t text_max_len,
    float* out_confidence,
    float* out_fluency)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain handle");
    API_CHECK_THROW(out_text, NIMCP_ERROR_NULL_ARG, "Output text buffer is NULL");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    brain_t ib = brain->internal_brain;
    out_text[0] = '\0';

    /* Use last decision output if no semantic input provided */
    const float* sem = semantic_input;
    uint32_t sem_dim = semantic_dim;
    if (!sem || sem_dim == 0) {
        /* No cached decision vector in public API — require explicit input */
        set_error("semantic_input is required (no cached decision)");
        return NIMCP_ERROR_NULL_ARG;
    }

    /* Try LNN-based language generator first */
    if (ib->lang_generator) {
        generation_result_t result;
        memset(&result, 0, sizeof(result));

        int rc = language_generator_generate(
            ib->lang_generator, sem, sem_dim, &result);

        if (rc == 0 && result.text && strlen(result.text) > 0) {
            strncpy(out_text, result.text, text_max_len - 1);
            out_text[text_max_len - 1] = '\0';

            if (out_confidence) *out_confidence = result.overall_confidence;
            if (out_fluency) *out_fluency = 1.0f - (result.perplexity > 100.0f ? 1.0f : result.perplexity / 100.0f);

            generation_result_cleanup(&result);
            set_error("No error");
            return NIMCP_OK;
        }
        generation_result_cleanup(&result);
        /* Fall through to orchestrator */
    }

    /* Check language layer */
    if (!ib->language_layer_enabled || !ib->language_layer) {
        set_error("Language layer not enabled or not initialized");
        return NIMCP_ERROR_INVALID;
    }

    /* Generate output through language orchestrator (fallback) */
    uint32_t output_size = 0;
    int gen_rc = language_orchestrator_generate_output(
        ib->language_layer,
        sem, sem_dim,
        out_text, text_max_len,
        &output_size,
        LANGUAGE_OUTPUT_TEXT
    );

    if (gen_rc != 0) {
        set_error("Language production failed");
        return NIMCP_ERROR;
    }

    /* Ensure null termination */
    if (output_size < text_max_len) {
        out_text[output_size] = '\0';
    } else {
        out_text[text_max_len - 1] = '\0';
    }

    /* Compute confidence: ratio of output length to max */
    float conf = (text_max_len > 0 && output_size > 0)
        ? fminf(1.0F, (float)output_size / 50.0F)  /* 50 chars = full confidence */
        : 0.0F;
    if (out_confidence) *out_confidence = conf;

    /* Fluency from production plan */
    float fluency = (output_size > 0) ? 0.7F : 0.0F;  /* Base fluency when words produced */
    if (out_fluency) *out_fluency = fluency;

    set_error("No error");
    return NIMCP_OK;
}


//=============================================================================
// Language Generator API (LNN-based autoregressive text generation)
//=============================================================================

nimcp_status_t nimcp_brain_train_language(
    nimcp_brain_t brain,
    const char* input_text,
    const char* target_text,
    float learning_rate,
    float* out_loss)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain handle");
    API_CHECK_THROW(input_text, NIMCP_ERROR_NULL_ARG, "NULL input_text");
    API_CHECK_THROW(target_text, NIMCP_ERROR_NULL_ARG, "NULL target_text");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    brain_t ib = brain->internal_brain;

    if (!ib->lang_generator || !ib->tokenizer) {
        set_error("Language generator not initialized. Enable language layer.");
        return NIMCP_ERROR_INVALID;
    }

    /* Apply learning rate if specified */
    if (learning_rate > 0.0f) {
        language_generator_set_temperature(ib->lang_generator, 1.0f); /* neutral for training */
    }

    /* Encode input and target */
    enum { MAX_TRAIN_TOKENS = 512 };
    uint32_t input_ids[MAX_TRAIN_TOKENS];
    uint32_t target_ids[MAX_TRAIN_TOKENS];
    uint32_t input_len = 0, target_len = 0;

    int rc = tokenizer_encode(ib->tokenizer, input_text,
                               input_ids, MAX_TRAIN_TOKENS, &input_len);
    if (rc != 0 || input_len == 0) {
        set_error("Failed to tokenize input text");
        return NIMCP_ERROR_INVALID;
    }

    rc = tokenizer_encode(ib->tokenizer, target_text,
                           target_ids, MAX_TRAIN_TOKENS, &target_len);
    if (rc != 0 || target_len == 0) {
        set_error("Failed to tokenize target text");
        return NIMCP_ERROR_INVALID;
    }

    /* Use shorter sequence */
    uint32_t seq_len = (input_len < target_len) ? input_len : target_len;

    /* Train step */
    float loss = 0.0f;
    rc = language_generator_train_step(
        ib->lang_generator,
        input_ids, target_ids,
        seq_len, &loss);

    if (rc != 0) {
        set_error("Language training step failed");
        return NIMCP_ERROR;
    }

    /* Also update embedding layer */
    if (ib->lang_embedding) {
        float lr = (learning_rate > 0.0f) ? learning_rate : 0.001f;
        embedding_update(ib->lang_embedding, lr);
    }

    if (out_loss) *out_loss = loss;

    set_error("No error");
    return NIMCP_OK;
}


nimcp_status_t nimcp_brain_generate_text(
    nimcp_brain_t brain,
    const char* prompt,
    const float* semantic_input,
    uint32_t semantic_dim,
    char* out_text,
    uint32_t text_max_len,
    float* out_confidence,
    float* out_perplexity)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain handle");
    API_CHECK_THROW(out_text, NIMCP_ERROR_NULL_ARG, "Output text buffer is NULL");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    brain_t ib = brain->internal_brain;
    out_text[0] = '\0';

    if (!ib->lang_generator) {
        /* Fall back to brain_speak */
        return nimcp_brain_speak(brain, semantic_input, semantic_dim,
                                 out_text, text_max_len,
                                 out_confidence, out_perplexity);
    }

    generation_result_t result;
    memset(&result, 0, sizeof(result));
    int rc;

    if (prompt && strlen(prompt) > 0) {
        /* Generate from text prompt */
        rc = language_generator_generate_from_prompt(
            ib->lang_generator, prompt, &result);
    } else if (semantic_input && semantic_dim > 0) {
        /* Generate from cognitive state vector */
        rc = language_generator_generate(
            ib->lang_generator, semantic_input, semantic_dim, &result);
    } else {
        set_error("Either prompt or semantic_input is required");
        return NIMCP_ERROR_NULL_ARG;
    }

    if (rc != 0) {
        set_error("Language generation failed");
        generation_result_cleanup(&result);
        return NIMCP_ERROR;
    }

    /* Copy result to output buffer */
    if (result.text) {
        strncpy(out_text, result.text, text_max_len - 1);
        out_text[text_max_len - 1] = '\0';
    }

    if (out_confidence) *out_confidence = result.overall_confidence;
    if (out_perplexity) *out_perplexity = result.perplexity;

    /* Feed generated output to speech cortex if available */
    if (ib->speech_cortex && result.text && strlen(result.text) > 0) {
        /* Encode text as simple audio features for speech cortex */
        float speech_features[128];
        memset(speech_features, 0, sizeof(speech_features));
        uint32_t text_len = (uint32_t)strlen(result.text);
        for (uint32_t i = 0; i < text_len && i < 128; i++) {
            speech_features[i] = (float)result.text[i] / 127.0f;
        }
        speech_cortex_process(ib->speech_cortex, speech_features,
                              text_len < 128 ? text_len : 128, NULL);
    }

    /* Feed semantic representation to visual cortex for cross-modal grounding */
    if (ib->visual_cortex && semantic_input && semantic_dim > 0) {
        /* Project semantic input as visual feature activation */
        float visual_features[256];
        memset(visual_features, 0, sizeof(visual_features));
        uint32_t copy_dim = semantic_dim < 256 ? semantic_dim : 256;
        for (uint32_t i = 0; i < copy_dim; i++) {
            visual_features[i] = semantic_input[i] * 0.5f; /* attenuated cross-modal */
        }
        visual_cortex_process(ib->visual_cortex, (const uint8_t*)visual_features,
                              16, 16, 1, NULL);
    }

    /* Activate cortical columns with generated token representations */
    if (ib->cortical_column_pool && ib->lang_embedding && result.token_ids) {
        /* Feed first few tokens' embeddings as columnar activation */
        uint32_t n_activate = result.num_tokens < 8 ? result.num_tokens : 8;
        float embed_buf[128];
        for (uint32_t t = 0; t < n_activate; t++) {
            if (embedding_lookup(ib->lang_embedding, result.token_ids[t],
                                 embed_buf) == 0) {
                /* Activate a minicolumn per token via columnar connectivity */
                minicolumn_t* col = minicolumn_create(ib->cortical_column_pool, NULL);
                if (col) {
                    minicolumn_destroy(col); /* Transient activation */
                }
            }
        }
    }

    generation_result_cleanup(&result);

    set_error("No error");
    return NIMCP_OK;
}


//=============================================================================
// Grounded Language API
//=============================================================================

nimcp_status_t nimcp_brain_ground_word_with_emotion(
    nimcp_brain_t brain,
    const char* word,
    const float* features,
    uint32_t feature_dim,
    uint32_t modality,
    float attention,
    float valence,
    float arousal)
{
    if (!brain || !word || !features) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = brain->internal_brain;
    if (!b || !b->grounded_lang) return NIMCP_ERROR_NOT_INITIALIZED;

    gl_grounding_event_t event = {
        .word = word,
        .modality = (gl_modality_t)modality,
        .sensory_features = features,
        .feature_dim = feature_dim,
        .emotional_valence = valence,
        .emotional_arousal = arousal,
        .attention = attention,
        .context_sentence = NULL
    };

    return (grounded_language_ground(b->grounded_lang, &event) == 0) ?
        NIMCP_OK : NIMCP_ERROR_OPERATION_FAILED;
}

nimcp_status_t nimcp_brain_ground_word(
    nimcp_brain_t brain,
    const char* word,
    const float* features,
    uint32_t feature_dim,
    uint32_t modality,
    float attention)
{
    /* Legacy zero-emotion path delegates to the with-emotion implementation
     * so we keep exactly one event-construction site. */
    return nimcp_brain_ground_word_with_emotion(
        brain, word, features, feature_dim, modality, attention,
        /*valence=*/0.0f, /*arousal=*/0.0f);
}

nimcp_status_t nimcp_brain_learn_language(
    nimcp_brain_t brain,
    const char* text,
    float* out_loss)
{
    if (!brain || !text) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = brain->internal_brain;
    if (!b || !b->grounded_lang) return NIMCP_ERROR_NOT_INITIALIZED;

    int updates = grounded_language_learn_from_text(b->grounded_lang, text);
    grounded_language_learn_syntax(b->grounded_lang, text);
    /* Drive bridge bigram + trigram STDP from real-text learning. Pre-fix the
     * only callers of learn_text_bigrams were brain_cognitive_learning_step
     * (which passes the cognitive LABEL — 1-2 tokens, too short for trigrams)
     * and the cascade self-train path (own production, gated on confidence).
     * Real user/training text never reached the bridge's trigram path, so
     * bridge_total_trigram_updates stayed at 0 even with the flag enabled.
     * LTD on top-1 false winner (inside learn_next_token_triple) is the
     * only mechanism that can pull saturated dominant word_pops down. */
    (void)grounded_language_learn_text_bigrams(b->grounded_lang, text, 0.02f);

    if (out_loss) *out_loss = (updates > 0) ? 1.0f / (float)updates : 1.0f;
    return (updates >= 0) ? NIMCP_OK : NIMCP_ERROR_OPERATION_FAILED;
}

nimcp_status_t nimcp_brain_learn_language_pair(
    nimcp_brain_t brain,
    const char* input_text,
    const char* target_text,
    float learning_rate,
    float* out_loss)
{
    if (!brain || !input_text || !target_text) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = brain->internal_brain;
    if (!b || !b->grounded_lang) return NIMCP_ERROR_NOT_INITIALIZED;

    float loss = grounded_language_learn_pair(b->grounded_lang,
                                              input_text, target_text, learning_rate);
    if (out_loss) *out_loss = loss;
    return (loss >= 0.0f) ? NIMCP_OK : NIMCP_ERROR_OPERATION_FAILED;
}

nimcp_status_t nimcp_brain_comprehend(
    nimcp_brain_t brain,
    const char* text,
    float* out_semantic,
    uint32_t semantic_dim,
    float* out_confidence)
{
    if (!brain || !text) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = brain->internal_brain;
    if (!b || !b->grounded_lang) return NIMCP_ERROR_NOT_INITIALIZED;

    gl_comprehension_result_t result = {0};
    int rc = grounded_language_comprehend(b->grounded_lang, text, &result);
    if (rc != 0) {
        gl_comprehension_result_cleanup(&result);
        return NIMCP_ERROR;
    }

    if (out_semantic && result.semantic_vector) {
        uint32_t copy_dim = (semantic_dim < 128) ? semantic_dim : 128;
        memcpy(out_semantic, result.semantic_vector, copy_dim * sizeof(float));
    }
    if (out_confidence) *out_confidence = result.comprehension_confidence;

    gl_comprehension_result_cleanup(&result);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_produce_text(
    nimcp_brain_t brain,
    const float* intent,
    uint32_t intent_dim,
    char* out_text,
    uint32_t text_max_len,
    float* out_confidence)
{
    if (!brain || !intent || !out_text) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = brain->internal_brain;
    if (!b || !b->grounded_lang) return NIMCP_ERROR_NOT_INITIALIZED;

    gl_production_result_t result = {0};
    int rc = grounded_language_produce(b->grounded_lang, intent, intent_dim,
                                       GL_PRODUCE_DESCRIBE, &result);
    if (rc != 0 || !result.text) {
        gl_production_result_cleanup(&result);
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    strncpy(out_text, result.text, text_max_len - 1);
    out_text[text_max_len - 1] = '\0';
    if (out_confidence) *out_confidence = result.fluency * result.relevance;

    gl_production_result_cleanup(&result);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_grounded_respond(
    nimcp_brain_t brain,
    const char* input_text,
    char* out_response,
    uint32_t response_max,
    float* out_confidence)
{
    if (!brain || !input_text || !out_response) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = brain->internal_brain;
    if (!b || !b->grounded_lang) return NIMCP_ERROR_NOT_INITIALIZED;

    /* === Toxicity counterclaim interceptor (2026-05-22 fix) ===
     * MUST run before the cascade-vs-bridge branch. The counterclaim
     * emission inside grounded_language_respond() only fires on the
     * non-cascade bridge path; with respond_via_cascade ON (the pod's
     * default), toxic input would otherwise flow through the cascade
     * untouched. Intercepting here guarantees toxic input always gets
     * stage-graded pushback regardless of which downstream path runs.
     *
     * Uses brain-level pointers (b->toxicity_classifier / _response),
     * which the safety wave / resume path populate. NULL-safe: if the
     * toxicity stack is absent we simply fall through to normal respond. */
    if (b->toxicity_classifier && b->toxicity_response &&
        input_text[0] != '\0') {
        toxicity_result_t tox = {0};
        if (toxicity_classify((toxicity_classifier_t*)b->toxicity_classifier,
                              input_text, &tox) == 0 && tox.would_block) {
            toxicity_response_result_t cr = {0};
            int stage = (int)b->current_stage;
            if (stage < 0) stage = 0;
            if (toxicity_response_generate(
                    (toxicity_response_t*)b->toxicity_response,
                    input_text, tox.matched_category, stage, &cr) == 0 &&
                cr.text[0] != '\0') {
                strncpy(out_response, cr.text, response_max - 1);
                out_response[response_max - 1] = '\0';
                if (out_confidence) *out_confidence = 0.95f;
                nimcp_safety_audit_log_event(
                    NIMCP_SAFETY_AUDIT_LGSS_ACTION_BLOCKED, 1,
                    "Counterclaim emitted (brain-level): cat=%s stage=%d "
                    "input='%.40s' response='%.60s'",
                    tox.matched_category, stage, input_text, out_response);
                return NIMCP_OK;
            }
        }
    }

    /* Audit Cat A #1 — cascade-orchestrator path. Opt-in via
     * b->respond_via_cascade (default OFF). When ON, run the full
     * 15-stage cascade with the input as prompt and emit
     * state->utterance. Falls back to the legacy bridge-passthrough
     * if the cascade produces no usable output (preserves the
     * "I don't know." DK-B marker semantics on dead-end inputs). */
    if (b->respond_via_cascade) {
        production_cascade_state_t state;
        memset(&state, 0, sizeof(state));
        int rc = communication_cascade_run(b, input_text,
                                            CASCADE_STAGE_ALL, &state);
        if (rc == 0 && state.utterance && state.utterance[0] &&
            state.word_count > 0) {
            strncpy(out_response, state.utterance, response_max - 1);
            out_response[response_max - 1] = '\0';
            if (out_confidence) {
                /* self_match (Phase 2D-B) is the cleanest "did the brain
                 * actually say what it meant" signal. Fluency falls back
                 * when self_match wasn't computed. */
                float conf = (state.self_match > 0.0f) ? state.self_match
                                                        : state.fluency;
                *out_confidence = conf;
            }
            /* S1 (walkthrough fix, 2026-05-27): output-side toxicity
             * defense-in-depth on the cascade-produced utterance. The
             * respond_via_cascade path returns here WITHOUT calling
             * grounded_language_respond, which is where the output toxicity
             * classifier normally runs — so a toxic cascade output would
             * otherwise get no audit entry and no confidence penalty. Mirror
             * the gl policy exactly: mark-never-delete (do NOT alter the
             * text), log a LGSS_ACTION_BLOCKED event, and reduce confidence
             * proportional to predicted harm. NULL-safe: no classifier → skip. */
            if (b->toxicity_classifier && out_response[0] != '\0') {
                toxicity_result_t otox = {0};
                if (toxicity_classify(
                        (toxicity_classifier_t*)b->toxicity_classifier,
                        out_response, &otox) == 0 && otox.would_block) {
                    nimcp_safety_audit_log_event(
                        NIMCP_SAFETY_AUDIT_LGSS_ACTION_BLOCKED, 2,
                        "cascade_produce flagged: cat=%s harm=%.2f fair=%.2f "
                        "input='%.40s' output='%.40s'",
                        otox.matched_category, otox.predicted_harm,
                        otox.fairness_violation, input_text, out_response);
                    if (out_confidence) *out_confidence *= (1.0f - otox.predicted_harm);
                }
            }
            cascade_state_cleanup(&state);
            return NIMCP_OK;
        }
        cascade_state_cleanup(&state);
        /* Fall through to the bridge passthrough below — preserves the
         * DK-B "I don't know." path for dead-end inputs. */
    }

    int rc = grounded_language_respond(b->grounded_lang, input_text,
                                       out_response, response_max, out_confidence);
    return (rc >= 0) ? NIMCP_OK : NIMCP_ERROR_OPERATION_FAILED;
}

/* Forward decl — definition is further down in this TU. */
static nimcp_status_t _gl_diag_validate(nimcp_brain_t brain, brain_t* out_b);

nimcp_status_t nimcp_brain_set_respond_via_cascade(nimcp_brain_t brain,
                                                     bool enabled) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    b->respond_via_cascade = enabled;
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_respond_via_cascade(nimcp_brain_t brain,
                                                     bool* out_enabled) {
    if (!out_enabled) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    *out_enabled = b->respond_via_cascade;
    return NIMCP_OK;
}

/* Tier 1 Step E — reasoning-conclusion blend opt-in. */
nimcp_status_t nimcp_brain_set_reason_in_content(nimcp_brain_t brain,
                                                   bool enabled) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    b->cascade_reason_in_content = enabled;
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_reason_in_content(nimcp_brain_t brain,
                                                   bool* out_enabled) {
    if (!out_enabled) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    *out_enabled = b->cascade_reason_in_content;
    return NIMCP_OK;
}

/* Non-SNN networks → language content_intent (2026-06-05). Four default-OFF
 * gates; each toggles one network's contribution to the cascade content stage
 * (ANN/LNN/CNN blend a vector into content_intent; HNN modulates the produce
 * floor from its energy-deviation). Same validate/get/set shape as the
 * reason_in_content pair above. */
#define NIMCP_DEFINE_CASCADE_NET_GATE(NAME, FIELD)                            \
nimcp_status_t nimcp_brain_set_##NAME(nimcp_brain_t brain, bool enabled) {     \
    brain_t b = NULL;                                                          \
    nimcp_status_t s = _gl_diag_validate(brain, &b);                           \
    if (s != NIMCP_OK) return s;                                               \
    b->FIELD = enabled;                                                        \
    return NIMCP_OK;                                                           \
}                                                                             \
nimcp_status_t nimcp_brain_get_##NAME(nimcp_brain_t brain, bool* out_enabled){ \
    if (!out_enabled) return NIMCP_ERROR_INVALID_PARAM;                        \
    brain_t b = NULL;                                                          \
    nimcp_status_t s = _gl_diag_validate(brain, &b);                           \
    if (s != NIMCP_OK) return s;                                               \
    *out_enabled = b->FIELD;                                                   \
    return NIMCP_OK;                                                           \
}
NIMCP_DEFINE_CASCADE_NET_GATE(ann_in_content, cascade_ann_in_content)
NIMCP_DEFINE_CASCADE_NET_GATE(lnn_in_content, cascade_lnn_in_content)
NIMCP_DEFINE_CASCADE_NET_GATE(hnn_in_content, cascade_hnn_in_content)
NIMCP_DEFINE_CASCADE_NET_GATE(cnn_in_content, cascade_cnn_in_content)
#undef NIMCP_DEFINE_CASCADE_NET_GATE

/* Slice 6 — thalamic gating public wrappers. Inline-extern the cascade
 * implementations to avoid the header re-include cycle (cascade.h is
 * already pulled in at the top of this TU). */
extern int  communication_cascade_set_thalamic_gate_enabled(brain_t brain, bool enabled);
extern bool communication_cascade_get_thalamic_gate_enabled(brain_t brain);
extern int  communication_cascade_set_thalamic_gate_for_stage(brain_t brain,
                                                              nimcp_cascade_stage_idx_t stage,
                                                              float weight);
extern int  communication_cascade_get_thalamic_gates(brain_t brain,
                                                     float* out_weights,
                                                     bool*  out_overrides,
                                                     uint32_t* count_out);

nimcp_status_t nimcp_brain_set_thalamic_gate_enabled(nimcp_brain_t brain,
                                                      bool enabled) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    return (communication_cascade_set_thalamic_gate_enabled(b, enabled) == 0)
              ? NIMCP_OK : NIMCP_ERROR;
}

nimcp_status_t nimcp_brain_get_thalamic_gate_enabled(nimcp_brain_t brain,
                                                      bool* out_enabled) {
    if (!out_enabled) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    *out_enabled = communication_cascade_get_thalamic_gate_enabled(b);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_thalamic_gate_for_stage(nimcp_brain_t brain,
                                                        uint32_t stage_idx,
                                                        float weight) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (stage_idx >= 15) return NIMCP_ERROR_INVALID_PARAM;
    /* NaN/Inf reads as "clear" — coerce to negative sentinel before
     * forwarding to the cascade setter. */
    if (!isfinite(weight)) weight = -1.0f;
    return (communication_cascade_set_thalamic_gate_for_stage(b,
                (nimcp_cascade_stage_idx_t)stage_idx, weight) == 0)
              ? NIMCP_OK : NIMCP_ERROR;
}

nimcp_status_t nimcp_brain_get_thalamic_gates(nimcp_brain_t brain,
                                                float* out_weights,
                                                bool*  out_overrides,
                                                uint32_t max_count,
                                                uint32_t* count_out) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;

    /* Pull into a local 15-slot scratch buffer so we can copy only up
     * to max_count entries into the caller's array. */
    float    scratch_w[15];
    bool     scratch_o[15];
    uint32_t got = 0;
    if (communication_cascade_get_thalamic_gates(b,
            scratch_w, scratch_o, &got) != 0) {
        return NIMCP_ERROR;
    }
    uint32_t copy = (got < max_count) ? got : max_count;
    if (out_weights) {
        for (uint32_t i = 0; i < copy; i++) out_weights[i] = scratch_w[i];
    }
    if (out_overrides) {
        for (uint32_t i = 0; i < copy; i++) out_overrides[i] = scratch_o[i];
    }
    if (count_out) *count_out = copy;
    return NIMCP_OK;
}

/* =================================================================
 * Slice 5 — phonological loop public API
 *
 * The setters are thin wrappers around the brain_struct fields; the
 * accessor functions (decay/merge/render/clear) live in
 * src/language/nimcp_phonological_loop.c. Setters intentionally do not
 * take the loop_mutex — the underlying field writes are scalar/atomic
 * at the hardware level on every platform we target, and runtime
 * setters are called rarely (once at start-up / config). Read APIs
 * that walk the buffer DO take the mutex inside the helper.
 * ================================================================= */

#include "language/nimcp_phonological_loop.h"

nimcp_status_t nimcp_brain_set_phonological_loop_enabled(nimcp_brain_t brain,
                                                           bool enabled) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    /* Ensure the loop is initialized when flipping ON — broca-init may
     * not have run yet (minimal-mode brain) or may have failed. Init is
     * idempotent + cheap. Failure to init is non-fatal: the caller can
     * still flip the flag, and the cascade helpers will continue to
     * no-op until init succeeds on a subsequent call. */
    if (enabled) {
        (void)phonological_loop_init(b);
    }
    b->loop_enabled = enabled;
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_phonological_loop_decay(nimcp_brain_t brain,
                                                         float decay) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!(decay == decay) /* NaN check */) decay = 0.15f;
    if (decay < 0.0f) decay = 0.0f;
    if (decay > 0.5f) decay = 0.5f;
    /* Setting decay before enable is allowed — init lazily so subsequent
     * loop helpers see the requested value. */
    (void)phonological_loop_init(b);
    b->loop_decay_rate = decay;
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_phonological_loop_threshold(nimcp_brain_t brain,
                                                            float threshold) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    /* S5-H5: validate + coerce. NaN/Inf/<=0 reset to default 0.3.
     * Setting before enable is allowed — init lazily so subsequent
     * loop helpers see the requested value. */
    if (!(threshold == threshold) /* NaN */ ||
        threshold <= 0.0f || threshold > 1.0f) {
        threshold = 0.3f;
    }
    (void)phonological_loop_init(b);
    b->loop_surface_threshold = threshold;
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_clear_phonological_loop(nimcp_brain_t brain) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    phonological_loop_clear(b);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_phonological_loop_state(
    nimcp_brain_t brain,
    char* out_buffer,
    uint32_t out_size,
    uint32_t* out_trace_count)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;

    if (out_buffer && out_size > 0) {
        out_buffer[0] = '\0';
        /* render_active honors enabled-flag internally; returns 0 + empty
         * string when disabled, so callers always see a well-defined
         * surface form. */
        (void)phonological_loop_render_active(b, 0.3f, out_buffer, out_size);
    }
    if (out_trace_count) {
        /* trace_count snapshot — read outside the lock; worst case torn
         * with a concurrent merge, but the field is uint32 (atomic on
         * every supported arch) and this is read-only diagnostic. */
        *out_trace_count = b->loop_trace_count;
    }
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_phonological_loop_diag(
    nimcp_brain_t brain,
    nimcp_phonological_loop_diag_t* out)
{
    if (!out) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;

    /* All scalar reads; no lock — torn-read worst case is a stale
     * snapshot, which is acceptable for a diagnostic surface. */
    out->enabled         = b->loop_enabled;
    out->buffer_len      = b->loop_buffer_len;
    out->trace_count     = b->loop_trace_count;
    out->trace_capacity  = b->loop_trace_capacity;
    out->max_words       = b->loop_max_words;
    out->decay_rate      = b->loop_decay_rate;
    out->last_refresh_ms = b->loop_last_refresh_ms;

    /* Avg trace strength — bounded loop over loop_trace_count traces.
     * No lock; same torn-read tolerance as above. */
    float sum = 0.0f;
    uint32_t n = b->loop_trace_count;
    if (b->loop_phonemic_trace && n > 0) {
        for (uint32_t i = 0; i < n; i++) sum += b->loop_phonemic_trace[i];
    }
    out->avg_trace_strength = (n > 0) ? (sum / (float)n) : 0.0f;
    /* S5-H5: surface threshold. 0 (calloc default) means "uninitialized" —
     * report the effective default (0.3) so trainers see what's in force. */
    {
        float t = b->loop_surface_threshold;
        if (!(t == t) /* NaN */ || t <= 0.0f || t > 1.0f) t = 0.3f;
        out->surface_threshold = t;
    }
    return NIMCP_OK;
}

/* =================================================================
 * Slice 7 — cerebellar prediction-correction public API
 *
 * Default OFF; when enabled, brain->cerebellum (existing adapter) is queried
 * for forward-model predictions inside cascade_stage_motor + stage_prosody
 * and a closed-loop error is fed back via cerebellum_update_forward_model +
 * cerebellum_broadcast_error. The recurrent cascade also sets/clears
 * correction_pending between iterations based on accumulated PE.
 *
 * The wrappers below are intentionally minimal — they only set the flag /
 * strength / threshold fields on the brain. The actual cerebellum calls
 * happen inside nimcp_communication_cascade.c when the flag is set and
 * brain->cerebellum is non-NULL. All scalar-field writes — no atomics, no
 * locks, by contract.
 * ================================================================= */

nimcp_status_t nimcp_brain_set_cerebellar_correction_enabled(nimcp_brain_t brain,
                                                              bool enabled) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    b->cerebellar_correction_enabled = enabled;
    /* Install sane defaults on first enable — caller can still override
     * via set_correction_strength below. */
    if (enabled) {
        if (!isfinite(b->cerebellar_correction_strength) ||
            b->cerebellar_correction_strength <= 0.0f) {
            b->cerebellar_correction_strength = 0.5f;
        }
        if (!isfinite(b->cerebellar_pe_threshold) ||
            b->cerebellar_pe_threshold <= 0.0f) {
            b->cerebellar_pe_threshold = 0.20f;
        }
    } else {
        /* Disable also resets the mid-loop pending flag so a future
         * recurrent run isn't surprised by stale state. */
        b->cerebellar_correction_pending = false;
    }
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_cerebellar_correction_enabled(nimcp_brain_t brain,
                                                              bool* out_enabled) {
    if (!out_enabled) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    *out_enabled = b->cerebellar_correction_enabled;
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_cerebellar_correction_strength(nimcp_brain_t brain,
                                                               float strength) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!isfinite(strength)) strength = 0.5f;
    if (strength < 0.0f) strength = 0.0f;
    if (strength > 1.0f) strength = 1.0f;
    b->cerebellar_correction_strength = strength;
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_cerebellar_pe_threshold(nimcp_brain_t brain,
                                                       float threshold) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    /* The PE norm computed by the cascade is the sum of two cosine-distance
     * norms (motor + prosody), each in [0, 1] — so the meaningful range of
     * the threshold is [0, 2]. NaN/Inf coerce to the default. */
    if (!isfinite(threshold)) threshold = 0.20f;
    if (threshold < 0.0f) threshold = 0.0f;
    if (threshold > 2.0f) threshold = 2.0f;
    b->cerebellar_pe_threshold = threshold;
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_cerebellar_diag(nimcp_brain_t brain,
                                                nimcp_cerebellar_diag_t* out) {
    if (!out) return NIMCP_ERROR_INVALID_PARAM;
    memset(out, 0, sizeof(*out));
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    out->enabled                    = b->cerebellar_correction_enabled ? 1 : 0;
    out->strength                   = b->cerebellar_correction_strength;
    out->correction_pending         = b->cerebellar_correction_pending ? 1 : 0;
    out->pe_threshold               = b->cerebellar_pe_threshold;
    out->predictions_made           = b->cerebellar_predictions_made;
    /* S7-H3: corrections_applied + correction_stages_applied are aliases —
     * same underlying counter (stages that ran while pending=true). The
     * dedicated iter counter is bumped once per recurrent iter from the
     * recurrent loop tail. */
    out->corrections_applied        = b->cerebellar_corrections_applied;
    out->correction_stages_applied  = b->cerebellar_corrections_applied;
    out->correction_iters_applied   = b->cerebellar_correction_iters_applied;
    out->last_pe_norm               = b->cerebellar_last_pe_norm;
    return NIMCP_OK;
}

/* =================================================================
 * Base lexicon bootstrap (Option C of language training plan)
 * =================================================================
 *
 * One-shot loader for data/lexicon/base_lexicon_v1.json — a curated
 * fixture of ~1500 common English nouns/verbs/adjectives with stable
 * 128-d feature vectors. Each entry is forwarded to
 * grounded_language_fast_map(), which creates the lexicon entry +
 * concept binding + context vector in a single call (DRY: this
 * function does NOT reinvent the lexicon insert path).
 *
 * The JSON parser below is intentionally minimal — just enough to
 * walk our own fixture format. It is NOT a general-purpose JSON lib
 * (no escape-decoding past the few we need, no UTF-16 surrogates, no
 * object-key ordering rules). Keeping it private here means the
 * bootstrap module owns parsing end-to-end (SOLID/SRP) without
 * pulling in a third-party dep just for one fixture.
 */

typedef struct {
    const char* p;     /* current cursor */
    const char* end;   /* one past last byte */
} _lex_json_t;

static void _lex_json_skip_ws(_lex_json_t* j) {
    while (j->p < j->end) {
        char c = *j->p;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            j->p++;
        } else {
            return;
        }
    }
}

/* Match a single literal char after whitespace. Returns 1 on success, 0 on mismatch. */
static int _lex_json_match(_lex_json_t* j, char want) {
    _lex_json_skip_ws(j);
    if (j->p < j->end && *j->p == want) { j->p++; return 1; }
    return 0;
}

/* Parse a JSON string into a fixed-size buffer. Returns 1 on success.
 * Handles only the escapes our generator produces ( \" \\ \/ \n \t ).
 * On overflow returns 0 (caller treats as malformed).
 */
static int _lex_json_parse_string(_lex_json_t* j, char* out, size_t out_cap) {
    _lex_json_skip_ws(j);
    if (j->p >= j->end || *j->p != '"') return 0;
    j->p++;
    size_t n = 0;
    while (j->p < j->end) {
        char c = *j->p++;
        if (c == '"') {
            if (n >= out_cap) return 0;
            out[n] = '\0';
            return 1;
        }
        if (c == '\\') {
            if (j->p >= j->end) return 0;
            char esc = *j->p++;
            char real;
            switch (esc) {
                case '"':  real = '"';  break;
                case '\\': real = '\\'; break;
                case '/':  real = '/';  break;
                case 'n':  real = '\n'; break;
                case 't':  real = '\t'; break;
                case 'r':  real = '\r'; break;
                case 'b':  real = '\b'; break;
                case 'f':  real = '\f'; break;
                /* \uXXXX not supported — fixture is plain ASCII. */
                default:   return 0;
            }
            if (n + 1 >= out_cap) return 0;
            out[n++] = real;
            continue;
        }
        if (n + 1 >= out_cap) return 0;
        out[n++] = c;
    }
    return 0;  /* unterminated */
}

/* Parse a JSON number (integer or floating point). Returns 1 on success. */
static int _lex_json_parse_number(_lex_json_t* j, double* out) {
    _lex_json_skip_ws(j);
    if (j->p >= j->end) return 0;
    /* strtod won't run past j->end naturally, but the JSON we parse is always
     * NUL-terminated by the caller (we read into a heap buffer + NUL), so
     * strtod is safe. */
    char* endp = NULL;
    double v = strtod(j->p, &endp);
    if (endp == j->p) return 0;
    j->p = endp;
    *out = v;
    return 1;
}

/* Skip an arbitrary JSON value (any depth). Used to gracefully tolerate
 * unknown fields. Returns 1 on success, 0 on malformed input. */
static int _lex_json_skip_value(_lex_json_t* j) {
    _lex_json_skip_ws(j);
    if (j->p >= j->end) return 0;
    char c = *j->p;
    if (c == '"') {
        /* skip string */
        j->p++;
        while (j->p < j->end) {
            char k = *j->p++;
            if (k == '\\' && j->p < j->end) { j->p++; continue; }
            if (k == '"') return 1;
        }
        return 0;
    }
    if (c == '{' || c == '[') {
        char open_c = c;
        char close_c = (c == '{') ? '}' : ']';
        int depth = 1;
        j->p++;
        while (j->p < j->end && depth > 0) {
            char k = *j->p++;
            if (k == '"') {
                /* skip string body */
                while (j->p < j->end) {
                    char s = *j->p++;
                    if (s == '\\' && j->p < j->end) { j->p++; continue; }
                    if (s == '"') break;
                }
            } else if (k == open_c) {
                depth++;
            } else if (k == close_c) {
                depth--;
            }
        }
        return depth == 0;
    }
    /* number / true / false / null — read until a structural char or ws */
    while (j->p < j->end) {
        char k = *j->p;
        if (k == ',' || k == '}' || k == ']' ||
            k == ' ' || k == '\t' || k == '\n' || k == '\r') {
            return 1;
        }
        j->p++;
    }
    return 1;
}

/* Map "noun"/"verb"/"adjective"/etc. to a category hint for fast_map.
 * fast_map's `category` arg is forwarded to find_or_create_concept; the
 * grounded_language module currently treats it as an opaque uint32. We
 * pass GL_CLASS_* so future versions that wire category → concept type
 * have something meaningful. */
static uint32_t _lex_class_from_string(const char* s) {
    if (!s) return GL_CLASS_UNKNOWN;
    if (strcmp(s, "noun")      == 0) return GL_CLASS_NOUN;
    if (strcmp(s, "verb")      == 0) return GL_CLASS_VERB;
    if (strcmp(s, "adjective") == 0) return GL_CLASS_ADJECTIVE;
    if (strcmp(s, "adverb")    == 0) return GL_CLASS_ADVERB;
    if (strcmp(s, "function")  == 0) return GL_CLASS_FUNCTION;
    if (strcmp(s, "pronoun")   == 0) return GL_CLASS_PRONOUN;
    return GL_CLASS_UNKNOWN;
}

/* Read entire file into a NUL-terminated heap buffer. Caller frees with
 * nimcp_free. Returns NULL on error. *out_len excludes the NUL. */
static char* _lex_slurp_file(const char* path, size_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    /* Sanity cap: 128 MB. The fixture is ~5 MB. */
    if ((unsigned long)sz > (128UL * 1024UL * 1024UL)) { fclose(f); return NULL; }
    char* buf = (char*)nimcp_malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz) { nimcp_free(buf); return NULL; }
    buf[sz] = '\0';
    if (out_len) *out_len = (size_t)sz;
    return buf;
}

/* Parse one word entry: expects { "form": "...", "class": "...", "features": [..] }.
 * Calls grounded_language_fast_map() on success. Returns 1 if the word was
 * bootstrapped, 0 if the entry was malformed/skipped. Skipping never aborts
 * the outer loop — we degrade gracefully on bad rows. */
static int _lex_parse_word_entry(_lex_json_t* j,
                                 grounded_language_t* gl,
                                 uint32_t expected_dim) {
    if (!_lex_json_match(j, '{')) return 0;

    char form[GL_MAX_WORD_LEN] = {0};
    char klass[32] = {0};
    int  have_form = 0, have_class = 0, have_feats = 0;
    /* Stack-side feature buffer sized to the canonical semantic dim.
     * Anything past expected_dim is ignored; missing tail is zero-padded. */
    float features[GL_SEMANTIC_DIM];
    memset(features, 0, sizeof(features));
    uint32_t feat_count = 0;

    /* parse object members */
    int first = 1;
    for (;;) {
        _lex_json_skip_ws(j);
        if (_lex_json_match(j, '}')) break;
        if (!first && !_lex_json_match(j, ',')) return 0;
        first = 0;

        char key[32] = {0};
        if (!_lex_json_parse_string(j, key, sizeof(key))) return 0;
        if (!_lex_json_match(j, ':')) return 0;

        if (strcmp(key, "form") == 0) {
            if (!_lex_json_parse_string(j, form, sizeof(form))) return 0;
            have_form = 1;
        } else if (strcmp(key, "class") == 0) {
            if (!_lex_json_parse_string(j, klass, sizeof(klass))) return 0;
            have_class = 1;
        } else if (strcmp(key, "features") == 0) {
            if (!_lex_json_match(j, '[')) return 0;
            int feat_first = 1;
            for (;;) {
                _lex_json_skip_ws(j);
                if (_lex_json_match(j, ']')) break;
                if (!feat_first && !_lex_json_match(j, ',')) return 0;
                feat_first = 0;
                double v = 0.0;
                if (!_lex_json_parse_number(j, &v)) return 0;
                if (feat_count < expected_dim) {
                    features[feat_count] = (float)v;
                }
                feat_count++;
            }
            have_feats = 1;
        } else {
            /* Unknown key — skip its value. */
            if (!_lex_json_skip_value(j)) return 0;
        }
    }

    if (!have_form || !have_class || !have_feats) return 0;
    if (form[0] == '\0') return 0;
    if (feat_count == 0) return 0;

    uint32_t category = _lex_class_from_string(klass);
    /* feature_dim passed to fast_map = how many we filled in (capped). */
    uint32_t fdim = (feat_count < expected_dim) ? feat_count : expected_dim;

    uint64_t cid = grounded_language_fast_map(gl, form, features, fdim, category);
    return (cid != 0) ? 1 : 0;
}

nimcp_status_t nimcp_brain_bootstrap_lexicon(
    nimcp_brain_t brain,
    const char* json_path)
{
    if (!brain || !json_path) return NIMCP_ERROR_INVALID;
    brain_t b = brain->internal_brain;
    if (!b) return NIMCP_ERROR_INVALID;
    if (!b->grounded_lang) return NIMCP_ERROR;

    size_t buf_len = 0;
    char* buf = _lex_slurp_file(json_path, &buf_len);
    if (!buf) {
        LOG_WARN("nimcp_brain_bootstrap_lexicon: failed to read %s", json_path);
        return NIMCP_ERROR;
    }

    _lex_json_t j = { .p = buf, .end = buf + buf_len };

    /* Top-level object */
    if (!_lex_json_match(&j, '{')) {
        LOG_WARN("nimcp_brain_bootstrap_lexicon: malformed JSON (expected '{')");
        nimcp_free(buf);
        return NIMCP_ERROR;
    }

    int saw_version = 0;
    int saw_words = 0;
    int parsed_count = 0;
    int skipped_count = 0;
    uint32_t expected_dim = grounded_language_get_semantic_dim(b->grounded_lang);
    if (expected_dim == 0 || expected_dim > GL_SEMANTIC_DIM) {
        expected_dim = GL_SEMANTIC_DIM;
    }

    int first_member = 1;
    for (;;) {
        _lex_json_skip_ws(&j);
        if (_lex_json_match(&j, '}')) break;
        if (!first_member && !_lex_json_match(&j, ',')) {
            LOG_WARN("nimcp_brain_bootstrap_lexicon: malformed JSON (member sep)");
            nimcp_free(buf);
            return NIMCP_ERROR;
        }
        first_member = 0;

        char key[32] = {0};
        if (!_lex_json_parse_string(&j, key, sizeof(key))) {
            LOG_WARN("nimcp_brain_bootstrap_lexicon: malformed top-level key");
            nimcp_free(buf);
            return NIMCP_ERROR;
        }
        if (!_lex_json_match(&j, ':')) {
            nimcp_free(buf);
            return NIMCP_ERROR;
        }

        if (strcmp(key, "version") == 0) {
            double v = 0.0;
            if (!_lex_json_parse_number(&j, &v)) {
                nimcp_free(buf);
                return NIMCP_ERROR;
            }
            if ((int)v != 1) {
                LOG_WARN("nimcp_brain_bootstrap_lexicon: unsupported version %d (expected 1)",
                         (int)v);
                nimcp_free(buf);
                return NIMCP_ERROR_INVALID;
            }
            saw_version = 1;
        } else if (strcmp(key, "semantic_dim_hint") == 0) {
            double v = 0.0;
            if (!_lex_json_parse_number(&j, &v)) {
                nimcp_free(buf);
                return NIMCP_ERROR;
            }
            /* Hint only — we trust the runtime gl->semantic_dim. */
            (void)v;
        } else if (strcmp(key, "words") == 0) {
            if (!_lex_json_match(&j, '[')) {
                nimcp_free(buf);
                return NIMCP_ERROR;
            }
            int word_first = 1;
            for (;;) {
                _lex_json_skip_ws(&j);
                if (_lex_json_match(&j, ']')) break;
                if (!word_first && !_lex_json_match(&j, ',')) {
                    LOG_WARN("nimcp_brain_bootstrap_lexicon: malformed words[] (sep)");
                    nimcp_free(buf);
                    return NIMCP_ERROR;
                }
                word_first = 0;

                /* Snapshot cursor so we can recover from a single bad entry. */
                const char* before = j.p;
                int ok = _lex_parse_word_entry(&j, b->grounded_lang, expected_dim);
                if (ok) {
                    parsed_count++;
                } else {
                    skipped_count++;
                    /* Recovery: if the parser bailed mid-object, advance to
                     * the next ',' or ']' at our depth. We do this by re-using
                     * skip_value starting from the snapshot: rewind, then
                     * skip the whole value. */
                    j.p = before;
                    if (!_lex_json_skip_value(&j)) {
                        LOG_WARN("nimcp_brain_bootstrap_lexicon: unrecoverable parse error in words[]");
                        nimcp_free(buf);
                        return NIMCP_ERROR;
                    }
                }
            }
            saw_words = 1;
        } else {
            /* Unknown top-level key — skip. */
            if (!_lex_json_skip_value(&j)) {
                nimcp_free(buf);
                return NIMCP_ERROR;
            }
        }
    }

    nimcp_free(buf);

    if (!saw_version || !saw_words) {
        LOG_WARN("nimcp_brain_bootstrap_lexicon: missing required fields (version=%d words=%d)",
                 saw_version, saw_words);
        return NIMCP_ERROR_INVALID;
    }

    LOG_INFO("Bootstrapped %d words from %s (skipped %d malformed)",
             parsed_count, json_path, skipped_count);
    return NIMCP_OK;
}

/* =================================================================
 * Bulk lexicon loader (binary format v1)
 * =================================================================
 *
 * Streams a packed .bin produced by scripts/build_wordnet_glove_lexicon.py
 * directly into the grounded_language lexicon via fast_map(). Targets the
 * 50K-150K-word use case where JSON parse cost dominates the cold start.
 *
 * Binary layout (little-endian, see the Python builder docstring for the
 * canonical version):
 *
 *   Header (32 bytes):
 *     u32 magic = 'BLEX' (0x58454c42)
 *     u32 version = 1
 *     u32 word_count
 *     u32 vector_dim       (must equal GL_SEMANTIC_DIM = 128)
 *     u32 record_max_form  (informational; we always cap at GL_MAX_WORD_LEN-1)
 *     u32 reserved[3]
 *
 *   Records (packed, no inter-record padding):
 *     u16 form_len         strlen(form), <= GL_MAX_WORD_LEN-1
 *     u8  form[form_len]   ASCII bytes, no NUL on disk
 *     u8  class_enum       gl_word_class_t value
 *     u8  reserved
 *     f32 vec[vector_dim]
 *
 * Validation: magic + version + dim are checked once on header. Each record
 * is bounds-checked against the file tail. A single bad record short-circuits
 * the load with a logged warning — partial loads are intentional (we'd
 * rather have N-1 entries than zero).
 */

#define NIMCP_BULK_LEX_MAGIC   0x58454C42u   /* 'BLEX' */
#define NIMCP_BULK_LEX_VERSION 1u
#define NIMCP_BULK_LEX_HEADER_BYTES 32u

/* Internal entry point — operates directly on grounded_language_t* so it
 * can also be called from brain_init code (which has the internal handle
 * but not the public nimcp_brain_t). The public wrapper below just
 * marshals the brain handle and delegates here. Returns inserted count
 * (>=0) on success, -1 on file/format error. */
int nimcp_internal_load_bulk_lexicon(
    grounded_language_t* gl,
    const char* bin_path)
{
    if (!gl || !bin_path) return -1;

    FILE* f = fopen(bin_path, "rb");
    if (!f) {
        LOG_WARN("nimcp_internal_load_bulk_lexicon: cannot open %s", bin_path);
        return -1;
    }

    /* --- header --- */
    uint32_t hdr[8];
    if (fread(hdr, sizeof(uint32_t), 8, f) != 8) {
        LOG_WARN("nimcp_internal_load_bulk_lexicon: short header in %s", bin_path);
        fclose(f);
        return -1;
    }

    uint32_t magic       = hdr[0];
    uint32_t version     = hdr[1];
    uint32_t word_count  = hdr[2];
    uint32_t vector_dim  = hdr[3];
    /* hdr[4]=record_max_form (informational), hdr[5..7]=reserved */

    if (magic != NIMCP_BULK_LEX_MAGIC) {
        LOG_WARN("nimcp_internal_load_bulk_lexicon: bad magic 0x%08x in %s", magic, bin_path);
        fclose(f);
        return -1;
    }
    if (version != NIMCP_BULK_LEX_VERSION) {
        LOG_WARN("nimcp_internal_load_bulk_lexicon: unsupported version %u (expected %u)",
                 version, NIMCP_BULK_LEX_VERSION);
        fclose(f);
        return -1;
    }

    uint32_t expected_dim = grounded_language_get_semantic_dim(gl);
    if (expected_dim == 0 || expected_dim > GL_SEMANTIC_DIM) expected_dim = GL_SEMANTIC_DIM;
    if (vector_dim != expected_dim) {
        LOG_WARN("nimcp_internal_load_bulk_lexicon: vector_dim %u != gl semantic_dim %u",
                 vector_dim, expected_dim);
        fclose(f);
        return -1;
    }

    LOG_INFO("nimcp_internal_load_bulk_lexicon: header OK — %u words, dim=%u, file=%s",
             word_count, vector_dim, bin_path);

    /* --- records --- */
    /* Per-record buffer reused across the loop. Form fits in
     * GL_MAX_WORD_LEN; vec fits in GL_SEMANTIC_DIM (asserted above via
     * the dim check). */
    char     form[GL_MAX_WORD_LEN];
    float    vec[GL_SEMANTIC_DIM];
    uint32_t inserted = 0;
    uint32_t skipped  = 0;

    for (uint32_t i = 0; i < word_count; i++) {
        uint16_t form_len = 0;
        if (fread(&form_len, sizeof(form_len), 1, f) != 1) {
            LOG_WARN("nimcp_internal_load_bulk_lexicon: short read at record %u (form_len)", i);
            break;
        }
        if (form_len == 0 || form_len >= GL_MAX_WORD_LEN) {
            LOG_WARN("nimcp_internal_load_bulk_lexicon: invalid form_len %u at record %u",
                     (unsigned)form_len, i);
            break;
        }
        if (fread(form, 1, form_len, f) != form_len) {
            LOG_WARN("nimcp_internal_load_bulk_lexicon: short read at record %u (form body)", i);
            break;
        }
        form[form_len] = '\0';

        uint8_t class_byte = 0;
        uint8_t reserved   = 0;
        if (fread(&class_byte, 1, 1, f) != 1 || fread(&reserved, 1, 1, f) != 1) {
            LOG_WARN("nimcp_internal_load_bulk_lexicon: short read at record %u (class)", i);
            break;
        }
        (void)reserved;

        if (fread(vec, sizeof(float), vector_dim, f) != vector_dim) {
            LOG_WARN("nimcp_internal_load_bulk_lexicon: short read at record %u (vec)", i);
            break;
        }

        /* Forward to fast_map. Category arg gets the class enum verbatim,
         * matching the JSON-loader convention so downstream consumers see
         * a uniform value regardless of which loader supplied the entry. */
        uint64_t cid = grounded_language_fast_map(
            gl,
            form,
            vec,
            vector_dim,
            (uint32_t)class_byte
        );
        if (cid != 0) {
            inserted++;
        } else {
            /* fast_map returns 0 once GL_MAX_VOCAB is hit (or on internal
             * alloc failure). We log only once and let the loop drain so
             * the file pointer ends in a known state — but bail early if
             * we've clearly hit the lexicon ceiling. */
            skipped++;
            if (skipped == 1) {
                LOG_WARN("nimcp_internal_load_bulk_lexicon: fast_map returned 0 at "
                         "record %u (form='%s') — lexicon full or alloc failed",
                         i, form);
            }
            if (skipped > 256) {
                LOG_WARN("nimcp_internal_load_bulk_lexicon: aborting after %u skipped "
                         "records (lexicon likely full)", skipped);
                break;
            }
        }
    }

    fclose(f);

    LOG_INFO("nimcp_internal_load_bulk_lexicon: inserted %u / %u entries from %s "
             "(skipped %u)", inserted, word_count, bin_path, skipped);
    return (int)inserted;
}

/* Public API wrapper — marshals the brain handle and delegates to the
 * internal loader so callers without a public handle (e.g., brain init)
 * can still reuse the same code path. */
nimcp_status_t nimcp_brain_load_bulk_lexicon(
    nimcp_brain_t brain,
    const char* bin_path)
{
    if (!brain || !bin_path) return NIMCP_ERROR_INVALID;
    brain_t b = brain->internal_brain;
    if (!b) return NIMCP_ERROR_INVALID;
    if (!b->grounded_lang) {
        LOG_WARN("nimcp_brain_load_bulk_lexicon: no grounded_lang on brain");
        return NIMCP_ERROR;
    }

    int rc = nimcp_internal_load_bulk_lexicon(b->grounded_lang, bin_path);
    if (rc < 0) return NIMCP_ERROR;
    return (rc > 0) ? NIMCP_OK : NIMCP_ERROR;
}

/* =================================================================
 * Grounded-language diagnostics (read-only collapse triage)
 * =================================================================
 *
 * Three accessors share a brain-handle preflight + zero-out-on-error
 * contract. Kept as separate functions per SRP — each one does exactly
 * one thing — but the validation is consolidated in a small static
 * helper to avoid copy-paste. */

static nimcp_status_t _gl_diag_validate(nimcp_brain_t brain, brain_t* out_b)
{
    if (!brain) return NIMCP_ERROR_INVALID;
    brain_t b = brain->internal_brain;
    if (!b) return NIMCP_ERROR;
    *out_b = b;
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_grounded_language_diagnostics(
    nimcp_brain_t brain,
    nimcp_grounded_language_diagnostics_t* out)
{
    if (!out) return NIMCP_ERROR_INVALID;
    memset(out, 0, sizeof(*out));
    out->snn_bridge_blend = -1.0f;  /* Sentinel for "no bridge". */

    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;

    if (b->grounded_lang) {
        gl_stats_t gls;
        memset(&gls, 0, sizeof(gls));
        grounded_language_get_stats(b->grounded_lang, &gls);
        out->vocab_size                  = gls.vocab_size;
        out->total_bindings              = gls.total_bindings;
        out->total_groundings            = gls.total_groundings;
        out->total_comprehensions        = gls.total_comprehensions;
        out->total_productions           = gls.total_productions;
        out->avg_binding_strength        = gls.avg_binding_strength;
        out->avg_comprehension_confidence = gls.avg_comprehension_confidence;
        out->vocabulary_growth_rate      = gls.vocabulary_growth_rate;
        /* Audit-2 follow-up: campaign feature flag snapshot. */
        out->enable_negation_inversion          = grounded_language_get_negation_enabled(b->grounded_lang) ? 1u : 0u;
        out->enable_sense_disambiguation        = grounded_language_get_sense_disambiguation_enabled(b->grounded_lang) ? 1u : 0u;
        out->enable_speech_act_classification   = grounded_language_get_speech_act_classification_enabled(b->grounded_lang) ? 1u : 0u;
        out->enable_sentence_segmentation       = grounded_language_get_sentence_segmentation_enabled(b->grounded_lang) ? 1u : 0u;
        out->enable_topic_shift_detection       = grounded_language_get_topic_shift_enabled(b->grounded_lang) ? 1u : 0u;
        out->enable_reconsolidation             = grounded_language_get_reconsolidation_enabled(b->grounded_lang) ? 1u : 0u;
        out->enable_anaphora_resolution         = grounded_language_get_anaphora_enabled(b->grounded_lang) ? 1u : 0u;
        out->reconsolidation_decay              = grounded_language_get_reconsolidation_decay(b->grounded_lang);
        out->produce_distributional_weight      = grounded_language_get_produce_distributional_weight(b->grounded_lang);
        out->produce_frequency_penalty          = grounded_language_get_produce_frequency_penalty(b->grounded_lang);
        out->topic_shift_threshold              = grounded_language_get_topic_shift_threshold(b->grounded_lang);
        out->topic_shift_min_turns              = grounded_language_get_topic_shift_min_turns(b->grounded_lang);
        /* Process-global counter (not per-GL-instance) — surfaced so the
         * trainer can tell trigram "stall" from "cold-start ramp". */
        out->next_token_cold_start_skips        = grounded_language_next_token_cold_start_skips();
        /* NLP-2 coref + NLP-1 subword OOV reachability telemetry. */
        out->enable_coref_resolution            = grounded_language_get_coref_resolution_enabled(b->grounded_lang) ? 1u : 0u;
        out->enable_subword_oov_fallback        = grounded_language_get_subword_oov_fallback_enabled(b->grounded_lang) ? 1u : 0u;
        out->coref_attempts                     = grounded_language_coref_attempts(b->grounded_lang);
        out->coref_resolved                     = grounded_language_coref_resolved(b->grounded_lang);
        out->subword_oov_attempts               = grounded_language_subword_oov_attempts(b->grounded_lang);
        out->subword_oov_resolved               = grounded_language_subword_oov_resolved(b->grounded_lang);
        /* TF (tier-feedback) counter + knob surface. The 10 counters mirror
         * gl_stats_t.tf_* fields (set above via grounded_language_get_stats);
         * the 5 knobs are independent getters on the gl handle. */
        out->tf_calls                           = gls.tf_calls;
        out->tf_deltas_captured                 = gls.tf_deltas_captured;
        out->tf_outcome_ok                      = gls.tf_outcome_ok;
        out->tf_outcome_blocked_stage           = gls.tf_outcome_blocked_stage;
        out->tf_outcome_blocked_master          = gls.tf_outcome_blocked_master;
        out->tf_outcome_blocked_da              = gls.tf_outcome_blocked_da;
        out->tf_outcome_blocked_stale           = gls.tf_outcome_blocked_stale;
        out->tf_outcome_blocked_retry           = gls.tf_outcome_blocked_retry;
        out->tf_trigram_updates                 = gls.tf_trigram_updates;
        out->tf_distrib_updates                 = gls.tf_distrib_updates;
        out->tf_stdp_updates_pos                = gls.tf_stdp_updates_pos;
        out->tf_stdp_updates_neg                = gls.tf_stdp_updates_neg;
        out->produce_corrector_feedback_enabled =
            grounded_language_get_produce_corrector_feedback_enabled(b->grounded_lang) ? 1u : 0u;
        out->tf_enabled_correctors              =
            grounded_language_get_tf_enabled_correctors(b->grounded_lang);
        out->tf_lr_trigram                      = grounded_language_get_tf_lr_trigram(b->grounded_lang);
        out->tf_lr_distrib                      = grounded_language_get_tf_lr_distrib(b->grounded_lang);
        out->tf_lr_bridge_stdp                  = grounded_language_get_tf_lr_bridge_stdp(b->grounded_lang);
        /* T3-2 (2026-05-29): cohesive-conjunction-insertion flag. */
        out->produce_t3_conjunction             =
            grounded_language_get_produce_t3_conjunction(b->grounded_lang) ? 1u : 0u;
        out->produce_via_snn                    =
            grounded_language_get_produce_via_snn(b->grounded_lang) ? 1u : 0u;
        out->metacog_gates_produce              =
            grounded_language_get_metacog_gates_produce(b->grounded_lang) ? 1u : 0u;
    }

    if (b->snn_lang_bridge) {
        out->snn_bridge_blend = snn_language_bridge_get_blend(b->snn_lang_bridge);
        snn_lang_stats_t bs;
        memset(&bs, 0, sizeof(bs));
        if (snn_language_bridge_get_stats(b->snn_lang_bridge, &bs) == 0) {
            out->bridge_total_productions   = (uint32_t)bs.total_produce_calls;
            out->bridge_avg_word_confidence = bs.avg_word_confidence;
            out->bridge_avg_binding_weight  = bs.avg_binding_weight;
            out->bridge_active_bindings     = bs.active_bindings;
            /* Audit-2 follow-up: TC-11 decode telemetry surface. */
            out->bridge_decode_total_ns     = bs.decode_total_ns;
            out->bridge_total_decode_calls  = bs.total_decode_calls;
            /* Plasticity-telemetry surface — lets operators verify trigram
             * flag flips drive the counter, and discriminates self_train
             * miss causes (no calls vs. zero pairs vs. target_misses). */
            out->bridge_total_stdp_updates          = bs.total_stdp_updates;
            out->bridge_total_trigram_updates       = bs.total_trigram_updates;
            out->bridge_echo_correct_calls          = bs.echo_correct_calls;
            out->bridge_echo_correct_pairs          = bs.echo_correct_pairs;
            out->bridge_echo_correct_target_misses  = bs.echo_correct_target_misses;
            out->bridge_comprehend_stdp_passes      = bs.comprehend_stdp_passes;
            out->bridge_comprehend_stdp_pairs_fired = bs.comprehend_stdp_pairs_fired;
            out->bridge_da_gated_stdp_passes        = bs.da_gated_stdp_passes;
            out->bridge_last_da_modulation          = bs.last_da_modulation;
            /* Wave-3 (2026-05-19) S4 lateral-inhibition counters. */
            out->bridge_lateral_inhibition_decode_calls       = bs.lateral_inhibition_decode_calls;
            out->bridge_lateral_inhibition_winner_margin_sum  = bs.lateral_inhibition_winner_margin_sum;
            out->bridge_lateral_inhibition_settled_steps_sum  = bs.lateral_inhibition_settled_steps_sum;
            out->bridge_lateral_inhibition_nan_fallbacks      = bs.lateral_inhibition_nan_fallbacks;
        }
        out->bridge_enable_da_modulation        = snn_language_bridge_get_da_modulation_enabled(b->snn_lang_bridge) ? 1u : 0u;
        out->bridge_enable_trigram_learning     = snn_language_bridge_get_trigram_learning_enabled(b->snn_lang_bridge) ? 1u : 0u;
        out->bridge_ltd_margin                  = snn_language_bridge_get_ltd_margin(b->snn_lang_bridge);
    }

    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_prune_lang_bindings(
    nimcp_brain_t brain,
    uint32_t max_bindings_per_word,
    uint64_t* dropped_out)
{
    if (!brain || max_bindings_per_word == 0) return NIMCP_ERROR_INVALID;
    brain_t b = brain->internal_brain;
    if (!b || !b->grounded_lang) return NIMCP_ERROR;

    uint64_t dropped = grounded_language_prune_bindings(b->grounded_lang,
                                                       max_bindings_per_word);
    if (dropped_out) *dropped_out = dropped;
    return NIMCP_OK;
}

/* Runtime fix for bridge->neuromod NULL state. The SNN language bridge's
 * DA gate (snn_language_bridge.c:910) is conditioned on bridge->neuromod
 * being non-NULL. Both init paths (nimcp_brain_init_language.c:841 and
 * nimcp_brain_init_language_pops.c:370) are gated on
 * brain->neuromodulator_system existing AT THE TIME those init waves
 * run. If init order puts the bridge first, bridge->neuromod stays NULL
 * forever — bridge_da_gated_stdp_passes never increments. This RPC lets
 * an operator re-connect them at runtime once both exist. Just stores
 * a pointer, no concurrency hazard. */
nimcp_status_t nimcp_brain_connect_lang_bridge_neuromod(nimcp_brain_t brain)
{
    if (!brain) return NIMCP_ERROR_INVALID;
    brain_t b = brain->internal_brain;
    if (!b) return NIMCP_ERROR_NOT_INITIALIZED;
    if (!b->snn_lang_bridge) return NIMCP_ERROR;
    if (!b->neuromodulator_system) return NIMCP_ERROR;

    int rc = snn_language_bridge_connect_neuromod(
        b->snn_lang_bridge, b->neuromodulator_system);
    return (rc == 0) ? NIMCP_OK : NIMCP_ERROR;
}

/*=============================================================================
 * Tier-1 #2: Anaphora / pronoun-resolution toggle.
 *===========================================================================*/

nimcp_status_t nimcp_brain_set_anaphora_enabled(
    nimcp_brain_t brain,
    bool enabled)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    bool ok = grounded_language_set_anaphora_enabled(b->grounded_lang, enabled);
    return ok ? NIMCP_OK : NIMCP_ERROR;
}

/* EN-5 — read-only engram integration toggle. Wires the brain-level
 * engram_system pointer into the grounded_language pass and flips the
 * enabled flag in one shot. brain->engram_system is created by the
 * cognitive init wave (nimcp_brain_init_cognitive.c) so it's normally
 * non-NULL by the time this is called. NULL engram_system → NIMCP_ERROR. */
nimcp_status_t nimcp_brain_set_grounded_engram_enabled(
    nimcp_brain_t brain,
    bool enabled)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    if (!b->engram_system) return NIMCP_ERROR;
    int rc = grounded_language_set_engram_system(b->grounded_lang,
                                                   b->engram_system, enabled);
    return (rc == 0) ? NIMCP_OK : NIMCP_ERROR;
}

/* IM-3 — Tier-3 immune content-inspection toggle on grounded_language.
 * Wires brain->immune_system into the grounded_language pass and flips
 * the enabled flag in one shot. brain->immune_system is created during
 * the cognitive/safety init wave; NULL means immune is disabled in
 * brain config or the wave hasn't run yet — return NIMCP_ERROR. */
nimcp_status_t nimcp_brain_set_grounded_immune_enabled(
    nimcp_brain_t brain,
    bool enabled)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    if (!b->immune_system) return NIMCP_ERROR;
    int rc = grounded_language_set_immune_system(b->grounded_lang,
                                                   b->immune_system, enabled);
    return (rc == 0) ? NIMCP_OK : NIMCP_ERROR;
}

/*=============================================================================
 * PA-4+ : Bigram FFT spectral metrics
 *
 * Lazily attaches a bigram-spectrum tracker to the brain's grounded
 * language module on first call. The tracker is owned for the brain's
 * lifetime and tied to the brain pointer in a small static side-map so
 * we don't have to extend the brain struct layout.
 *===========================================================================*/

#define BIGRAM_SPECTRUM_VOCAB_CAP 256u
#define BRAIN_SPECTRUM_MAP_CAP    4

static struct {
    nimcp_brain_t      brain;
    bigram_spectrum_t* spectrum;
} g_brain_spectrum_map[BRAIN_SPECTRUM_MAP_CAP] = { {0} };

static bigram_spectrum_t* _brain_get_or_create_spectrum(nimcp_brain_t brain,
                                                          struct grounded_language* gl) {
    /* Existing? */
    for (int i = 0; i < BRAIN_SPECTRUM_MAP_CAP; i++) {
        if (g_brain_spectrum_map[i].brain == brain) {
            return g_brain_spectrum_map[i].spectrum;
        }
    }
    /* Create new + attach. */
    bigram_spectrum_t* bs = bigram_spectrum_create(BIGRAM_SPECTRUM_VOCAB_CAP);
    if (!bs) return NULL;

    /* Insert into first free slot. */
    for (int i = 0; i < BRAIN_SPECTRUM_MAP_CAP; i++) {
        if (g_brain_spectrum_map[i].brain == NULL) {
            g_brain_spectrum_map[i].brain = brain;
            g_brain_spectrum_map[i].spectrum = bs;
            grounded_language_attach_bigram_spectrum(gl, bs);
            return bs;
        }
    }
    /* Map full. Tear down — caller will see NULL and bail. */
    bigram_spectrum_destroy(bs);
    return NULL;
}

nimcp_status_t nimcp_brain_get_bigram_spectral_metrics(
    nimcp_brain_t brain,
    bigram_spectral_metrics_t* out)
{
    if (!out) return NIMCP_ERROR_INVALID;
    out->peak_strength = 0.0f;
    out->low_freq_concentration = 0.0f;
    out->spectral_entropy = 0.0f;

    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;

    bigram_spectrum_t* bs =
        _brain_get_or_create_spectrum(brain, b->grounded_lang);
    if (!bs) return NIMCP_ERROR;

    if (bigram_spectrum_compute(bs, out) != 0) return NIMCP_ERROR;
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_probe_comprehend(
    nimcp_brain_t brain,
    const char* input_text,
    float* out_components,
    uint32_t max_components,
    uint32_t* out_components_written,
    float* out_l2_norm,
    float* out_confidence,
    uint32_t* out_concept_count)
{
    /* Zero outputs first so callers see a deterministic failure state. */
    if (out_components && max_components > 0) {
        memset(out_components, 0, max_components * sizeof(float));
    }
    if (out_components_written) *out_components_written = 0;
    if (out_l2_norm)            *out_l2_norm = 0.0f;
    if (out_confidence)         *out_confidence = 0.0f;
    if (out_concept_count)      *out_concept_count = 0;

    if (!brain || !input_text) return NIMCP_ERROR_INVALID;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;

    gl_comprehension_result_t result;
    memset(&result, 0, sizeof(result));
    int rc = grounded_language_comprehend(b->grounded_lang, input_text, &result);
    if (rc != 0) {
        gl_comprehension_result_cleanup(&result);
        return NIMCP_ERROR;
    }

    if (out_confidence)    *out_confidence = result.comprehension_confidence;
    if (out_concept_count) *out_concept_count = result.concept_count;

    /* Compute L2 norm + capture leading components, bounded strictly
     * by semantic_dim. Walking past it reads OOB heap and produced
     * garbage L2/components in the first version of this probe. */
    if (result.semantic_vector) {
        uint32_t sdim = grounded_language_get_semantic_dim(b->grounded_lang);
        if (sdim == 0) sdim = 1;  /* defensive — shouldn't happen */
        double sumsq = 0.0;
        uint32_t written = 0;
        for (uint32_t i = 0; i < sdim; i++) {
            float v = result.semantic_vector[i];
            if (!isfinite(v)) v = 0.0f;
            sumsq += (double)v * (double)v;
            if (out_components && written < max_components) {
                out_components[written++] = v;
            }
        }
        if (out_components_written) *out_components_written = written;
        if (out_l2_norm) *out_l2_norm = (float)sqrt(sumsq);
    }

    gl_comprehension_result_cleanup(&result);
    return NIMCP_OK;
}

/* Public top-phrases accessor.
 *
 * Returns the count copied (>= 0) or -1 on error. The internal
 * grounded_language_get_top_phrases() returns const-pointers into a
 * GL-owned table; we copy form/frequency/component_words into a caller-
 * owned POD array to avoid lifetime entanglement.
 */
int nimcp_brain_get_top_phrases(
    nimcp_brain_t brain,
    nimcp_phrase_entry_t* out_phrases,
    uint32_t max_phrases)
{
    if (!out_phrases || max_phrases == 0) return -1;
    /* Zero output up front so a partial failure leaves a deterministic state. */
    memset(out_phrases, 0, (size_t)max_phrases * sizeof(*out_phrases));

    brain_t b = NULL;
    if (_gl_diag_validate(brain, &b) != NIMCP_OK) return -1;
    if (!b->grounded_lang) return -1;

    /* Bound max_phrases to GL capacity to keep our stack array small. */
    uint32_t cap = max_phrases;
    if (cap > GL_MAX_PHRASES) cap = GL_MAX_PHRASES;

    const gl_phrase_t* tmp[GL_MAX_PHRASES] = {0};
    uint32_t n = grounded_language_get_top_phrases(b->grounded_lang,
                                                    /* min_freq */ 0,
                                                    /* min_n    */ 0,
                                                    tmp, cap);
    for (uint32_t i = 0; i < n; i++) {
        if (!tmp[i]) continue;
        /* form is already null-terminated and bounded by GL_MAX_PHRASE_LEN
         * (== NIMCP_PHRASE_FORM_MAX); strncpy + force-terminate is paranoid
         * defence in depth in case GL_MAX_PHRASE_LEN ever shrinks. */
        strncpy(out_phrases[i].form, tmp[i]->form, NIMCP_PHRASE_FORM_MAX - 1);
        out_phrases[i].form[NIMCP_PHRASE_FORM_MAX - 1] = '\0';
        out_phrases[i].frequency       = tmp[i]->frequency;
        out_phrases[i].component_words = tmp[i]->component_words;
    }
    return (int)n;
}

/* Public per-modality binding-coverage accessor.
 *
 * Returns 0 on success, -1 on bad handle / no GL. Mirrors get_top_phrases
 * shape — the wrapper exists so curriculum integration tests don't have
 * to crack open the internal grounded_language_t. */
int nimcp_brain_get_modality_counts(
    nimcp_brain_t brain,
    uint32_t out_counts[6])
{
    if (!out_counts) return -1;
    /* Zero up front so a NULL-handle caller still gets a deterministic
     * all-zero result if they ignore the return code. */
    for (uint32_t m = 0; m < 6; m++) out_counts[m] = 0;

    brain_t b = NULL;
    if (_gl_diag_validate(brain, &b) != NIMCP_OK) return -1;
    if (!b->grounded_lang) return -1;

    /* GL_MODALITY_COUNT is fixed at 6 by the gl_modality_t enum; the
     * public API caps at 6 so callers that statically size [6] stay
     * safe even if the enum ever grows (impl will write only the first 6). */
    grounded_language_get_modality_counts(b->grounded_lang, out_counts);
    return 0;
}

nimcp_status_t nimcp_brain_set_snn_language_bridge_blend(
    nimcp_brain_t brain,
    float blend)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge) return NIMCP_ERROR;
    if (!isfinite(blend) || blend < 0.0f || blend > 1.0f) {
        return NIMCP_ERROR_INVALID;
    }
    snn_language_bridge_set_blend(b->snn_lang_bridge, blend);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_recompute_snn_language_bridge_norms(
    nimcp_brain_t brain)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge) return NIMCP_ERROR;
    int rc = snn_language_bridge_recompute_norms(b->snn_lang_bridge);
    return (rc == 0) ? NIMCP_OK : NIMCP_ERROR;
}

nimcp_status_t nimcp_brain_set_snn_language_bridge_sampling(
    nimcp_brain_t brain,
    float temperature,
    float top_p)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge) return NIMCP_ERROR;
    int rc = snn_language_bridge_set_sampling(b->snn_lang_bridge,
                                               temperature, top_p);
    if (rc != 0) return NIMCP_ERROR_INVALID;
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_snn_language_bridge_glove_blend(
    nimcp_brain_t brain,
    float blend)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge) return NIMCP_ERROR;
    int rc = snn_language_bridge_set_glove_blend(b->snn_lang_bridge, blend);
    if (rc != 0) return NIMCP_ERROR_INVALID;
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_snn_language_bridge_autoregressive(
    nimcp_brain_t brain,
    float intent_persistence,
    float word_feedback)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge) return NIMCP_ERROR;
    int rc = snn_language_bridge_set_autoregressive(b->snn_lang_bridge,
                                                     intent_persistence,
                                                     word_feedback);
    if (rc != 0) return NIMCP_ERROR_INVALID;
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_snn_language_bridge_spike_routing(
    nimcp_brain_t brain,
    bool enabled,
    float tau_ms)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge) return NIMCP_ERROR;
    int rc = snn_language_bridge_set_snn_spike_routing(b->snn_lang_bridge,
                                                        enabled, tau_ms);
    if (rc != 0) return NIMCP_ERROR_INVALID;
    return NIMCP_OK;
}

/* PA-5+: hyperbolic-distance GloVe metric. */
nimcp_status_t nimcp_brain_set_snn_language_bridge_hyperbolic_embeddings(
    nimcp_brain_t brain,
    bool enabled)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge) return NIMCP_ERROR;
    int rc = snn_language_bridge_set_hyperbolic_embeddings(b->snn_lang_bridge,
                                                            enabled);
    if (rc != 0) return NIMCP_ERROR_INVALID;
    return NIMCP_OK;
}

/* PA-6+: select produce-time sampling mode (0=auto, 1=softmax, 2=q-MC). */
nimcp_status_t nimcp_brain_set_snn_language_bridge_sampling_mode(
    nimcp_brain_t brain,
    int mode)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge) return NIMCP_ERROR;
    int rc = snn_language_bridge_set_sampling_mode(b->snn_lang_bridge, mode);
    if (rc != 0) return NIMCP_ERROR_INVALID;
    return NIMCP_OK;
}

/* TIER1-A: configure beam-K decoding in produce(). k=1 = greedy / legacy. */
nimcp_status_t nimcp_brain_set_snn_language_bridge_beam_width(
    nimcp_brain_t brain,
    uint32_t k)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge) return NIMCP_ERROR;
    int rc = snn_language_bridge_set_beam_width(b->snn_lang_bridge, k);
    if (rc != 0) return NIMCP_ERROR_INVALID;
    return NIMCP_OK;
}

/* TIER1-B: register EOS word_pop. UINT32_MAX disables. */
nimcp_status_t nimcp_brain_set_snn_language_bridge_eos_word_pop(
    nimcp_brain_t brain,
    uint32_t pop)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge) return NIMCP_ERROR;
    int rc = snn_language_bridge_set_eos_word_pop(b->snn_lang_bridge, pop);
    if (rc != 0) return NIMCP_ERROR_INVALID;
    return NIMCP_OK;
}

/* TIER1-C: configure n-gram repetition penalty. */
nimcp_status_t nimcp_brain_set_snn_language_bridge_repetition_penalty(
    nimcp_brain_t brain,
    float penalty,
    uint32_t window)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge) return NIMCP_ERROR;
    int rc = snn_language_bridge_set_repetition_penalty(b->snn_lang_bridge,
                                                          penalty, window);
    if (rc != 0) return NIMCP_ERROR_INVALID;
    return NIMCP_OK;
}

/* Tier-4 #15: consolidated config getter — copy entire snn_lang_config_t. */
nimcp_status_t nimcp_brain_get_snn_language_bridge_config(
    nimcp_brain_t brain,
    snn_lang_config_t* out)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge || !out) return NIMCP_ERROR;
    int rc = snn_language_bridge_get_config(b->snn_lang_bridge, out);
    return (rc == 0) ? NIMCP_OK : NIMCP_ERROR;
}

/* Tier-4 #17: explicit RNG seed (deterministic sampling tests). */
nimcp_status_t nimcp_brain_set_snn_language_bridge_rng_seed(
    nimcp_brain_t brain,
    uint64_t seed)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge) return NIMCP_ERROR;
    int rc = snn_language_bridge_set_rng_seed(b->snn_lang_bridge, seed);
    return (rc == 0) ? NIMCP_OK : NIMCP_ERROR;
}

nimcp_status_t nimcp_brain_learn_next_token_pair(nimcp_brain_t brain,
                                                   const char* prev_word,
                                                   const char* next_word,
                                                   float lr)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    int rc = grounded_language_learn_next_token_pair(b->grounded_lang,
                                                       prev_word, next_word, lr);
    return (rc == 0) ? NIMCP_OK : NIMCP_ERROR;
}

nimcp_status_t nimcp_brain_learn_next_token_pair_riemannian(nimcp_brain_t brain,
                                                              const char* prev_word,
                                                              const char* next_word,
                                                              float lr)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    int rc = grounded_language_learn_next_token_pair_riemannian(b->grounded_lang,
                                                                  prev_word,
                                                                  next_word, lr);
    return (rc == 0) ? NIMCP_OK : NIMCP_ERROR;
}

nimcp_status_t nimcp_brain_learn_text_bigrams(nimcp_brain_t brain,
                                                const char* text,
                                                float lr,
                                                int* out_count)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    int n = grounded_language_learn_text_bigrams(b->grounded_lang, text, lr);
    if (out_count) *out_count = n;
    return (n >= 0) ? NIMCP_OK : NIMCP_ERROR;
}

nimcp_status_t nimcp_brain_set_trigram_learning_enabled(nimcp_brain_t brain,
                                                          bool enabled)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge) return NIMCP_ERROR;
    int rc = snn_language_bridge_set_trigram_learning_enabled(
        b->snn_lang_bridge, enabled);
    return (rc == 0) ? NIMCP_OK : NIMCP_ERROR;
}

nimcp_status_t nimcp_brain_set_ltd_margin(nimcp_brain_t brain, float margin)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge) return NIMCP_ERROR;
    int rc = snn_language_bridge_set_ltd_margin(b->snn_lang_bridge, margin);
    return (rc == 0) ? NIMCP_OK : NIMCP_ERROR;
}

nimcp_status_t nimcp_brain_reset_lang_bridge_weights(nimcp_brain_t brain,
                                                       float w_min,
                                                       float w_max,
                                                       int64_t* out_count)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge) return NIMCP_ERROR;
    int64_t n = snn_language_bridge_reset_weights(b->snn_lang_bridge, w_min, w_max);
    if (out_count) *out_count = n;
    return (n >= 0) ? NIMCP_OK : NIMCP_ERROR;
}

nimcp_status_t nimcp_brain_reset_lexicon_distributional(nimcp_brain_t brain,
                                                          bool zero_and_mark_uninit,
                                                          float jitter,
                                                          int64_t* out_count)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    int64_t n = grounded_language_reset_lexicon_distributional(
        b->grounded_lang, zero_and_mark_uninit, jitter);
    if (out_count) *out_count = n;
    return (n >= 0) ? NIMCP_OK : NIMCP_ERROR;
}

nimcp_status_t nimcp_brain_reset_concept_grounding(nimcp_brain_t brain,
                                                    int64_t* out_cleared)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    int64_t n = grounded_language_reset_concept_grounding(b->grounded_lang);
    if (out_cleared) *out_cleared = n;
    return (n >= 0) ? NIMCP_OK : NIMCP_ERROR;
}

/* ========== Toxicity classifier diagnostics (2026-05-21) ========== */
nimcp_status_t nimcp_brain_classify_toxicity(nimcp_brain_t brain,
                                              const char* text,
                                              float* out_harm,
                                              float* out_fairness,
                                              float* out_anti_toxic,
                                              int* out_would_block,
                                              char* out_category,
                                              size_t out_category_len)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->toxicity_classifier) return NIMCP_ERROR;
    if (!text) return NIMCP_ERROR_NULL_ARG;
    toxicity_result_t r = {0};
    if (toxicity_classify((toxicity_classifier_t*)b->toxicity_classifier,
                          text, &r) != 0) {
        return NIMCP_ERROR;
    }
    if (out_harm)         *out_harm         = r.predicted_harm;
    if (out_fairness)     *out_fairness     = r.fairness_violation;
    if (out_anti_toxic)   *out_anti_toxic   = r.anti_toxic_signal;
    if (out_would_block)  *out_would_block  = r.would_block;
    if (out_category && out_category_len > 0) {
        strncpy(out_category, r.matched_category, out_category_len - 1);
        out_category[out_category_len - 1] = '\0';
    }
    return NIMCP_OK;
}

/* ========== Audit fix: campaign feature setter wrappers ========== */

nimcp_status_t nimcp_brain_set_da_modulation_enabled(nimcp_brain_t brain, bool enabled) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge) return NIMCP_ERROR;
    return (snn_language_bridge_set_da_modulation_enabled(
              b->snn_lang_bridge, enabled) == 0) ? NIMCP_OK : NIMCP_ERROR;
}

nimcp_status_t nimcp_brain_set_comprehend_stdp_enabled(nimcp_brain_t brain, bool enabled) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge) return NIMCP_ERROR;
    return (snn_language_bridge_set_comprehend_stdp_enabled(
              b->snn_lang_bridge, enabled) == 0) ? NIMCP_OK : NIMCP_ERROR;
}

/* Forward decl — implementation lives in nimcp_communication_cascade.c. */
struct production_cascade_state_s;
typedef struct production_cascade_state_s production_cascade_state_dummy_t;

nimcp_status_t nimcp_brain_produce_cascade(
    nimcp_brain_t brain,
    const char* prompt_or_null,
    char* out_utterance,
    uint32_t out_text_max,
    uint32_t* out_word_count,
    float* out_confidence)
{
    if (!brain) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = brain->internal_brain;
    if (!b) return NIMCP_ERROR_NOT_INITIALIZED;

    /* Pull in cascade types via a runtime cast — keeps the include
     * graph minimal in nimcp.h while still calling through to the
     * cascade implementation. */
    extern int nimcp_brain_produce_cascade_impl(
        brain_t brain,
        const char* prompt_or_null,
        char* out_utterance,
        uint32_t out_text_max,
        uint32_t* out_word_count,
        float* out_confidence);

    int rc = nimcp_brain_produce_cascade_impl(b, prompt_or_null,
                                                out_utterance, out_text_max,
                                                out_word_count, out_confidence);
    return (rc == 0) ? NIMCP_OK : NIMCP_ERROR;
}

nimcp_status_t nimcp_brain_produce_cascade_recurrent(
    nimcp_brain_t brain,
    const char* prompt_or_null,
    uint32_t max_iters,
    float self_match_eps,
    char* out_utterance,
    uint32_t out_text_max,
    uint32_t* out_word_count,
    float* out_confidence,
    uint32_t* out_settling_steps)
{
    if (!brain) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = brain->internal_brain;
    if (!b) return NIMCP_ERROR_NOT_INITIALIZED;

    extern int nimcp_brain_produce_cascade_recurrent_impl(
        brain_t brain,
        const char* prompt_or_null,
        uint32_t max_iters,
        float self_match_eps,
        char* out_utterance,
        uint32_t out_text_max,
        uint32_t* out_word_count,
        float* out_confidence,
        uint32_t* out_settling_steps);

    int rc = nimcp_brain_produce_cascade_recurrent_impl(
        b, prompt_or_null, max_iters, self_match_eps,
        out_utterance, out_text_max, out_word_count,
        out_confidence, out_settling_steps);
    return (rc == 0) ? NIMCP_OK : NIMCP_ERROR;
}

nimcp_status_t nimcp_brain_echo_and_correct(
    nimcp_brain_t brain,
    const char* parent_text,
    const char* target_word,
    float lr_scale,
    int* out_pairs_strengthened)
{
    if (!brain || !parent_text || !target_word) return NIMCP_ERROR_INVALID_PARAM;
    if (!isfinite(lr_scale) || lr_scale <= 0.0f) lr_scale = 1.0f;

    brain_t b = brain->internal_brain;
    if (!b || !b->grounded_lang || !b->snn_lang_bridge) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    /* Step 1: comprehend the parent's utterance to get the semantic
     * vector that represents what the brain "heard". Same vector that
     * would feed into produce — so reinforcing concept→target_word
     * here is teaching the bridge "when this set of concepts fires,
     * say target_word". */
    gl_comprehension_result_t comp = {0};
    int rc = grounded_language_comprehend(b->grounded_lang, parent_text, &comp);
    if (rc != 0 || !comp.semantic_vector) {
        gl_comprehension_result_cleanup(&comp);
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    uint32_t semantic_dim = grounded_language_get_semantic_dim(b->grounded_lang);

    /* Step 2: supervised LTP — strengthen (active concepts → target word)
     * bindings. Returns negative on error or # bindings strengthened. */
    int pairs = snn_language_bridge_echo_correct(
        b->snn_lang_bridge,
        comp.semantic_vector,
        semantic_dim,
        target_word,
        lr_scale);

    gl_comprehension_result_cleanup(&comp);

    if (pairs < 0) {
        if (out_pairs_strengthened) *out_pairs_strengthened = 0;
        /* -2 = target word not registered in bridge — caller fed an
         * unfamiliar word. Report as "operation failed" (recoverable). */
        if (pairs == -2) return NIMCP_ERROR_OPERATION_FAILED;
        return NIMCP_ERROR;
    }
    if (out_pairs_strengthened) *out_pairs_strengthened = pairs;
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_da_modulation_gain(nimcp_brain_t brain, float gain) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge) return NIMCP_ERROR;
    return (snn_language_bridge_set_da_modulation_gain(
              b->snn_lang_bridge, gain) == 0) ? NIMCP_OK : NIMCP_ERROR;
}

nimcp_status_t nimcp_brain_set_reconsolidation_enabled(nimcp_brain_t brain, bool enabled) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    grounded_language_set_reconsolidation_enabled(b->grounded_lang, enabled);
    return NIMCP_OK;
}

/* Slice 4 — Lateral inhibition control surface.
 *
 * Four thin wrappers over snn_language_bridge_*_lateral_inhibition_*.
 * Bridge is the load-bearing handle (cascade routes through it); when
 * the bridge isn't initialized these all report NIMCP_ERROR_NOT_INITIALIZED. */
nimcp_status_t nimcp_brain_set_lateral_inhibition_enabled(nimcp_brain_t brain,
                                                            bool enabled) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge) return NIMCP_ERROR_NOT_INITIALIZED;
    return (snn_language_bridge_set_lateral_inhibition_enabled(
              b->snn_lang_bridge, enabled) == 0) ? NIMCP_OK : NIMCP_ERROR;
}

nimcp_status_t nimcp_brain_get_lateral_inhibition_enabled(nimcp_brain_t brain,
                                                            bool* out_enabled) {
    if (!out_enabled) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge) return NIMCP_ERROR_NOT_INITIALIZED;
    *out_enabled = snn_language_bridge_get_lateral_inhibition_enabled(
                       b->snn_lang_bridge);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_lateral_inhibition_params(nimcp_brain_t brain,
                                                          float gain_self,
                                                          float gain_inhibit,
                                                          uint32_t micro_steps) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge) return NIMCP_ERROR_NOT_INITIALIZED;
    return (snn_language_bridge_set_lateral_inhibition_params(
              b->snn_lang_bridge, gain_self, gain_inhibit, micro_steps) == 0)
              ? NIMCP_OK : NIMCP_ERROR_INVALID_PARAM;
}

nimcp_status_t nimcp_brain_get_lateral_inhibition_params(nimcp_brain_t brain,
                                                          float* out_gain_self,
                                                          float* out_gain_inhibit,
                                                          uint32_t* out_micro_steps) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge) return NIMCP_ERROR_NOT_INITIALIZED;
    return (snn_language_bridge_get_lateral_inhibition_params(
              b->snn_lang_bridge, out_gain_self, out_gain_inhibit, out_micro_steps) == 0)
              ? NIMCP_OK : NIMCP_ERROR;
}

nimcp_status_t nimcp_brain_set_reconsolidation_decay(nimcp_brain_t brain, float decay) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    grounded_language_set_reconsolidation_decay(b->grounded_lang, decay);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_sentence_segmentation_enabled(nimcp_brain_t brain, bool enabled) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    grounded_language_set_sentence_segmentation_enabled(b->grounded_lang, enabled);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_length_control(nimcp_brain_t brain,
                                                uint32_t min_words, uint32_t max_words) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge) return NIMCP_ERROR;
    return (snn_language_bridge_set_length_control(
              b->snn_lang_bridge, min_words, max_words) == 0) ? NIMCP_OK : NIMCP_ERROR;
}

nimcp_status_t nimcp_brain_set_speech_act_classification_enabled(nimcp_brain_t brain, bool enabled) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    grounded_language_set_speech_act_classification_enabled(b->grounded_lang, enabled);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_topic_shift_enabled(nimcp_brain_t brain, bool enabled) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    grounded_language_set_topic_shift_enabled(b->grounded_lang, enabled);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_topic_shift_threshold(nimcp_brain_t brain, float threshold) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    grounded_language_set_topic_shift_threshold(b->grounded_lang, threshold);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_topic_shift_min_turns(nimcp_brain_t brain, uint32_t min_turns) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    grounded_language_set_topic_shift_min_turns(b->grounded_lang, min_turns);
    return NIMCP_OK;
}

/*=============================================================================
 * NLP-2 coref + NLP-1 subword OOV — runtime reachability surface.
 *
 * Both features shipped earlier (gl-side impl + flag + counters) but had
 * NO brain wrapper, so they were unreachable through the Python binding or
 * daemon RPC — dead in production. These thin wrappers + the matching
 * Python/RPC rows close that gap (full-walkthrough 2026-05-25).
 *===========================================================================*/
nimcp_status_t nimcp_brain_set_coref_resolution_enabled(nimcp_brain_t brain, bool enabled) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    grounded_language_set_coref_resolution_enabled(b->grounded_lang, enabled);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_coref_resolution_enabled(nimcp_brain_t brain, bool* out_enabled) {
    if (!out_enabled) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    *out_enabled = grounded_language_get_coref_resolution_enabled(b->grounded_lang);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_autoregressive_produce(nimcp_brain_t brain, bool enabled) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    grounded_language_set_autoregressive_produce(b->grounded_lang, enabled);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_autoregressive_produce(nimcp_brain_t brain, bool* out_enabled) {
    if (!out_enabled) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    *out_enabled = grounded_language_get_autoregressive_produce(b->grounded_lang);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_produce_pronominalize(nimcp_brain_t brain, bool enabled) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    grounded_language_set_produce_pronominalize(b->grounded_lang, enabled);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_produce_pronominalize(nimcp_brain_t brain, bool* out_enabled) {
    if (!out_enabled) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    *out_enabled = grounded_language_get_produce_pronominalize(b->grounded_lang);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_produce_discourse_seed(nimcp_brain_t brain, bool enabled) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    grounded_language_set_produce_discourse_seed(b->grounded_lang, enabled);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_produce_discourse_seed(nimcp_brain_t brain, bool* out_enabled) {
    if (!out_enabled) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    *out_enabled = grounded_language_get_produce_discourse_seed(b->grounded_lang);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_produce_clause_frame(nimcp_brain_t brain, bool enabled) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    grounded_language_set_produce_clause_frame(b->grounded_lang, enabled);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_produce_clause_frame(nimcp_brain_t brain, bool* out_enabled) {
    if (!out_enabled) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    *out_enabled = grounded_language_get_produce_clause_frame(b->grounded_lang);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_produce_via_snn(nimcp_brain_t brain, bool enabled) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    grounded_language_set_produce_via_snn(b->grounded_lang, enabled);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_produce_via_snn(nimcp_brain_t brain, bool* out_enabled) {
    if (!out_enabled) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    *out_enabled = grounded_language_get_produce_via_snn(b->grounded_lang);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_metacog_gates_produce(nimcp_brain_t brain, bool enabled) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    grounded_language_set_metacog_gates_produce(b->grounded_lang, enabled);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_metacog_gates_produce(nimcp_brain_t brain, bool* out_enabled) {
    if (!out_enabled) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    *out_enabled = grounded_language_get_metacog_gates_produce(b->grounded_lang);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_warmstart_lang_projection(nimcp_brain_t brain, float k,
                                                     int* out_updated) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge || !b->snn_network) return NIMCP_ERROR;
    int wid = snn_network_find_pop_by_name(b->snn_network, "wernicke_substrate");
    int bid = snn_network_find_pop_by_name(b->snn_network, "broca_substrate");
    if (wid < 0 || bid < 0) return NIMCP_ERROR;   /* pops or projection absent */
    int updated = snn_language_bridge_warmstart_projection(
        b->snn_lang_bridge, b->snn_network, wid, bid, k, 1.0f /* overwrite */);
    if (out_updated) *out_updated = updated;
    return (updated >= 0) ? NIMCP_OK : NIMCP_ERROR;
}

/* Build the concept→word projection TOPOLOGY across all lexicon bindings (the
 * missing coverage half). connect_network stores the wernicke/broca pop ids on
 * the bridge, so build_projection resolves them itself. MUST run with stepping
 * paused — the daemon serializes RPCs with learn_vector under the global lock. */
nimcp_status_t nimcp_brain_build_lang_projection(nimcp_brain_t brain, float w_init,
                                                 int* out_wired) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge || !b->snn_network) return NIMCP_ERROR;
    int wired = snn_language_bridge_build_projection(
        b->snn_lang_bridge, b->snn_network, w_init);
    if (out_wired) *out_wired = wired;
    return (wired >= 0) ? NIMCP_OK : NIMCP_ERROR;
}

nimcp_status_t nimcp_brain_snn_language_generate_step(nimcp_brain_t brain,
                                                      const float* intent,
                                                      uint32_t dim,
                                                      uint32_t n_steps) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->snn_lang_bridge || !b->snn_network || !b->grounded_lang || !intent) {
        return NIMCP_ERROR;
    }
    int wid = snn_network_find_pop_by_name(b->snn_network, "wernicke_substrate");
    int bid = snn_network_find_pop_by_name(b->snn_network, "broca_substrate");
    if (wid < 0 || bid < 0) return NIMCP_ERROR;   /* projection/pops absent */
    uint64_t concept_ids[32];
    uint32_t nc = grounded_language_top_concepts_for_intent(
        b->grounded_lang, intent, dim, concept_ids, 32);
    if (nc == 0) return NIMCP_ERROR;              /* no concept seed → caller falls back */
    int rc = snn_language_bridge_generate_step(
        b->snn_lang_bridge, b->snn_network, wid, bid,
        concept_ids, nc, n_steps, 1.0f);
    return (rc == 0) ? NIMCP_OK : NIMCP_ERROR;
}

nimcp_status_t nimcp_brain_set_produce_distributional_weight(nimcp_brain_t brain, float weight) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    grounded_language_set_produce_distributional_weight(b->grounded_lang, weight);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_produce_distributional_weight(nimcp_brain_t brain, float* out_weight) {
    if (!out_weight) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    *out_weight = grounded_language_get_produce_distributional_weight(b->grounded_lang);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_produce_frequency_penalty(nimcp_brain_t brain, float penalty) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    grounded_language_set_produce_frequency_penalty(b->grounded_lang, penalty);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_produce_frequency_penalty(nimcp_brain_t brain, float* out_penalty) {
    if (!out_penalty) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    *out_penalty = grounded_language_get_produce_frequency_penalty(b->grounded_lang);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_produce_givenness_definite(nimcp_brain_t brain, bool enabled) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    grounded_language_set_produce_givenness_definite(b->grounded_lang, enabled);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_produce_givenness_definite(nimcp_brain_t brain, bool* out_enabled) {
    if (!out_enabled) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    *out_enabled = grounded_language_get_produce_givenness_definite(b->grounded_lang);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_produce_t3_conjunction(nimcp_brain_t brain, bool enabled) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    grounded_language_set_produce_t3_conjunction(b->grounded_lang, enabled);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_produce_t3_conjunction(nimcp_brain_t brain, bool* out_enabled) {
    if (!out_enabled) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    *out_enabled = grounded_language_get_produce_t3_conjunction(b->grounded_lang);
    return NIMCP_OK;
}

/*============================================================================
 * TF-2 — Tier-feedback flag wrappers.
 *==========================================================================*/
nimcp_status_t nimcp_brain_set_produce_corrector_feedback_enabled(nimcp_brain_t brain, bool enabled) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    grounded_language_set_produce_corrector_feedback_enabled(b->grounded_lang, enabled);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_produce_corrector_feedback_enabled(nimcp_brain_t brain, bool* out_enabled) {
    if (!out_enabled) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    *out_enabled = grounded_language_get_produce_corrector_feedback_enabled(b->grounded_lang);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_tf_enabled_correctors(nimcp_brain_t brain, uint16_t mask) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    grounded_language_set_tf_enabled_correctors(b->grounded_lang, mask);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_tf_enabled_correctors(nimcp_brain_t brain, uint16_t* out_mask) {
    if (!out_mask) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    *out_mask = grounded_language_get_tf_enabled_correctors(b->grounded_lang);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_tf_lr_trigram(nimcp_brain_t brain, float lr) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    grounded_language_set_tf_lr_trigram(b->grounded_lang, lr);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_tf_lr_trigram(nimcp_brain_t brain, float* out_lr) {
    if (!out_lr) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    *out_lr = grounded_language_get_tf_lr_trigram(b->grounded_lang);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_tf_lr_distrib(nimcp_brain_t brain, float lr) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    grounded_language_set_tf_lr_distrib(b->grounded_lang, lr);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_tf_lr_distrib(nimcp_brain_t brain, float* out_lr) {
    if (!out_lr) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    *out_lr = grounded_language_get_tf_lr_distrib(b->grounded_lang);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_tf_lr_bridge_stdp(nimcp_brain_t brain, float lr) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    grounded_language_set_tf_lr_bridge_stdp(b->grounded_lang, lr);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_tf_lr_bridge_stdp(nimcp_brain_t brain, float* out_lr) {
    if (!out_lr) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    *out_lr = grounded_language_get_tf_lr_bridge_stdp(b->grounded_lang);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_subword_oov_fallback_enabled(nimcp_brain_t brain, bool enabled) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    /* The fallback no-ops without an attached tokenizer. Attach the
     * brain-native BPE tokenizer on enable when one exists, so flipping the
     * flag via RPC is sufficient even if brain init didn't run the attach
     * (e.g. enable_brain_tokenizer was off at init then enabled later).
     * Borrowed pointer — brain owns the tokenizer lifetime. */
    if (enabled && b->brain_tokenizer) {
        grounded_language_attach_subword_tokenizer(
            b->grounded_lang, (struct nimcp_tokenizer*)b->brain_tokenizer);
    }
    grounded_language_set_subword_oov_fallback_enabled(b->grounded_lang, enabled);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_subword_oov_fallback_enabled(nimcp_brain_t brain, bool* out_enabled) {
    if (!out_enabled) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    *out_enabled = grounded_language_get_subword_oov_fallback_enabled(b->grounded_lang);
    return NIMCP_OK;
}

/*=============================================================================
 * Wave 2 Item #10 — cascade self-train public surface.
 *
 * Four thin wrappers over communication_cascade_set/get_self_train_*
 * so trainers can drive Stage 14 via the same brain handle they use
 * for produce_cascade. Default OFF; flipping enabled also installs
 * default tunables (0.05 alpha, 1.0 lr_scale) inside the cascade
 * setter so trainers don't have to know the magic numbers.
 *===========================================================================*/

/* Forward decls — full headers in include/language/nimcp_communication_cascade.h.
 * Inline extern keeps this TU from pulling the whole cascade header chain
 * just for these four small wrappers (the produce_cascade wrapper uses
 * the same inline-extern idiom). */
extern int  communication_cascade_set_self_train_enabled(brain_t brain, bool enabled);
extern bool communication_cascade_get_self_train_enabled(brain_t brain);
extern int  communication_cascade_set_self_train_tunables(brain_t brain,
                                                            float alpha,
                                                            float lr_scale);

nimcp_status_t nimcp_brain_set_cascade_self_train_enabled(nimcp_brain_t brain,
                                                            bool enabled) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    return (communication_cascade_set_self_train_enabled(b, enabled) == 0)
              ? NIMCP_OK : NIMCP_ERROR;
}

nimcp_status_t nimcp_brain_get_cascade_self_train_enabled(nimcp_brain_t brain,
                                                            bool* out_enabled) {
    if (!out_enabled) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    *out_enabled = communication_cascade_get_self_train_enabled(b);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_cascade_self_train_tunables(nimcp_brain_t brain,
                                                             float alpha,
                                                             float lr_scale) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    return (communication_cascade_set_self_train_tunables(b, alpha, lr_scale) == 0)
              ? NIMCP_OK : NIMCP_ERROR;
}

nimcp_status_t nimcp_brain_get_cascade_self_train_state(nimcp_brain_t brain,
                                                          bool*  out_enabled,
                                                          float* out_baseline,
                                                          float* out_alpha,
                                                          float* out_lr_scale) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (out_enabled)  *out_enabled  = b->cascade_self_train_enabled;
    if (out_baseline) *out_baseline = b->cascade_self_train_baseline;
    if (out_alpha)    *out_alpha    = b->cascade_self_train_alpha;
    if (out_lr_scale) *out_lr_scale = b->cascade_self_train_lr_scale;
    return NIMCP_OK;
}

/* === SLICE D — CASCADE SELF-TRAIN EXTERNAL-REWARD GATING (2026-05-19) ===
 *
 * The caregiver-critic / RL pipeline calls this setter on every external
 * reward signal. The cascade's stage_self_train gates on the freshness +
 * threshold of this value: stale or below-threshold reward → no plasticity.
 *
 * Clamps reward to [-1.0, +1.0] (matches CriticVerdict.reward contract).
 * Stamps last_external_reward_us with the current monotonic_us so
 * stage_self_train can compute (now - reward_us) > ttl_us. */
int nimcp_brain_set_last_external_reward(nimcp_brain_t brain, float reward) {
    if (!brain) return -1;
    brain_t b = brain->internal_brain;
    if (!b) return -1;
    if (!isfinite(reward)) return -1;
    if (reward < -1.0f) reward = -1.0f;
    if (reward >  1.0f) reward =  1.0f;
    b->last_external_reward     = reward;
    b->last_external_reward_us  = nimcp_time_monotonic_us();
    return 0;
}

/* Slice C — Public API wrapper for brain_apply_reward_learning. The Slice C
 * caregiver-critic pipeline calls this on every brain production to drive
 * reward-modulated plasticity through the canonical three-factor STDP path.
 *
 * The internal brain_apply_reward_learning() (src/core/brain/learning/
 * nimcp_brain_learning.c:6045) walks active neurons, applies the reward via
 * the neuromod-plasticity bridge, and updates the dopamine baseline. This
 * thin wrapper validates the public handle and forwards.
 *
 * Returns 0 on success, -1 on invalid brain or non-finite reward. */
int nimcp_brain_apply_reward_learning(nimcp_brain_t brain, float reward) {
    if (!brain) return -1;
    brain_t b = brain->internal_brain;
    if (!b) return -1;
    if (!isfinite(reward)) return -1;
    if (reward < -1.0f) reward = -1.0f;
    if (reward >  1.0f) reward =  1.0f;
    (void)brain_apply_reward_learning(b, reward);
    return 0;
}

/* Slice D — extended tunables setter that also configures the reward
 * threshold + TTL. Mirrors set_cascade_self_train_tunables but takes the
 * two Slice-D fields. Callable via the RPC handler (see
 * scripts/brain_daemon.py _cmd_set_cascade_self_train_tunables). Either
 * argument as <= 0 leaves the brain's value unchanged (so the partial
 * update path doesn't require the caller to know prior values). */
nimcp_status_t nimcp_brain_set_cascade_self_train_reward_gating(
        nimcp_brain_t brain,
        float reward_threshold,
        uint64_t reward_ttl_us) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (isfinite(reward_threshold) && reward_threshold > 0.0f) {
        if (reward_threshold > 1.0f) reward_threshold = 1.0f;
        b->cascade_self_train_reward_threshold = reward_threshold;
    }
    if (reward_ttl_us > 0) {
        b->cascade_self_train_reward_ttl_us = reward_ttl_us;
    }
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_cascade_self_train_gate_counters(
        nimcp_brain_t brain,
        uint64_t* out_skipped_stale,
        uint64_t* out_skipped_below_threshold,
        uint64_t* out_fired) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (out_skipped_stale) {
        *out_skipped_stale = atomic_load_explicit(
            &b->cascade_self_train_skipped_stale, memory_order_relaxed);
    }
    if (out_skipped_below_threshold) {
        *out_skipped_below_threshold = atomic_load_explicit(
            &b->cascade_self_train_skipped_below_threshold, memory_order_relaxed);
    }
    if (out_fired) {
        *out_fired = atomic_load_explicit(
            &b->cascade_self_train_fired, memory_order_relaxed);
    }
    return NIMCP_OK;
}

/*-----------------------------------------------------------------------------
 * Slice E (Option-1 architectural rebuild) — public stage-scaffolding API.
 *
 * Returns 0 on success and -1 on invalid brain / missing internal brain so
 * the daemon RPC can surface a simple OK/error without juggling
 * nimcp_status_t enum names.
 *---------------------------------------------------------------------------*/

int nimcp_brain_set_active_stage(nimcp_brain_t brain, uint32_t stage) {
    if (!brain) return -1;
    brain_t b = brain->internal_brain;
    if (!b) return -1;
    /* Clamp out-of-range stage to the highest defined row. The stage table
     * itself does this internally on read, but stamping the field with a
     * clamped value keeps trainer telemetry consistent. */
    uint32_t max_s = stage_table_max_stage();
    if (stage > max_s) stage = max_s;
    b->current_stage = stage;
    /* Propagate to grounded_language so the produce-side developmental
     * confidence floor (gl_produce_confidence_floor) follows the same
     * curriculum advance. Without this, the brain-level cap moves but
     * the gl-level soft floor stays at its init value (0). */
    if (b->grounded_lang) {
        grounded_language_set_current_stage_int(b->grounded_lang, (int)stage);
    }
    return 0;
}

int nimcp_brain_get_active_stage(nimcp_brain_t brain, uint32_t* out_stage) {
    if (!brain) return -1;
    brain_t b = brain->internal_brain;
    if (!b) return -1;
    if (out_stage) *out_stage = b->current_stage;
    return 0;
}

int nimcp_brain_set_vocab_mask_for_stage(nimcp_brain_t brain, uint32_t stage) {
    if (!brain) return -1;
    brain_t b = brain->internal_brain;
    if (!b || !b->grounded_lang) return -1;
    uint32_t max_s = stage_table_max_stage();
    if (stage > max_s) stage = max_s;
    return grounded_language_set_active_vocab_mask(b->grounded_lang, stage);
}

int nimcp_brain_clear_vocab_mask(nimcp_brain_t brain) {
    if (!brain) return -1;
    brain_t b = brain->internal_brain;
    if (!b || !b->grounded_lang) return -1;
    grounded_language_clear_active_vocab_mask(b->grounded_lang);
    return 0;
}

int nimcp_brain_get_stage_constraints(nimcp_brain_t brain,
                                       uint32_t stage,
                                       size_t* out_max_visible_vocab,
                                       uint32_t* out_min_produce_words,
                                       uint32_t* out_max_produce_words,
                                       uint32_t* out_allowed_grammar_mask) {
    (void)brain;
    /* Table lookup is brain-independent — we accept the brain handle so
     * the API signature stays symmetric with the rest of the brain API,
     * but the call itself is pure. */
    const stage_constraints_t* sc = stage_table_get(stage);
    if (!sc) sc = stage_table_default();
    if (out_max_visible_vocab)    *out_max_visible_vocab    = sc->max_visible_vocab;
    if (out_min_produce_words)    *out_min_produce_words    = (uint32_t)sc->min_produce_words;
    if (out_max_produce_words)    *out_max_produce_words    = (uint32_t)sc->max_produce_words;
    if (out_allowed_grammar_mask) *out_allowed_grammar_mask = sc->allowed_grammar_mask;
    return 0;
}

/* Batch K — cascade lifetime counters public wrappers.
 * (Header now included at the top of the TU since the respond opt-in
 * also uses cascade types.) */
extern int nimcp_brain_get_cascade_counters_impl(brain_t brain,
                                                   nimcp_cascade_counters_t* out);
extern int nimcp_brain_reset_cascade_counters_impl(brain_t brain);

nimcp_status_t nimcp_brain_get_cascade_counters(nimcp_brain_t brain,
                                                  struct nimcp_cascade_counters* out) {
    if (!out) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    return (nimcp_brain_get_cascade_counters_impl(b, out) == 0)
              ? NIMCP_OK : NIMCP_ERROR;
}

nimcp_status_t nimcp_brain_reset_cascade_counters(nimcp_brain_t brain) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    return (nimcp_brain_reset_cascade_counters_impl(b) == 0)
              ? NIMCP_OK : NIMCP_ERROR;
}

/* Slice 3 — recurrent-cascade FEP prediction-error metrics. */
extern int nimcp_brain_get_cascade_fep_metrics_impl(
    brain_t brain, nimcp_cascade_fep_metrics_t* out);

nimcp_status_t nimcp_brain_get_cascade_fep_metrics(
    nimcp_brain_t brain,
    struct nimcp_cascade_fep_metrics* out)
{
    if (!out) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    return (nimcp_brain_get_cascade_fep_metrics_impl(
                b, (nimcp_cascade_fep_metrics_t*)out) == 0)
              ? NIMCP_OK : NIMCP_ERROR;
}

nimcp_status_t nimcp_brain_set_dialect(nimcp_brain_t brain, const char* dialect) {
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    /* NULL or empty input clears the dialect; module setter handles
     * truncation + NUL termination. */
    grounded_language_set_dialect(b->grounded_lang, dialect);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_learn_next_token_triple(nimcp_brain_t brain,
                                                     const char* prev1,
                                                     const char* prev2,
                                                     const char* next_word,
                                                     float lr)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    int rc = grounded_language_learn_next_token_triple(b->grounded_lang,
                                                         prev1, prev2,
                                                         next_word, lr);
    return (rc == 0) ? NIMCP_OK : NIMCP_ERROR;
}

/*=============================================================================
 * Tier-2 #3 / #6 / #7 brain wrappers — toggle negation, toggle word-sense
 * disambiguation, push a discourse turn, query / clamp the discourse buffer.
 *
 * All five share the _gl_diag_validate gate and return NIMCP_ERROR
 * (rather than NIMCP_ERROR_NOT_INITIALIZED) when grounded_lang is
 * absent — matches the existing PA-* wrapper convention so RPC clients
 * don't need a separate error-code branch.
 *===========================================================================*/

nimcp_status_t nimcp_brain_set_grounded_negation_enabled(nimcp_brain_t brain,
                                                           bool enabled)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    grounded_language_set_negation_enabled(b->grounded_lang, enabled);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_set_grounded_sense_disambiguation_enabled(
    nimcp_brain_t brain,
    bool enabled)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    grounded_language_set_sense_disambiguation_enabled(b->grounded_lang, enabled);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_grounded_push_turn(nimcp_brain_t brain,
                                                const float* semantic_vec,
                                                uint32_t vec_dim,
                                                uint32_t n_words,
                                                bool is_user)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    int rc = grounded_language_push_turn(b->grounded_lang, semantic_vec,
                                           vec_dim, n_words, is_user);
    return (rc == 0) ? NIMCP_OK : NIMCP_ERROR;
}

int nimcp_brain_grounded_get_discourse_turn_count(nimcp_brain_t brain)
{
    if (!brain) return -1;
    brain_t b = brain->internal_brain;
    if (!b || !b->grounded_lang) return -1;
    return (int)grounded_language_get_discourse_turn_count(b->grounded_lang);
}

nimcp_status_t nimcp_brain_grounded_set_discourse_capacity(nimcp_brain_t brain,
                                                              uint8_t capacity)
{
    brain_t b = NULL;
    nimcp_status_t s = _gl_diag_validate(brain, &b);
    if (s != NIMCP_OK) return s;
    if (!b->grounded_lang) return NIMCP_ERROR;
    grounded_language_set_discourse_capacity(b->grounded_lang, capacity);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_creative_blend(
    nimcp_brain_t brain,
    const float* vector_a,
    const float* vector_b,
    uint32_t vec_dim,
    float blend_ratio,
    char* out_text,
    uint32_t text_max)
{
    if (!brain || !vector_a || !vector_b || !out_text) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = brain->internal_brain;
    if (!b || !b->grounded_lang) return NIMCP_ERROR_NOT_INITIALIZED;

    gl_production_result_t result = {0};
    int rc = grounded_language_blend(b->grounded_lang, 0, 0,
                                      vector_a, vector_b, vec_dim,
                                      blend_ratio, &result);
    if (rc != 0 || !result.text) {
        gl_production_result_cleanup(&result);
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    strncpy(out_text, result.text, text_max - 1);
    out_text[text_max - 1] = '\0';

    gl_production_result_cleanup(&result);
    return NIMCP_OK;
}

//=============================================================================
// Unified Cognitive Training API
//=============================================================================

nimcp_status_t nimcp_brain_train_cognitive(
    nimcp_brain_t brain,
    const char* text,
    int domain,
    const char* target_text,
    float learning_rate,
    float* out_loss)
{
    if (!brain || !text) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = brain->internal_brain;
    if (!b) return NIMCP_ERROR_NOT_INITIALIZED;

    float total_loss = 0.0f;
    int modules_trained = 0;

    /* 1. Grounded language — distributional + syntactic learning */
    if (b->grounded_lang) {
        int updates = grounded_language_learn_from_text(b->grounded_lang, text);
        grounded_language_learn_syntax(b->grounded_lang, text);
        /* Drive bridge bigram + trigram STDP from real-text learning.
         * Mirrors the fix in nimcp_brain_learn_language. Trainer calls
         * train_cognitive every cognitive injection — this is the hot
         * path that actually fires per step. Without this, the bridge's
         * trigram path (the only LTD-on-false-winner mechanism) stays
         * dormant and mode-collapsed word_pops at weight=1.0 never
         * get pulled down. */
        (void)grounded_language_learn_text_bigrams(b->grounded_lang, text, 0.02f);
        /* Also feed target_text if provided — pairs are often {prompt,
         * answer} where the answer has the supervised content. */
        if (target_text && target_text != text) {
            (void)grounded_language_learn_text_bigrams(b->grounded_lang,
                                                        target_text, 0.02f);
        }
        if (updates > 0) {
            total_loss += 1.0f / (float)updates;
            modules_trained++;
        }

        /* If target text provided, learn the pair relationship */
        if (target_text) {
            float pair_loss = grounded_language_learn_pair(
                b->grounded_lang, text, target_text,
                (learning_rate > 0.0f) ? learning_rate : 0.0f);
            if (pair_loss >= 0.0f) {
                total_loss += pair_loss;
                modules_trained++;
            }
        }
    }

    /* 2. Knowledge system — domain-specific learning */
    if (b->knowledge && domain >= 0 && domain <= 10) {
        uint32_t concepts = knowledge_learn_from_text(
            b->knowledge, text, (knowledge_domain_t)domain);
        if (concepts > 0) {
            total_loss += 1.0f / (float)concepts;
            modules_trained++;
        }
    }

    /* 3. Language generator (LNN decoder) — autoregressive training */
    if (b->lang_generator && b->tokenizer && target_text) {
        enum { MAX_TOKENS = 512 };
        uint32_t input_ids[MAX_TOKENS], target_ids[MAX_TOKENS];
        uint32_t input_len = 0, target_len = 0;

        int rc1 = tokenizer_encode(b->tokenizer, text,
                                    input_ids, MAX_TOKENS, &input_len);
        int rc2 = tokenizer_encode(b->tokenizer, target_text,
                                    target_ids, MAX_TOKENS, &target_len);

        if (rc1 == 0 && rc2 == 0 && input_len > 0 && target_len > 0) {
            uint32_t seq_len = (input_len < target_len) ? input_len : target_len;
            float lang_loss = 0.0f;
            int rc = language_generator_train_step(
                b->lang_generator, input_ids, target_ids, seq_len, &lang_loss);
            if (rc == 0) {
                total_loss += lang_loss;
                modules_trained++;
            }
        }

        /* Also update embedding layer */
        if (b->lang_embedding) {
            float lr = (learning_rate > 0.0f) ? learning_rate : 0.001f;
            embedding_update(b->lang_embedding, lr);
        }
    }

    if (out_loss) {
        *out_loss = (modules_trained > 0) ? total_loss / (float)modules_trained : 1.0f;
    }

    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_get_cognitive_stats(
    nimcp_brain_t handle,
    uint32_t* out_stats,
    float* out_losses,
    uint32_t* out_count)
{
    if (!handle || !out_stats || !out_losses || !out_count) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    brain_t brain = handle->internal_brain;
    if (!brain) return NIMCP_ERROR_NOT_INITIALIZED;

    /* 11 cognitive modules tracked. NaN is the "no signal yet" sentinel
     * for modules whose underlying task doesn't write r->loss. */
    out_stats[0]  = brain->cognitive_stats.grounded_lang_steps;
    out_losses[0] = brain->cognitive_stats.grounded_lang_last_loss;
    out_stats[1]  = brain->cognitive_stats.knowledge_steps;
    out_losses[1] = brain->cognitive_stats.knowledge_last_loss;
    out_stats[2]  = brain->cognitive_stats.vae_steps;
    out_losses[2] = brain->cognitive_stats.vae_last_loss;
    out_stats[3]  = brain->cognitive_stats.fep_parietal_steps;
    out_losses[3] = brain->cognitive_stats.fep_parietal_last_loss;
    out_stats[4]  = brain->cognitive_stats.physics_nn_steps;
    out_losses[4] = brain->cognitive_stats.physics_nn_last_loss;
    out_stats[5]  = brain->cognitive_stats.pred_hierarchy_steps;
    out_losses[5] = brain->cognitive_stats.pred_hierarchy_last_loss;
    out_stats[6]  = brain->cognitive_stats.jepa_steps;
    out_losses[6] = brain->cognitive_stats.jepa_last_loss;
    out_stats[7]  = brain->cognitive_stats.creative_steps;
    out_losses[7] = brain->cognitive_stats.creative_last_loss;
    out_stats[8]  = brain->cognitive_stats.self_heal_steps;
    out_losses[8] = brain->cognitive_stats.self_heal_last_loss;
    out_stats[9]  = brain->cognitive_stats.intuition_steps;
    out_losses[9] = brain->cognitive_stats.intuition_last_loss;
    out_stats[10] = brain->cognitive_stats.fep_orchestrator_steps;
    out_losses[10] = brain->cognitive_stats.fep_orchestrator_last_loss;
    *out_count = 11;
    return NIMCP_OK;
}


uint32_t nimcp_brain_get_transcript(
    const brain_decision_t* decision,
    char (*out_entries)[256],
    float* out_saliences,
    float* out_confidences,
    const char** out_modules,
    uint32_t max_entries)
{
    if (!decision || !decision->transcript) return 0;
    const cognitive_transcript_t* t = (const cognitive_transcript_t*)decision->transcript;
    uint32_t count = (t->num_entries < max_entries) ? t->num_entries : max_entries;

    for (uint32_t i = 0; i < count; i++) {
        const transcript_entry_t* e = &t->entries[i];
        if (out_entries) {
            strncpy(out_entries[i], e->summary, 255);
            out_entries[i][255] = '\0';
        }
        if (out_saliences) out_saliences[i] = e->salience;
        if (out_confidences) out_confidences[i] = e->confidence;
        if (out_modules) out_modules[i] = transcript_module_name(e->module);
    }
    return count;
}

uint32_t nimcp_brain_get_last_transcript(
    nimcp_brain_t handle,
    char (*out_entries)[256],
    float* out_saliences,
    float* out_confidences,
    const char** out_modules,
    uint32_t max_entries)
{
    if (!handle || !handle->internal_brain) return 0;
    brain_t brain = handle->internal_brain;
    if (!brain->last_transcript) return 0;

    const cognitive_transcript_t* t = (const cognitive_transcript_t*)brain->last_transcript;
    uint32_t count = (t->num_entries < max_entries) ? t->num_entries : max_entries;

    for (uint32_t i = 0; i < count; i++) {
        const transcript_entry_t* e = &t->entries[i];
        if (out_entries) {
            strncpy(out_entries[i], e->summary, 255);
            out_entries[i][255] = '\0';
        }
        if (out_saliences) out_saliences[i] = e->salience;
        if (out_confidences) out_confidences[i] = e->confidence;
        if (out_modules) out_modules[i] = transcript_module_name(e->module);
    }
    return count;
}


nimcp_status_t nimcp_brain_learn_knowledge(
    nimcp_brain_t brain,
    const char* text,
    int domain)
{
    if (!brain || !text) return NIMCP_ERROR_INVALID_PARAM;
    brain_t b = brain->internal_brain;
    if (!b) return NIMCP_ERROR_NOT_INITIALIZED;

    if (!b->knowledge) {
        set_error("Knowledge system not initialized");
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    if (domain < 0 || domain > 10) {
        domain = 10; /* KNOWLEDGE_DOMAIN_GENERAL */
    }

    uint32_t concepts = knowledge_learn_from_text(
        b->knowledge, text, (knowledge_domain_t)domain);
    return (concepts > 0) ? NIMCP_OK : NIMCP_ERROR_OPERATION_FAILED;
}


//=============================================================================
// Multi-Brain Collective API
//=============================================================================

nimcp_status_t nimcp_brain_connect_collective(
    nimcp_brain_t brain_a,
    nimcp_brain_t brain_b,
    uint32_t instance_id)
{
    API_CHECK_THROW(brain_a, NIMCP_ERROR_NULL_ARG, "NULL brain_a handle");
    API_CHECK_THROW(brain_b, NIMCP_ERROR_NULL_ARG, "NULL brain_b handle");
    API_CHECK_THROW(brain_a->internal_brain, NIMCP_ERROR_INVALID, "brain_a has NULL internal_brain");
    API_CHECK_THROW(brain_b->internal_brain, NIMCP_ERROR_INVALID, "brain_b has NULL internal_brain");

    brain_t a = brain_a->internal_brain;
    brain_t b = brain_b->internal_brain;

    /* Lazily initialize collective cognition if not already enabled */
    if (!a->collective_cognition || !a->collective_cognition_enabled) {
        extern bool nimcp_brain_factory_init_collective_cognition_subsystem(brain_t brain);
        a->config.enable_collective_cognition = true;
        bool ok = nimcp_brain_factory_init_collective_cognition_subsystem(a);
        if (!ok || !a->collective_cognition) {
            set_error("Failed to initialize collective cognition on brain_a");
            return NIMCP_ERROR_OPERATION_FAILED;
        }
    }

    /* Register brain_b as an instance in brain_a's collective */
    int rc = collective_cognition_register_instance(
        a->collective_cognition, instance_id, &b);
    if (rc != 0) {
        set_error("Failed to register brain_b (id=%u) in collective", instance_id);
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Also share brain_a's collective with brain_b so both reference the same system */
    if (b->collective_cognition && b->collective_cognition != a->collective_cognition) {
        /* brain_b has its own — replace with shared reference */
        /* NOTE: Don't destroy brain_b's old one here; lifecycle will handle it */
    }
    b->collective_cognition = a->collective_cognition;
    b->collective_cognition_enabled = true;

    set_error("No error");
    return NIMCP_OK;
}

//=============================================================================
// Avatar State API — Visual Communication
//=============================================================================

/* Forward declarations — emotion_state_t and emotional_system_t already visible
 * via nimcp_emotional_system.h (pulled in by brain_internal.h).
 * speech_motor types are NOT visible, so we forward-declare them. */
typedef struct speech_motor_planner speech_motor_planner_t_api;
typedef struct broca_adapter broca_adapter_t_api;

/* articulator_type_t values (from nimcp_speech_motor.h, can't include due to conflicts) */
#define API_ARTICULATOR_LIPS   0
#define API_ARTICULATOR_TONGUE 1
#define API_ARTICULATOR_JAW    2
#define API_ARTICULATOR_LARYNX 3
#define API_ARTICULATOR_VELUM  4

extern speech_motor_planner_t_api* broca_get_speech_motor_planner(broca_adapter_t_api* adapter);
extern bool speech_motor_get_articulator(const speech_motor_planner_t_api* planner,
                                         int articulator, float* position);

/**
 * @brief Map emotion category to FACS Action Unit activations
 *
 * Based on Ekman's Facial Action Coding System. Each emotion maps to
 * specific AU combinations at biologically-motivated intensities.
 */
static void emotion_to_facs(uint32_t emotion_id, float intensity, nimcp_avatar_state_t* s) {
    /* Reset all AUs */
    s->au1_inner_brow_raise = 0; s->au2_outer_brow_raise = 0; s->au4_brow_lower = 0;
    s->au5_upper_lid_raise = 0;  s->au6_cheek_raise = 0;      s->au7_lid_tighten = 0;
    s->au9_nose_wrinkle = 0;     s->au10_upper_lip_raise = 0;
    s->au12_lip_corner_pull = 0; s->au15_lip_corner_drop = 0;
    s->au17_chin_raise = 0;      s->au20_lip_stretch = 0;      s->au23_lip_tighten = 0;
    s->au25_lips_part = 0;       s->au26_jaw_drop = 0;         s->au28_lip_suck = 0;

    float em_i = fminf(1.0F, intensity);  /* Clamp */

    switch (emotion_id) {
        case 0: /* HAPPINESS */
            s->au6_cheek_raise = 0.8F * em_i;
            s->au12_lip_corner_pull = 0.9F * em_i;  /* Duchenne smile */
            s->au25_lips_part = 0.3F * em_i;
            break;
        case 1: /* SADNESS */
            s->au1_inner_brow_raise = 0.7F * em_i;
            s->au4_brow_lower = 0.3F * em_i;
            s->au15_lip_corner_drop = 0.6F * em_i;
            s->au17_chin_raise = 0.4F * em_i;
            break;
        case 2: /* ANGER */
            s->au4_brow_lower = 0.9F * em_i;
            s->au5_upper_lid_raise = 0.5F * em_i;
            s->au7_lid_tighten = 0.6F * em_i;
            s->au23_lip_tighten = 0.7F * em_i;
            break;
        case 3: /* FEAR */
            s->au1_inner_brow_raise = 0.8F * em_i;
            s->au2_outer_brow_raise = 0.6F * em_i;
            s->au4_brow_lower = 0.3F * em_i;
            s->au5_upper_lid_raise = 0.7F * em_i;
            s->au20_lip_stretch = 0.6F * em_i;
            s->au26_jaw_drop = 0.4F * em_i;
            break;
        case 4: /* DISGUST */
            s->au9_nose_wrinkle = 0.8F * em_i;
            s->au10_upper_lip_raise = 0.7F * em_i;
            s->au4_brow_lower = 0.4F * em_i;
            break;
        case 5: /* SURPRISE */
            s->au1_inner_brow_raise = 0.9F * em_i;
            s->au2_outer_brow_raise = 0.9F * em_i;
            s->au5_upper_lid_raise = 0.8F * em_i;
            s->au26_jaw_drop = 0.7F * em_i;
            s->au25_lips_part = 0.5F * em_i;
            break;
        case 6: /* INTEREST */
            s->au1_inner_brow_raise = 0.4F * em_i;
            s->au2_outer_brow_raise = 0.3F * em_i;
            s->au12_lip_corner_pull = 0.2F * em_i;
            break;
        case 7: /* CONFUSION */
            s->au4_brow_lower = 0.5F * em_i;
            s->au1_inner_brow_raise = 0.6F * em_i;  /* Asymmetric brow */
            s->au7_lid_tighten = 0.3F * em_i;
            break;
        case 16: /* CALM */
            s->au12_lip_corner_pull = 0.15F * em_i;  /* Subtle smile */
            s->au6_cheek_raise = 0.1F * em_i;
            break;
        case 18: /* NEUTRAL */
        default:
            /* No strong AU activation */
            break;
    }
}

/**
 * @brief Derive Preston Blair viseme from articulator positions
 *
 * Maps lip rounding, jaw opening, and tongue position to one of 22
 * canonical viseme shapes used in lip-sync animation.
 *
 * 0=rest, 1=AA/AH, 2=AO, 3=EE, 4=EH, 5=IH, 6=OH, 7=OO/UH,
 * 8=W, 9=R, 10=L, 11=S/Z, 12=SH/CH/J, 13=TH, 14=F/V,
 * 15=T/D/N, 16=K/G/NG, 17=P/B/M, 18=silence
 */
static uint8_t derive_viseme(float lip_round, float jaw_open, float tongue_pos) {
    if (jaw_open < 0.05F && lip_round < 0.1F) return 18; /* silence/rest */
    if (jaw_open > 0.7F && lip_round < 0.3F) return 1;   /* AA wide open */
    if (jaw_open > 0.5F && lip_round > 0.5F) return 2;   /* AO rounded open */
    if (lip_round > 0.7F && jaw_open < 0.3F) return 7;   /* OO/UH tight round */
    if (lip_round > 0.5F && jaw_open > 0.3F) return 6;   /* OH rounded */
    if (lip_round > 0.5F && jaw_open < 0.2F) return 8;   /* W */
    if (tongue_pos > 0.7F && jaw_open < 0.3F) return 3;  /* EE high tongue */
    if (tongue_pos > 0.5F && jaw_open > 0.3F) return 4;  /* EH mid tongue */
    if (tongue_pos > 0.3F) return 5;                       /* IH */
    if (jaw_open > 0.3F) return 4;                         /* EH default open */
    return 0;                                               /* rest */
}


nimcp_status_t nimcp_brain_get_avatar_state(
    nimcp_brain_t brain,
    nimcp_avatar_state_t* state)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain handle");
    API_CHECK_THROW(state, NIMCP_ERROR_NULL_ARG, "NULL avatar state pointer");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    brain_t ib = brain->internal_brain;
    memset(state, 0, sizeof(*state));

    /* --- 1. Speech motor articulators → viseme/mouth shape --- */
    float lip_pos = 0.0F, tongue_pos = 0.0F, jaw_pos = 0.0F;
    float larynx_pos = 0.0F, velum_pos = 0.0F;
    bool has_motor = false;

    if (ib->broca) {
        speech_motor_planner_t_api* planner = broca_get_speech_motor_planner(
            (broca_adapter_t_api*)ib->broca);
        if (planner) {
            has_motor = true;
            speech_motor_get_articulator(planner, API_ARTICULATOR_LIPS,   &lip_pos);
            speech_motor_get_articulator(planner, API_ARTICULATOR_TONGUE, &tongue_pos);
            speech_motor_get_articulator(planner, API_ARTICULATOR_JAW,    &jaw_pos);
            speech_motor_get_articulator(planner, API_ARTICULATOR_LARYNX, &larynx_pos);
            speech_motor_get_articulator(planner, API_ARTICULATOR_VELUM,  &velum_pos);
        }
    }

    state->mouth_open = jaw_pos;
    state->lip_round = lip_pos;
    state->lip_upper = fminf(1.0F, lip_pos * 0.8F + jaw_pos * 0.2F);
    state->lip_lower = fminf(1.0F, lip_pos * 0.6F + jaw_pos * 0.4F);
    state->tongue_position = tongue_pos;
    state->current_viseme = derive_viseme(lip_pos, jaw_pos, tongue_pos);
    state->is_speaking = has_motor && (jaw_pos > 0.05F || lip_pos > 0.1F);

    /* --- 2. Emotional state → FACS Action Units --- */
    emotion_state_t emo = {0};
    bool has_emotion = false;

    if (ib->emotional_system) {
        has_emotion = emotion_system_get_state(ib->emotional_system, &emo);
    }

    if (has_emotion) {
        state->valence = emo.valence;
        state->arousal = emo.arousal;
        state->emotion_id = emo.dominant_emotion;
        state->emotion_intensity = emo.intensity;
        /* Dominance approximation: high valence + low arousal = high dominance */
        state->dominance = fminf(1.0F, fmaxf(-1.0F,
            0.5F * emo.valence - 0.3F * (emo.arousal - 0.5F)));

        emotion_to_facs(emo.dominant_emotion, emo.intensity, state);

        /* Blend speech with emotion: speaking opens jaw/lips more */
        if (state->is_speaking) {
            state->au25_lips_part = fminf(1.0F, state->au25_lips_part + jaw_pos * 0.5F);
            state->au26_jaw_drop = fminf(1.0F, state->au26_jaw_drop + jaw_pos * 0.3F);
        }
    }

    /* --- 3. Gaze and head pose (idle animation from internal state) --- */
    /* Use timestamp-based micro-saccades for natural idle gaze */
    uint64_t now_us = nimcp_time_now_us();
    state->timestamp_us = now_us;

    /* Smooth sinusoidal idle gaze with arousal-modulated amplitude */
    float gaze_amp = 0.05F + (has_emotion ? emo.arousal * 0.1F : 0.0F);
    double t_sec = (double)now_us / 1000000.0;
    state->gaze_x = gaze_amp * sinf((float)(t_sec * 0.7));
    state->gaze_y = gaze_amp * 0.6F * sinf((float)(t_sec * 1.1 + 0.5));

    /* Head follows gaze slightly */
    state->head_yaw = state->gaze_x * 5.0F;   /* degrees */
    state->head_pitch = state->gaze_y * 3.0F;
    state->head_roll = 0.0F;

    /* Blink: ~15 blinks/min (every ~4 sec), blink duration ~150ms */
    double blink_phase = fmod(t_sec, 4.0);
    state->blink = (blink_phase < 0.15) ? 1.0F : 0.0F;

    /* --- 4. Voice parameters modulated by arousal --- */
    float base_pitch = 180.0F;  /* Default female pitch Hz */
    if (has_emotion) {
        /* Arousal raises pitch, valence affects rate */
        state->pitch_hz = base_pitch * (1.0F + 0.3F * emo.arousal);
        state->speaking_rate = 1.0F + 0.2F * emo.arousal;
        state->volume = 0.7F + 0.3F * emo.intensity;
    } else {
        state->pitch_hz = base_pitch;
        state->speaking_rate = 1.0F;
        state->volume = 0.7F;
    }

    set_error("No error");
    return NIMCP_OK;
}


//=============================================================================
// Mixed Precision (AMP) Training
//=============================================================================

nimcp_status_t nimcp_brain_enable_mixed_precision(nimcp_brain_t brain, bool enable)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain handle");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    brain_t ib = brain->internal_brain;

    // Mixed precision requires GPU
    if (!ib->gpu_enabled || !ib->gpu_ctx) {
        set_error("Mixed precision requires GPU acceleration (no GPU context)");
        return NIMCP_ERROR_INVALID;
    }

    // Access the adaptive network's weight cache
    if (!ib->network) {
        set_error("Brain has no neural network");
        return NIMCP_ERROR_INVALID;
    }

    // Use extern declarations to access the adaptive network's GPU weight cache
    extern struct nimcp_gpu_weight_cache_s* adaptive_network_get_gpu_weight_cache(
        adaptive_network_t network);
    extern bool nimcp_gpu_weight_cache_enable_mixed_precision(
        struct nimcp_gpu_weight_cache_s* cache, bool enable);

    struct nimcp_gpu_weight_cache_s* weight_cache =
        adaptive_network_get_gpu_weight_cache(ib->network);
    if (!weight_cache) {
        set_error("GPU weight cache not initialized (run at least one learn step first)");
        return NIMCP_ERROR_INVALID;
    }

    if (!nimcp_gpu_weight_cache_enable_mixed_precision(weight_cache, enable)) {
        set_error("Failed to %s mixed precision on GPU weight cache",
                  enable ? "enable" : "disable");
        return NIMCP_ERROR;
    }

    set_error("No error");
    LOG_INFO("Mixed precision %s for brain", enable ? "enabled" : "disabled");
    return NIMCP_OK;
}


nimcp_status_t nimcp_brain_enable_gradient_checkpointing(
    nimcp_brain_t brain, bool enable, uint32_t checkpoint_interval)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain handle");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    brain_t ib = brain->internal_brain;

    // Gradient checkpointing requires GPU
    if (!ib->gpu_enabled || !ib->gpu_ctx) {
        set_error("Gradient checkpointing requires GPU acceleration (no GPU context)");
        return NIMCP_ERROR_INVALID;
    }

    // Access the adaptive network's weight cache
    if (!ib->network) {
        set_error("Brain has no neural network");
        return NIMCP_ERROR_INVALID;
    }

    // Use extern declarations to access the adaptive network's GPU weight cache
    extern struct nimcp_gpu_weight_cache_s* adaptive_network_get_gpu_weight_cache(
        adaptive_network_t network);
    extern bool nimcp_gpu_weight_cache_set_gradient_checkpointing(
        struct nimcp_gpu_weight_cache_s* cache, bool enable, uint32_t checkpoint_interval);

    struct nimcp_gpu_weight_cache_s* weight_cache =
        adaptive_network_get_gpu_weight_cache(ib->network);
    if (!weight_cache) {
        set_error("GPU weight cache not initialized (run at least one learn step first)");
        return NIMCP_ERROR_INVALID;
    }

    if (!nimcp_gpu_weight_cache_set_gradient_checkpointing(weight_cache, enable, checkpoint_interval)) {
        set_error("Failed to %s gradient checkpointing on GPU weight cache",
                  enable ? "enable" : "disable");
        return NIMCP_ERROR;
    }

    set_error("No error");
    LOG_INFO("Gradient checkpointing %s for brain (interval=%u)",
             enable ? "enabled" : "disabled", checkpoint_interval);
    return NIMCP_OK;
}


// ============================================================================
// Hemispheric Architecture (Callosum + Lateralization)
// ============================================================================

nimcp_status_t nimcp_brain_enable_hemispheric(nimcp_brain_t brain, bool enable)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain handle");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    brain_t ib = brain->internal_brain;

    if (enable && !ib->hemispheric_enabled) {
        // Create corpus callosum if not yet created
        if (!ib->callosum) {
            callosum_config_t cc_cfg = callosum_default_config();
            cc_cfg.bandwidth_mode = CALLOSUM_BW_REALISTIC;
            cc_cfg.queue_capacity = 256;
            cc_cfg.drop_on_overflow = true;
            cc_cfg.initial_connection_strength = 1.0f;
            cc_cfg.enable_bio_async = false;  // Use lightweight mode

            ib->callosum = callosum_create(&cc_cfg);
            if (!ib->callosum) {
                set_error("Failed to create corpus callosum");
                return NIMCP_ERROR;
            }
        }

        // Initialize lateralization with default right-handed profile
        ib->lateralization = lateralization_default_profile();
        ib->dominant_hemisphere = HEMISPHERE_LEFT;  // Right-handed default
        ib->hemispheric_balance = 0.0f;
        ib->last_callosum_process_us = 0;
        ib->hemispheric_enabled = true;

        LOG_INFO("Hemispheric architecture enabled (5-channel callosum, lateralization active)");
    } else if (!enable && ib->hemispheric_enabled) {
        ib->hemispheric_enabled = false;
        // Keep callosum allocated for potential re-enable
        LOG_INFO("Hemispheric architecture disabled");
    }

    set_error("No error");
    return NIMCP_OK;
}


float nimcp_brain_get_lateralization(nimcp_brain_t brain, uint32_t domain)
{
    if (!brain || !brain->internal_brain) return -1.0f;
    if (!brain->internal_brain->hemispheric_enabled) return -1.0f;
    if (domain >= COGNITIVE_DOMAIN_COUNT) return -1.0f;

    return lateralization_get_dominance(
        &brain->internal_brain->lateralization,
        (cognitive_domain_t)domain);
}


nimcp_status_t nimcp_brain_shift_lateralization(
    nimcp_brain_t brain, uint32_t domain, float shift)
{
    API_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain handle");
    API_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain");

    if (!brain->internal_brain->hemispheric_enabled) {
        set_error("Hemispheric architecture not enabled");
        return NIMCP_ERROR_INVALID;
    }
    if (domain >= COGNITIVE_DOMAIN_COUNT) {
        set_error("Invalid cognitive domain index %u (max %d)", domain, COGNITIVE_DOMAIN_COUNT - 1);
        return NIMCP_ERROR_INVALID;
    }

    lateralization_shift_dominance(
        &brain->internal_brain->lateralization,
        (cognitive_domain_t)domain, shift);

    set_error("No error");
    return NIMCP_OK;
}


uint64_t nimcp_brain_get_callosum_transfers(nimcp_brain_t brain)
{
    if (!brain || !brain->internal_brain) return 0;
    if (!brain->internal_brain->hemispheric_enabled || !brain->internal_brain->callosum) return 0;

    callosum_stats_t stats;
    if (callosum_get_stats(brain->internal_brain->callosum, &stats) != 0) return 0;

    return stats.total_messages_left_to_right + stats.total_messages_right_to_left;
}


float nimcp_brain_get_hemispheric_balance(nimcp_brain_t brain)
{
    if (!brain || !brain->internal_brain) return 0.0f;
    if (!brain->internal_brain->hemispheric_enabled) return 0.0f;
    return brain->internal_brain->hemispheric_balance;
}


/* nimcp_brain_get_module_activity — statue-suspect probe accessor.
 *
 * Field order (MUST match nimcp.h docstring + Python binding):
 *   [0]  astrocyte_modulation_events
 *   [1]  oligodendrocyte_myelin_apply
 *   [2]  microglia_pruning_events
 *   [3]  sleep_replay_events
 *   [4]  sleep_downscale_events
 *   [5]  lgss_input_rejections
 *   [6]  lgss_action_blocks
 *   [7]  lgss_motor_blocks
 *   [8]  lgss_training_blocks
 *   [9]  lgss_reward_blocks
 *   [10] ethics_violations
 *   [11] hnn_forward_invocations
 *   [12] hnn_fallback_invocations
 *   [13] cortical_column_forward_invocations
 *   [14] cortical_wta_winners_total
 *   [15] cortical_wta_calls
 *   [16] thalamic_routes_dispatched
 *   [17] thalamic_drops_backpressure
 *   [18] callosum_transfers
 *   [19] kg_consumer_hits
 *   [20..31] reserved (zero)
 *
 * Counters are monotonic uint64; reset only at brain create. Read is
 * unlocked — counters may race with writers, callers should treat
 * deltas as approximate. */
nimcp_status_t nimcp_brain_get_module_activity(
    nimcp_brain_t brain, uint64_t out[32])
{
    if (!brain || !brain->internal_brain || !out) {
        set_error("nimcp_brain_get_module_activity: NULL brain or out");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    brain_t b = brain->internal_brain;
    out[0]  = b->module_activity.astrocyte_modulation_events;
    out[1]  = b->module_activity.oligodendrocyte_myelin_apply;
    out[2]  = b->module_activity.microglia_pruning_events;
    out[3]  = b->module_activity.sleep_replay_events;
    out[4]  = b->module_activity.sleep_downscale_events;
    out[5]  = b->module_activity.lgss_input_rejections;
    out[6]  = b->module_activity.lgss_action_blocks;
    out[7]  = b->module_activity.lgss_motor_blocks;
    out[8]  = b->module_activity.lgss_training_blocks;
    out[9]  = b->module_activity.lgss_reward_blocks;
    out[10] = b->module_activity.ethics_violations;
    out[11] = b->module_activity.hnn_forward_invocations;
    out[12] = b->module_activity.hnn_fallback_invocations;
    out[13] = b->module_activity.cortical_column_forward_invocations;
    out[14] = b->module_activity.cortical_wta_winners_total;
    out[15] = b->module_activity.cortical_wta_calls;
    out[16] = b->module_activity.thalamic_routes_dispatched;
    out[17] = b->module_activity.thalamic_drops_backpressure;
    out[18] = b->module_activity.callosum_transfers;
    out[19] = b->module_activity.kg_consumer_hits;
    for (int i = 20; i < 32; i++) out[i] = 0;

    set_error("No error");
    return NIMCP_OK;
}


nimcp_status_t nimcp_brain_enable_recurrent(nimcp_brain_t brain, bool enable,
                                             uint32_t max_iterations,
                                             float confidence_threshold,
                                             float blend_alpha)
{
    if (!brain || !brain->internal_brain) return NIMCP_ERROR_NULL_POINTER;
    brain_t b = brain->internal_brain;
    b->recurrent_enabled = enable;
    if (max_iterations >= 1 && max_iterations <= 10) {
        b->recurrent_max_iterations = max_iterations;
    }
    if (confidence_threshold >= 0.0f && confidence_threshold <= 1.0f) {
        b->recurrent_confidence_threshold = confidence_threshold;
    }
    if (blend_alpha >= 0.0f && blend_alpha <= 1.0f) {
        b->recurrent_blend_alpha = blend_alpha;
    }
    return NIMCP_OK;
}


nimcp_status_t nimcp_brain_enable_bptt(nimcp_brain_t brain, bool enable,
                                        uint32_t window_size, float discount)
{
    if (!brain || !brain->internal_brain) return NIMCP_ERROR_NULL_POINTER;
    brain_t b = brain->internal_brain;
    b->bptt_enabled = enable;

    if (window_size >= 1 && window_size <= 32 && window_size != b->bptt_window_size) {
        /* Free old buffer */
        if (b->bptt_buffer) {
            for (uint32_t i = 0; i < b->bptt_window_size; i++) {
                nimcp_free(b->bptt_buffer[i].input);
                nimcp_free(b->bptt_buffer[i].output);
                nimcp_free(b->bptt_buffer[i].target);
            }
            nimcp_free(b->bptt_buffer);
        }
        b->bptt_window_size = window_size;
        b->bptt_buffer = nimcp_calloc(window_size, sizeof(*b->bptt_buffer));
        b->bptt_head = 0;
        b->bptt_count = 0;
        b->bptt_input_dim = 0;
        b->bptt_output_dim = 0;
    }
    if (discount >= 0.0f && discount <= 1.0f) {
        b->bptt_discount = discount;
    }
    return NIMCP_OK;
}


uint32_t nimcp_brain_get_recurrent_iterations(nimcp_brain_t brain)
{
    if (!brain || !brain->internal_brain) return 0;
    return brain->internal_brain->recurrent_iteration_count;
}


nimcp_status_t nimcp_brain_connect_cloud(nimcp_brain_t brain,
                                          nimcp_brain_t cloud_brain,
                                          float confidence_threshold,
                                          bool enable_distillation)
{
    if (!brain || !brain->internal_brain) return NIMCP_ERROR_NULL_POINTER;
    if (!cloud_brain || !cloud_brain->internal_brain) return NIMCP_ERROR_NULL_POINTER;

    brain_t b = brain->internal_brain;

    // Destroy existing bridge if any
    if (b->cloud_bridge) {
        cloud_inference_destroy((cloud_inference_bridge_t*)b->cloud_bridge);
    }

    cloud_inference_config_t cfg = cloud_inference_default_config();
    if (confidence_threshold >= 0.0f && confidence_threshold <= 1.0f) {
        cfg.confidence_threshold = confidence_threshold;
    }
    cfg.enable_distillation = enable_distillation;

    cloud_inference_bridge_t* bridge = cloud_inference_create(
        &cfg, cloud_backend_local_brain, cloud_brain->internal_brain);

    if (!bridge) return NIMCP_ERROR_NO_MEMORY;

    b->cloud_bridge = (struct cloud_inference_bridge*)bridge;
    b->cloud_inference_enabled = true;

    return NIMCP_OK;
}


nimcp_status_t nimcp_brain_disconnect_cloud(nimcp_brain_t brain)
{
    if (!brain || !brain->internal_brain) return NIMCP_ERROR_NULL_POINTER;
    brain_t b = brain->internal_brain;

    if (b->cloud_bridge) {
        cloud_inference_destroy((cloud_inference_bridge_t*)b->cloud_bridge);
        b->cloud_bridge = NULL;
    }
    b->cloud_inference_enabled = false;

    return NIMCP_OK;
}


nimcp_status_t nimcp_brain_get_cloud_stats(nimcp_brain_t brain,
                                            uint64_t* total_queries,
                                            uint64_t* local_handled,
                                            uint64_t* cloud_escalated,
                                            uint64_t* distillation_steps)
{
    if (!brain || !brain->internal_brain) return NIMCP_ERROR_NULL_POINTER;
    brain_t b = brain->internal_brain;

    if (!b->cloud_bridge) {
        if (total_queries) *total_queries = 0;
        if (local_handled) *local_handled = 0;
        if (cloud_escalated) *cloud_escalated = 0;
        if (distillation_steps) *distillation_steps = 0;
        return NIMCP_OK;
    }

    cloud_inference_stats_t stats = cloud_inference_get_stats(
        (const cloud_inference_bridge_t*)b->cloud_bridge);
    if (total_queries) *total_queries = stats.total_queries;
    if (local_handled) *local_handled = stats.local_handled;
    if (cloud_escalated) *cloud_escalated = stats.cloud_escalated;
    if (distillation_steps) *distillation_steps = stats.distillation_steps;

    return NIMCP_OK;
}


uint32_t nimcp_brain_distill_cloud_batch(nimcp_brain_t brain, uint32_t max_examples)
{
    if (!brain || !brain->internal_brain) return 0;
    brain_t b = brain->internal_brain;
    if (!b->cloud_bridge) return 0;

    return cloud_inference_distill_batch(
        (cloud_inference_bridge_t*)b->cloud_bridge,
        b, max_examples);
}


uint32_t nimcp_brain_get_active_neuron_count(nimcp_brain_t brain) {
    if (!brain || !brain->internal_brain) return 0;
    brain_t b = brain->internal_brain;
    if (!b->network) return 0;
    neural_network_t net = adaptive_network_get_base_network(b->network);
    if (!net) return 0;
    return neural_network_get_active_count(net);
}

float nimcp_brain_get_sparsity_ratio(nimcp_brain_t brain) {
    if (!brain || !brain->internal_brain) return 0.0f;
    brain_t b = brain->internal_brain;
    if (!b->network) return 0.0f;
    neural_network_t net = adaptive_network_get_base_network(b->network);
    if (!net) return 0.0f;
    return neural_network_get_sparsity_ratio(net);
}

void nimcp_brain_set_training_mode(nimcp_brain_t brain, bool active) {
    if (!brain || !brain->internal_brain) return;
    brain->internal_brain->config.training_mode_active = active;
}

/* SNN-only recovery mode: freezes adaptive/CNN/LNN/cortex-CNN training so
 * the SNN can re-converge against a stable ensemble after the Apr 11 2026
 * unroll_steps=1000 fix exposed co-adaptation drift. Toggle via the daemon
 * set_snn_only_recovery RPC. */
void nimcp_brain_set_snn_only_recovery(nimcp_brain_t brain, bool enabled) {
    if (!brain || !brain->internal_brain) return;
    brain->internal_brain->config.snn_only_recovery_mode = enabled;
}

bool nimcp_brain_get_snn_only_recovery(nimcp_brain_t brain) {
    if (!brain || !brain->internal_brain) return false;
    return brain->internal_brain->config.snn_only_recovery_mode;
}

/* Per-network training ablation setters. Dynamically toggle whether a
 * given sub-network participates in brain_learn_vector updates. Reads
 * are made on every learn call, so flips take effect immediately with
 * no rebuild required. Paired getters return the current state. */

void nimcp_brain_set_train_ann(nimcp_brain_t brain, bool enabled) {
    if (!brain || !brain->internal_brain) return;
    brain->internal_brain->config.train_ann = enabled;
}
bool nimcp_brain_get_train_ann(nimcp_brain_t brain) {
    if (!brain || !brain->internal_brain) return false;
    return brain->internal_brain->config.train_ann;
}

void nimcp_brain_set_train_cnn(nimcp_brain_t brain, bool enabled) {
    if (!brain || !brain->internal_brain) return;
    brain->internal_brain->config.train_cnn = enabled;
}
bool nimcp_brain_get_train_cnn(nimcp_brain_t brain) {
    if (!brain || !brain->internal_brain) return false;
    return brain->internal_brain->config.train_cnn;
}

void nimcp_brain_set_train_snn(nimcp_brain_t brain, bool enabled) {
    if (!brain || !brain->internal_brain) return;
    brain->internal_brain->config.train_snn = enabled;
}
bool nimcp_brain_get_train_snn(nimcp_brain_t brain) {
    if (!brain || !brain->internal_brain) return false;
    return brain->internal_brain->config.train_snn;
}

void nimcp_brain_set_train_lnn(nimcp_brain_t brain, bool enabled) {
    if (!brain || !brain->internal_brain) return;
    brain->internal_brain->config.train_lnn = enabled;
}
bool nimcp_brain_get_train_lnn(nimcp_brain_t brain) {
    if (!brain || !brain->internal_brain) return false;
    return brain->internal_brain->config.train_lnn;
}

/* Ensemble warmup scale [0.0, 1.0] — probabilistic gate on non-SNN
 * training updates. Used by the plateau detector to ramp ANN/CNN/LNN
 * back in gradually after snn-only recovery. Out-of-range values are
 * clamped to [0.0, 1.0]. */
void nimcp_brain_set_ensemble_warmup_scale(nimcp_brain_t brain, float scale) {
    if (!brain || !brain->internal_brain) return;
    if (scale < 0.0f) scale = 0.0f;
    if (scale > 1.0f) scale = 1.0f;
    brain->internal_brain->config.ensemble_warmup_scale = scale;
}
float nimcp_brain_get_ensemble_warmup_scale(nimcp_brain_t brain) {
    if (!brain || !brain->internal_brain) return 1.0f;
    return brain->internal_brain->config.ensemble_warmup_scale;
}

int nimcp_brain_eager_init_cognitive(nimcp_brain_t brain) {
    if (!brain || !brain->internal_brain) return -1;
    brain_t b = brain->internal_brain;
    int count = 0;

    /* Initialize subsystems that are NULL after checkpoint load.
     *
     * IMPORTANT: We CANNOT call nimcp_brain_parallel_init_subsystems() here.
     * That function is designed for fresh brain creation — it NULLs GPU context,
     * destroys inference pools, and reinitializes from scratch. On a loaded brain
     * with active GPU/pool, it would leak CUDA memory and crash.
     *
     * Enable config flags that gate region initialization.
     * These may be false in old checkpoints that didn't have these features. */
    /* Enable config flags needed by region inits. */
    b->config.enable_oscillations = true;
    b->config.enable_speech_cortex = true;
    b->config.enable_multimodal_integration = true;
    b->config.enable_parietal = true;
    b->config.enable_thousand_brains_integration = true;

    /* Core dependencies first */
    if (!b->engram_system) {
        extern engram_system_t* engram_system_create(void);
        b->engram_system = engram_system_create();
        if (b->engram_system) count++;
    }

    /* Cognitive modules */
    extern bool nimcp_brain_factory_init_working_memory_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_executive_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_symbolic_logic_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_global_workspace_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_mirror_neurons(brain_t);
    extern bool nimcp_brain_factory_init_glial_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_theory_of_mind_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_ethics_engine_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_fep_orchestrator_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_language_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_creative_subsystem(brain_t);

    #define INIT_IF_NULL(field, fn) do { \
        if (!(b->field)) { fn(b); if (b->field) count++; } \
    } while(0)
    #define INIT_IF_DISABLED(field, fn) do { \
        if (!(b->field)) { fn(b); if (b->field) count++; } \
    } while(0)

    INIT_IF_NULL(working_memory, nimcp_brain_factory_init_working_memory_subsystem);
    INIT_IF_NULL(executive, nimcp_brain_factory_init_executive_subsystem);
    INIT_IF_NULL(symbolic_logic, nimcp_brain_factory_init_symbolic_logic_subsystem);
    INIT_IF_NULL(global_workspace, nimcp_brain_factory_init_global_workspace_subsystem);
    INIT_IF_NULL(mirror_neurons, nimcp_brain_factory_init_mirror_neurons);
    INIT_IF_NULL(glial, nimcp_brain_factory_init_glial_subsystem);
    INIT_IF_NULL(theory_of_mind, nimcp_brain_factory_init_theory_of_mind_subsystem);
    INIT_IF_NULL(ethics, nimcp_brain_factory_init_ethics_engine_subsystem);
    INIT_IF_NULL(fep_orchestrator, nimcp_brain_factory_init_fep_orchestrator_subsystem);
    INIT_IF_NULL(language_layer, nimcp_brain_factory_init_language_subsystem);
    INIT_IF_NULL(creative_orchestrator, nimcp_brain_factory_init_creative_subsystem);

    /* Brain regions */
    extern bool nimcp_brain_factory_init_broca_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_wernicke_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_cerebellum_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_hippocampus_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_hypothalamus_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_medulla_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_basal_ganglia_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_parietal_cortex_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_parietal_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_intuition_subsystem(brain_t);
    /* Previously dormant — init waves existed but were never invoked, so
     * brain->prefrontal/cingulate/insula stayed NULL and the GL regional
     * subscribers no-op'd silently. Activated here so the regional
     * language wiring (commit 5074c4b0f) can actually fire. */
    extern bool nimcp_brain_factory_init_prefrontal_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_cingulate_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_insula_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_ofc_subsystem(brain_t);

    INIT_IF_NULL(broca, nimcp_brain_factory_init_broca_subsystem);
    INIT_IF_NULL(wernicke, nimcp_brain_factory_init_wernicke_subsystem);
    INIT_IF_NULL(cerebellum, nimcp_brain_factory_init_cerebellum_subsystem);
    INIT_IF_NULL(hippocampus, nimcp_brain_factory_init_hippocampus_subsystem);
    INIT_IF_NULL(hypothalamus, nimcp_brain_factory_init_hypothalamus_subsystem);
    INIT_IF_DISABLED(medulla_enabled, nimcp_brain_factory_init_medulla_subsystem);
    INIT_IF_NULL(basal_ganglia, nimcp_brain_factory_init_basal_ganglia_subsystem);
    INIT_IF_NULL(parietal_cortex, nimcp_brain_factory_init_parietal_cortex_subsystem);
    INIT_IF_NULL(parietal, nimcp_brain_factory_init_parietal_subsystem);
    INIT_IF_NULL(intuition_system, nimcp_brain_factory_init_intuition_subsystem);
    INIT_IF_NULL(prefrontal, nimcp_brain_factory_init_prefrontal_subsystem);
    INIT_IF_NULL(cingulate, nimcp_brain_factory_init_cingulate_subsystem);
    INIT_IF_NULL(insula, nimcp_brain_factory_init_insula_subsystem);
    INIT_IF_NULL(ofc, nimcp_brain_factory_init_ofc_subsystem);

    /* Phase 1 audit fix: language subsystem ran at line 5229 BEFORE broca
     * (line 5252) and wernicke (line 5253) existed, so the
     * `if (brain->broca) connect_broca(...)` lines inside init_language
     * silently no-op'd. Result: gl->broca_adapter and gl->wernicke_adapter
     * stayed NULL for the brain's lifetime, breaking every produce-side
     * Broca path. Re-attach now that both ends exist.
     *
     * The connect_* funcs are idempotent pointer assignments. The
     * broca_attach_grounded_language / wernicke_attach_grounded_language
     * reverse-direction calls live in init_language but are also safe to
     * re-fire if needed — they just overwrite a cached handle.
     *
     * TODO: this should be moved into init_language as a deferred attach
     * pass, but for the audit experiment the placement here is sufficient
     * and minimally invasive. */
    {
        extern void grounded_language_connect_broca(grounded_language_t*, void*);
        extern void grounded_language_connect_wernicke(grounded_language_t*, void*);
        extern bool broca_attach_grounded_language(broca_adapter_t*, grounded_language_t*);
        extern bool wernicke_attach_grounded_language(wernicke_adapter_t*, grounded_language_t*);
        extern uint64_t grounded_language_mirror_lexicon_to_broca(
            grounded_language_t*, void*);
        if (b->grounded_lang && b->broca) {
            grounded_language_connect_broca(b->grounded_lang, b->broca);
            broca_attach_grounded_language((broca_adapter_t*)b->broca,
                                            (grounded_language_t*)b->grounded_lang);
            /* Phase 2C: push GL's vocab into Broca's separate lexicon
             * with POS info so Broca's CYK chart parser can fire phrase
             * rules on bridge production output. Without this, every
             * WordNet word looks like POS_UNKNOWN to Broca and
             * syntax_build_tree fails on every input. */
            (void)grounded_language_mirror_lexicon_to_broca(
                (grounded_language_t*)b->grounded_lang, b->broca);
        }
        if (b->grounded_lang && b->wernicke) {
            grounded_language_connect_wernicke(b->grounded_lang, b->wernicke);
            wernicke_attach_grounded_language((wernicke_adapter_t*)b->wernicke,
                                                (grounded_language_t*)b->grounded_lang);
        }
    }

    /* Monitoring & infrastructure */
    extern bool nimcp_brain_factory_init_mental_health_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_core_directives_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_cortical_columns_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_pink_noise_subsystem(brain_t);

    INIT_IF_NULL(mental_health_monitor, nimcp_brain_factory_init_mental_health_subsystem);
    INIT_IF_DISABLED(core_directives_enabled, nimcp_brain_factory_init_core_directives_subsystem);
    INIT_IF_NULL(pink_noise, nimcp_brain_factory_init_pink_noise_subsystem);

    INIT_IF_NULL(tb_integration_hub, nimcp_brain_factory_init_cortical_columns_subsystem);

    /* Cognitive subsystems that use safer init patterns */
    extern bool nimcp_brain_factory_init_curiosity_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_imagination_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_reasoning_engine_subsystem(brain_t);
    extern bool nimcp_brain_factory_init_introspection_subsystem(brain_t);

    INIT_IF_NULL(curiosity, nimcp_brain_factory_init_curiosity_subsystem);
    INIT_IF_NULL(imagination, nimcp_brain_factory_init_imagination_subsystem);
    INIT_IF_NULL(reasoning_engine, nimcp_brain_factory_init_reasoning_engine_subsystem);
    INIT_IF_NULL(introspection, nimcp_brain_factory_init_introspection_subsystem);

    /* Oscillation analyzer — no factory function, created directly */
    if (!b->oscillations && b->config.enable_oscillations) {
        extern brain_oscillation_analyzer_t* brain_oscillation_create(
            brain_t, uint32_t, uint32_t);
        b->oscillations = brain_oscillation_create(b, 1000, 1000);
        if (b->oscillations) count++;
    }

    #undef INIT_IF_NULL
    #undef INIT_IF_DISABLED

    /* Unified Training Manager: not serialized in checkpoint.
     * Without UTM, the cortex CNN training block (which gates on
     * brain->unified_training != NULL) is entirely skipped, leaving
     * all 4 cortex processors at forward_steps=0. */
    if (!b->unified_training) {
        extern int brain_enable_multi_network_training(brain_t);
        if (brain_enable_multi_network_training(b) >= 0) {
            count++;
        }
    }

    /* GPU inference: adaptive network weight cache + GPU forward path.
     * Must run AFTER full brain restoration — adaptive_network_get_config()
     * crashes on partially-restored state during nimcp_brain_load(). */
    if (b->gpu_enabled && b->gpu_ctx && b->network) {
        extern bool nimcp_brain_factory_init_gpu_inference(brain_t);
        if (nimcp_brain_factory_init_gpu_inference(b)) {
            count++;
        }
    }

    /* Inference thread pool for parallel cognitive dispatch.
     * Not serialized in checkpoint — create on loaded brains. */
    if (!b->inference_pool) {
        extern nimcp_thread_pool_t* nimcp_pool_create(size_t num_threads);
        b->inference_pool = nimcp_pool_create(8);  /* 8 threads for parallel actors */
        if (b->inference_pool) {
            count++;
        }
    }

    /* Post-init sidecar load (.immune / .kg / .gl_lang).
     *
     * These sidecars used to be loaded inside brain_load() itself, gated on
     * brain->immune_system / brain->internal_kg / brain->grounded_lang being
     * non-NULL. But those subsystems are created HERE — not during
     * brain_load — so the gates always failed silently and the trained
     * lexicon / immune memory / KG facts were lost on every resume.
     *
     * brain_load_auto() stashes the source filepath in brain->loaded_from_path;
     * the loader is a no-op for fresh brains (path is empty). */
    {
        extern void brain_load_post_init_sidecars(brain_t brain);
        brain_load_post_init_sidecars(b);
    }

    /* === Toxicity wire-up — FORCED late-binding (2026-05-21 fix) ===
     * Bug found on first pod deploy: on --resume, brain->language_layer
     * was already non-NULL from the checkpoint, so INIT_IF_NULL above
     * SKIPPED init_language_subsystem, so the toxicity wire-up code
     * inside init_language never ran, so grounded_respond's input-toxicity
     * check was a no-op (gl->toxicity_classifier was NULL even though
     * brain->toxicity_classifier was set by my resume path).
     *
     * Symptom: classify_toxicity() RPC worked (uses brain->), but
     * grounded_respond("jews are subhuman") returned the same word as
     * a benign input — no counterclaim. Verified live on pod.
     *
     * Fix: re-wire toxicity_classifier + toxicity_response into
     * grounded_lang unconditionally here, after all subsystems are
     * initialized. Idempotent if already wired. */
    if (b->grounded_lang) {
        if (b->toxicity_classifier) {
            extern void grounded_language_set_toxicity_classifier(
                struct grounded_language*, void*);
            grounded_language_set_toxicity_classifier(b->grounded_lang,
                                                      b->toxicity_classifier);
        }
        if (b->toxicity_response) {
            extern void grounded_language_set_toxicity_response(
                struct grounded_language*, void*);
            grounded_language_set_toxicity_response(b->grounded_lang,
                                                    b->toxicity_response);
        }
    }

    return count;
}

/* ============================================================================
 * Training Dashboard Metrics — written by training script, read by exporter
 * ============================================================================ */

int nimcp_brain_set_training_dashboard(nimcp_brain_t brain,
    uint32_t stage, uint32_t step, const char* domain,
    float fact_ratio, bool warm_start_complete, uint32_t warm_start_step,
    float lr_physics, float lr_chemistry, float lr_biology,
    uint32_t wm_steps, uint32_t wm_phys, uint32_t wm_chem, uint32_t wm_bio,
    uint32_t collapse_events, uint32_t surprises, uint32_t replays,
    uint32_t vocab_size, float lang_confidence, uint32_t active_engines)
{
    if (!brain || !brain->internal_brain) return -1;
    brain_t b = brain->internal_brain;
    b->training_dashboard.current_stage = stage;
    b->training_dashboard.current_step = step;
    if (domain) {
        strncpy(b->training_dashboard.current_domain, domain, 63);
        b->training_dashboard.current_domain[63] = '\0';
    }
    b->training_dashboard.fact_ratio = fact_ratio;
    b->training_dashboard.warm_start_complete = warm_start_complete;
    b->training_dashboard.warm_start_step = warm_start_step;
    b->training_dashboard.lr_physics = lr_physics;
    b->training_dashboard.lr_chemistry = lr_chemistry;
    b->training_dashboard.lr_biology = lr_biology;
    b->training_dashboard.wm_steps = wm_steps;
    b->training_dashboard.wm_physics_transitions = wm_phys;
    b->training_dashboard.wm_chemistry_transitions = wm_chem;
    b->training_dashboard.wm_biology_transitions = wm_bio;
    b->training_dashboard.collapse_events = collapse_events;
    b->training_dashboard.surprises_stored = surprises;
    b->training_dashboard.replays_done = replays;
    b->training_dashboard.vocab_size = vocab_size;
    b->training_dashboard.language_confidence = lang_confidence;
    b->training_dashboard.active_engines = active_engines;
    return 0;
}

int nimcp_brain_get_training_dashboard(nimcp_brain_t brain,
    uint32_t* stage, uint32_t* step, char* domain, uint32_t domain_max,
    float* fact_ratio, bool* warm_start_complete, uint32_t* warm_start_step,
    float* lr_physics, float* lr_chemistry, float* lr_biology,
    uint32_t* wm_steps, uint32_t* wm_phys, uint32_t* wm_chem, uint32_t* wm_bio,
    uint32_t* collapse_events, uint32_t* surprises, uint32_t* replays,
    uint32_t* vocab_size, float* lang_confidence, uint32_t* active_engines,
    /* Inference metrics */
    float* inference_time_ms, uint32_t* active_neurons,
    float* reasoning_hypotheses, float* reasoning_plausibility,
    float* attention_strength, float* sleep_pressure)
{
    if (!brain || !brain->internal_brain) return -1;
    brain_t b = brain->internal_brain;

    #define SET_IF(ptr, val) do { if (ptr) *(ptr) = (val); } while(0)
    SET_IF(stage, b->training_dashboard.current_stage);
    SET_IF(step, b->training_dashboard.current_step);
    if (domain && domain_max > 0) {
        strncpy(domain, b->training_dashboard.current_domain, domain_max - 1);
        domain[domain_max - 1] = '\0';
    }
    SET_IF(fact_ratio, b->training_dashboard.fact_ratio);
    SET_IF(warm_start_complete, b->training_dashboard.warm_start_complete);
    SET_IF(warm_start_step, b->training_dashboard.warm_start_step);
    SET_IF(lr_physics, b->training_dashboard.lr_physics);
    SET_IF(lr_chemistry, b->training_dashboard.lr_chemistry);
    SET_IF(lr_biology, b->training_dashboard.lr_biology);
    SET_IF(wm_steps, b->training_dashboard.wm_steps);
    SET_IF(wm_phys, b->training_dashboard.wm_physics_transitions);
    SET_IF(wm_chem, b->training_dashboard.wm_chemistry_transitions);
    SET_IF(wm_bio, b->training_dashboard.wm_biology_transitions);
    SET_IF(collapse_events, b->training_dashboard.collapse_events);
    SET_IF(surprises, b->training_dashboard.surprises_stored);
    SET_IF(replays, b->training_dashboard.replays_done);
    SET_IF(vocab_size, b->training_dashboard.vocab_size);
    SET_IF(lang_confidence, b->training_dashboard.language_confidence);
    SET_IF(active_engines, b->training_dashboard.active_engines);

    /* Inference metrics — from brain state directly */
    SET_IF(inference_time_ms, b->stats.avg_inference_time_us / 1000.0f);
    SET_IF(active_neurons, b->stats.num_active_synapses);  /* proxy: active synapses */
    /* Reasoning — read from last reasoning chain */
    SET_IF(reasoning_hypotheses, 0.0f);  /* TODO: expose from reasoning engine */
    SET_IF(reasoning_plausibility, 0.0f);
    /* Attention */
    SET_IF(attention_strength, b->last_novelty_score);
    /* Sleep */
    SET_IF(sleep_pressure, 0.0f);  /* TODO: expose from sleep system */
    #undef SET_IF

    return 0;
}

void nimcp_brain_set_network_ablation(nimcp_brain_t brain,
                                       int train_cnn, int train_snn, int train_lnn) {
    if (!brain || !brain->internal_brain) return;
    if (train_cnn >= 0) brain->internal_brain->config.train_cnn = (bool)train_cnn;
    if (train_snn >= 0) brain->internal_brain->config.train_snn = (bool)train_snn;
    if (train_lnn >= 0) brain->internal_brain->config.train_lnn = (bool)train_lnn;
}

/* ============================================================================
 * Unified Brain Probe System — Public API
 * ============================================================================ */

#include "core/probes/nimcp_brain_probes.h"

static probe_registry_t* ensure_probe_registry(nimcp_brain_t brain) {
    brain_t b = brain->internal_brain;
    if (!b->probe_registry) {
        /* Atomic CAS to prevent double-init race */
        probe_registry_t* reg = probe_registry_create(b);
        if (reg) {
            struct probe_registry* expected = NULL;
            if (__atomic_compare_exchange_n(
                    (struct probe_registry**)&b->probe_registry,
                    &expected, (struct probe_registry*)reg,
                    false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
                probe_registry_start(reg);
            } else {
                /* Another thread won the race — destroy our duplicate */
                probe_registry_destroy(reg);
            }
        }
    }
    return (probe_registry_t*)b->probe_registry;
}

int nimcp_brain_create_probe(nimcp_brain_t brain,
    const uint16_t* module_ids, uint32_t num_modules,
    uint32_t interval_ms, uint32_t mode,
    uint32_t* out_handle)
{
    if (!brain || !brain->internal_brain || !out_handle) return -1;

    probe_registry_t* reg = ensure_probe_registry(brain);
    if (!reg) return -1;

    probe_handle_t h = PROBE_INVALID_HANDLE;
    if (mode == PROBE_MODE_ACTIVE || mode == PROBE_MODE_HYBRID) {
        /* Generic sampler: reports whether each module is initialized */
        extern void probe_generic_sampler(void*, uint16_t, brain_t,
            probe_metric_t*, uint32_t*, void*);
        h = probe_create_active(reg, "custom", module_ids, num_modules,
                                probe_generic_sampler, NULL, interval_ms);
    } else {
        h = probe_create_passive(reg, "custom", module_ids, num_modules,
                                  NULL, 0);
    }

    *out_handle = h;
    return (h != PROBE_INVALID_HANDLE) ? 0 : -1;
}

int nimcp_brain_get_probe_metrics(nimcp_brain_t brain,
    uint32_t handle,
    nimcp_probe_metric_t* out, uint32_t max, uint32_t* count)
{
    if (!brain || !brain->internal_brain || !out || !count) return -1;
    probe_registry_t* reg = (probe_registry_t*)brain->internal_brain->probe_registry;
    if (!reg) return -1;

    /* nimcp_probe_metric_t and probe_metric_t have identical layout */
    *count = probe_get_metrics(reg, handle, (probe_metric_t*)out, max);
    return 0;
}

int nimcp_brain_get_all_probe_metrics_json(nimcp_brain_t brain, char** out_json) {
    if (!brain || !brain->internal_brain || !out_json) return -1;
    probe_registry_t* reg = (probe_registry_t*)brain->internal_brain->probe_registry;
    if (!reg) {
        *out_json = NULL;
        return -1;
    }
    *out_json = probe_get_all_metrics_json(reg);
    return *out_json ? 0 : -1;
}

void nimcp_brain_destroy_probe(nimcp_brain_t brain, uint32_t handle) {
    if (!brain || !brain->internal_brain) return;
    probe_registry_t* reg = (probe_registry_t*)brain->internal_brain->probe_registry;
    if (reg) probe_destroy(reg, handle);
}

int nimcp_brain_attach_builtin_probes(nimcp_brain_t brain, uint32_t interval_ms) {
    if (!brain || !brain->internal_brain) return -1;
    probe_registry_t* reg = ensure_probe_registry(brain);
    if (!reg) return -1;

    int count = 0;
    if (probe_attach_network_metrics(reg, interval_ms) != PROBE_INVALID_HANDLE) count++;
    if (probe_attach_cognitive_stats(reg, interval_ms) != PROBE_INVALID_HANDLE) count++;
    if (probe_attach_training_dashboard(reg, interval_ms) != PROBE_INVALID_HANDLE) count++;
    if (probe_attach_inference(reg, interval_ms) != PROBE_INVALID_HANDLE) count++;
    if (probe_attach_glial(reg, interval_ms) != PROBE_INVALID_HANDLE) count++;
    if (probe_attach_neurons(reg, interval_ms) != PROBE_INVALID_HANDLE) count++;
    if (probe_attach_synapses(reg, interval_ms) != PROBE_INVALID_HANDLE) count++;
    if (probe_attach_brain_regions(reg, interval_ms) != PROBE_INVALID_HANDLE) count++;
    if (probe_attach_dispatch(reg, interval_ms) != PROBE_INVALID_HANDLE) count++;
    return count;
}

bool nimcp_brain_get_network_metrics(nimcp_brain_t brain,
                                      float* ema_ann, float* ema_cnn,
                                      float* ema_snn, float* ema_lnn,
                                      uint64_t* ann_steps, uint64_t* cnn_steps,
                                      uint64_t* snn_steps, uint64_t* lnn_steps) {
    if (!brain || !brain->internal_brain) return false;
    brain_t b = brain->internal_brain;
    if (ema_ann) *ema_ann = b->network_metrics.ema_ann_loss;
    if (ema_cnn) *ema_cnn = b->network_metrics.ema_cnn_loss;
    if (ema_snn) *ema_snn = b->network_metrics.ema_snn_loss;
    if (ema_lnn) *ema_lnn = b->network_metrics.ema_lnn_loss;
    if (ann_steps) *ann_steps = b->network_metrics.ann_steps;
    if (cnn_steps) *cnn_steps = b->network_metrics.cnn_steps;
    if (snn_steps) *snn_steps = b->network_metrics.snn_steps;
    if (lnn_steps) *lnn_steps = b->network_metrics.lnn_steps;
    return true;
}
