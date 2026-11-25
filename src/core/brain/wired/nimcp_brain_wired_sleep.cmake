# Sources for wired-module group BRAIN_WIRED_SLEEP (owned by this group only).
# Only bridges whose target exists on the brain (and has an API to apply the
# bridge's effects) are built; see nimcp_brain_wired_sleep.c.
list(APPEND NIMCP_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/wired/nimcp_brain_wired_sleep.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../perception/sleep/nimcp_visual_cortex_sleep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/sleep/nimcp_cortical_layers_sleep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/sleep/nimcp_cortical_column_sleep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../perception/sleep/nimcp_retina_sleep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/adaptive/nimcp_adaptive_sleep_bridge.c
)
