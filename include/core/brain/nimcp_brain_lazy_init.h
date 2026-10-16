/**
 * @file nimcp_brain_lazy_init.h
 * @brief Lazy initialization trigger macros for brain subsystems.
 *
 * Provides BRAIN_LAZY_INIT and per-subsystem BRAIN_ENSURE_* macros that
 * transparently initialize a subsystem on first access.  Each macro
 * checks whether the subsystem pointer is NULL and, if so, calls the
 * corresponding factory init function.  A debug log message is emitted
 * on successful lazy creation.
 *
 * Usage example (inside a function that needs glial integration):
 *
 *     BRAIN_ENSURE_GLIAL(brain);
 *     // brain->glial is now guaranteed non-NULL (or init failed)
 *
 * IMPORTANT: These macros are NOT thread-safe on their own.  If the
 * calling context does not already hold the brain mutex, wrap the
 * macro invocation in a lock/unlock pair.
 */
#ifndef NIMCP_BRAIN_LAZY_INIT_H
#define NIMCP_BRAIN_LAZY_INIT_H

#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"

/* Forward-declare factory init functions so this header is self-contained. */
#include "core/brain/factory/nimcp_brain_factory.h"

/* These two are not in nimcp_brain_factory.h — declared in sub-headers. */
extern bool nimcp_brain_factory_init_fep_orchestrator_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_creative_subsystem(brain_t brain);

/* --------------------------------------------------------------------------
 * Generic lazy-init macro
 * --------------------------------------------------------------------------
 * @param brain    brain_t (i.e. struct brain_struct*).  This is the same
 *                 pointer type used by both callers with internal access
 *                 and the factory init functions.
 * @param field    Name of the struct field to check (e.g. glial).
 * @param init_fn  Factory function: bool init_fn(brain_t).
 * -------------------------------------------------------------------------- */
#define BRAIN_LAZY_INIT(brain, field, init_fn) do {                          \
    if (!(brain)->field) {                                                   \
        (init_fn)((brain));                                                  \
        if ((brain)->field) {                                                \
            LOG_DEBUG("LAZY_INIT", "Lazily initialized: " #field);           \
        }                                                                    \
    }                                                                        \
} while (0)

/* --------------------------------------------------------------------------
 * Per-subsystem convenience macros
 *
 * Field name and init function verified against:
 *   - include/core/brain/nimcp_brain_internal.h  (field declarations)
 *   - include/core/brain/factory/nimcp_brain_factory.h  (init prototypes)
 *   - src/core/brain/factory/init/*.c  (implementations)
 * -------------------------------------------------------------------------- */

/** Glial integration (astrocytes, oligodendrocytes, microglia).
 *  Field: glial_integration_t* glial */
#define BRAIN_ENSURE_GLIAL(brain) \
    BRAIN_LAZY_INIT(brain, glial, nimcp_brain_factory_init_glial_subsystem)

/** Axon network (output fibers for all neurons).
 *  Field: void* axon_network */
#define BRAIN_ENSURE_AXON_NETWORK(brain) \
    BRAIN_LAZY_INIT(brain, axon_network, nimcp_brain_factory_init_axon_subsystem)

/** Dendrite network (input fibers for all neurons).
 *  Field: void* dendrite_network */
#define BRAIN_ENSURE_DENDRITE_NETWORK(brain) \
    BRAIN_LAZY_INIT(brain, dendrite_network, nimcp_brain_factory_init_dendrite_subsystem)

/** Neuromodulator system (DA, 5-HT, ACh, NE, GABA, GLU).
 *  Field: neuromodulator_system_t neuromodulator_system
 *  Note: neuromodulator_system_t is a pointer typedef
 *        (struct neuromodulator_system_struct*), so the NULL check works. */
#define BRAIN_ENSURE_NEUROMOD(brain) \
    BRAIN_LAZY_INIT(brain, neuromodulator_system, nimcp_brain_factory_init_neuromodulator_system)

/** Memory consolidation (hippocampal replay, sleep-dependent transfer).
 *  Field: consolidation_handle_t consolidation
 *  Note: consolidation_handle_t is a pointer typedef
 *        (struct consolidation_handle_struct*). */
#define BRAIN_ENSURE_CONSOLIDATION(brain) \
    BRAIN_LAZY_INIT(brain, consolidation, nimcp_brain_factory_init_consolidation_subsystem)

/** Symbolic logic (first-order logic, resolution, backward chaining).
 *  Field: symbolic_logic_t* symbolic_logic */
#define BRAIN_ENSURE_SYMBOLIC_LOGIC(brain) \
    BRAIN_LAZY_INIT(brain, symbolic_logic, nimcp_brain_factory_init_symbolic_logic_subsystem)

/** Working memory (active representation buffer, prefrontal cortex).
 *  Field: working_memory_t* working_memory */
#define BRAIN_ENSURE_WORKING_MEMORY(brain) \
    BRAIN_LAZY_INIT(brain, working_memory, nimcp_brain_factory_init_working_memory_subsystem)

/** Executive controller (DLPFC, task switching, inhibition).
 *  Field: executive_controller_t* executive */
#define BRAIN_ENSURE_EXECUTIVE(brain) \
    BRAIN_LAZY_INIT(brain, executive, nimcp_brain_factory_init_executive_subsystem)

/** Theory of mind (modeling other agents' beliefs and goals).
 *  Field: theory_of_mind_t theory_of_mind
 *  Note: theory_of_mind_t is a pointer typedef
 *        (struct theory_of_mind_s*). */
#define BRAIN_ENSURE_THEORY_OF_MIND(brain) \
    BRAIN_LAZY_INIT(brain, theory_of_mind, nimcp_brain_factory_init_theory_of_mind_subsystem)

/** Global workspace (conscious access broadcast architecture).
 *  Field: global_workspace_t* global_workspace */
#define BRAIN_ENSURE_GLOBAL_WORKSPACE(brain) \
    BRAIN_LAZY_INIT(brain, global_workspace, nimcp_brain_factory_init_global_workspace_subsystem)

/** Ethics engine (Golden Rule, Asimov pipeline, empathy).
 *  Field: ethics_engine_t ethics
 *  Note: ethics_engine_t is a pointer typedef
 *        (struct ethics_engine_struct*). */
#define BRAIN_ENSURE_ETHICS(brain) \
    BRAIN_LAZY_INIT(brain, ethics, nimcp_brain_factory_init_ethics_engine_subsystem)

/** Mirror neurons (observation-action learning).
 *  Field: mirror_neurons_t mirror_neurons
 *  Note: mirror_neurons_t is a pointer typedef
 *        (mirror_neurons_system_t*). */
#define BRAIN_ENSURE_MIRROR_NEURONS(brain) \
    BRAIN_LAZY_INIT(brain, mirror_neurons, nimcp_brain_factory_init_mirror_neurons)

/** FEP orchestrator (free energy principle bridge coordination).
 *  Field: struct fep_orchestrator* fep_orchestrator */
#define BRAIN_ENSURE_FEP_ORCHESTRATOR(brain) \
    BRAIN_LAZY_INIT(brain, fep_orchestrator, nimcp_brain_factory_init_fep_orchestrator_subsystem)

/** Creative orchestrator (aesthetic evaluation, generation, validation).
 *  Field: struct creative_orchestrator* creative_orchestrator */
#define BRAIN_ENSURE_CREATIVE(brain) \
    BRAIN_LAZY_INIT(brain, creative_orchestrator, nimcp_brain_factory_init_creative_subsystem)

#endif /* NIMCP_BRAIN_LAZY_INIT_H */
