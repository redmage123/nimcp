// nimcp_brain_part_stats.c - stats functions
// Part of nimcp_brain.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_brain.c


/**
 * @brief Initialize brain statistics
 *
 * WHY: Separates stats initialization for clarity
 * Makes stats setup reusable
 *
 * COMPLEXITY: O(1)
 *
 * @param stats Output stats structure
 * @param task_name Name for brain
 * @param size Size preset
 * @param num_inputs Input dimension
 * @param learning_rate Learning rate
 */
void init_brain_stats(brain_stats_t* stats, const char* task_name, brain_size_t size,
                             uint32_t num_inputs, float learning_rate)
{
    uint32_t num_neurons = get_neuron_count(size);

    stats->size = size;
    stats->num_neurons = num_neurons;
    stats->num_synapses = num_neurons * num_inputs;
    stats->num_active_synapses = stats->num_synapses;
    stats->current_learning_rate = learning_rate;
    stats->quantum_annealing_runs = 0;  // Initialize quantum annealing counter
    strncpy(stats->task_name, task_name, sizeof(stats->task_name) - 1);
}


/**
 * @brief Update brain statistics after inference
 *
 * COMPLEXITY: O(1)
 */
static void update_inference_stats(brain_t brain, brain_decision_t* decision)
{
    // Process pending bio-async messages (use atomic load for thread safety)
    bio_module_context_t ctx = __atomic_load_n(&g_brain_bio_ctx, __ATOMIC_ACQUIRE);
    if (ctx) {
        bio_router_process_inbox(ctx, 5);
    }

    // Use atomic increment for thread-safe stats update
    uint64_t new_count = __atomic_fetch_add(&brain->stats.total_inferences, 1, __ATOMIC_RELAXED) + 1;

    // P1-51 FIX: Use atomic load/store for avg_inference_time_us to prevent
    // torn reads/writes under concurrent access. This is intentionally approximate
    // (not perfectly serialized) for performance - acceptable for statistics.
    float old_avg;
    __atomic_load(&brain->stats.avg_inference_time_us, &old_avg, __ATOMIC_RELAXED);
    float new_avg = (old_avg * (new_count - 1) + decision->inference_time_us) / new_count;
    __atomic_store(&brain->stats.avg_inference_time_us, &new_avg, __ATOMIC_RELAXED);
    __atomic_store(&brain->stats.avg_sparsity, &decision->sparsity, __ATOMIC_RELAXED);
}
