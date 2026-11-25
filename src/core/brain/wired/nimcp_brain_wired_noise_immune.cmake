# Sources for wired-module group BRAIN_WIRED_NOISE_IMMUNE (owned by this group only).
list(APPEND NIMCP_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/wired/nimcp_brain_wired_noise_immune.c
    # Pink-noise plasticity bridges (driven by the group's tick, bounded +/-2%)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/neuron_models/nimcp_sfa_pink_noise_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/astrocytes/nimcp_gliotransmission_pink_noise_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/bcm/nimcp_bcm_pink_noise_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/eligibility/nimcp_eligibility_pink_noise_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/homeostatic/nimcp_homeostatic_pink_noise_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/metaplasticity/nimcp_metaplasticity_pink_noise_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/structural/nimcp_structural_pink_noise_bridge.c
    # Immune bridges (no-ops unless brain->immune_system exists)
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/immune/nimcp_gpu_neuron_immune_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../information/immune/nimcp_cross_modal_immune_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../information/immune/nimcp_shannon_immune_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../optimization/immune/nimcp_quantum_annealing_immune_bridge.c
)
