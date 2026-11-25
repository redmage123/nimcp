# Sources for wired-module group BRAIN_WIRED_LANGUAGE (owned by this group only).
# The 14 language<->cognitive bridges export one vtable each (lang_<mod>_bridge_vt);
# they no longer define grounded_language_integrate_* (those stay in
# nimcp_language_integration_stubs.c).
list(APPEND NIMCP_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/wired/nimcp_brain_wired_language.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_language_acc_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_language_amygdala_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_language_basal_ganglia_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_language_consciousness_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_language_episodic_replay_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_language_hamiltonian_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_language_hypothalamus_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_language_imagination_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_language_introspection_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_language_jepa_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_language_mirror_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_language_prime_resonance_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_language_tom_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_language_world_model_bridge.c
)
