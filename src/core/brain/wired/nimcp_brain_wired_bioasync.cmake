# Sources for wired-module group BRAIN_WIRED_BIOASYNC (owned by this group only).
# Per-domain bio-async bridges registered on brain->bio_router and drained from
# brain_wired_bioasync_tick(). Not included: middleware/events/nimcp_event_bus_async.c
# (targets the removed middleware event bus; its event_bus_* calls would bind to the
# incompatible core/events event bus exports).
list(APPEND NIMCP_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/wired/nimcp_brain_wired_bioasync.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../async/integration/nimcp_async_integration_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/integration/nimcp_core_bio_async_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/integration/nimcp_glial_bio_async_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/integration/nimcp_gpu_bio_async_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/integration/nimcp_language_bio_async_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../perception/integration/nimcp_perception_bio_async_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../quantum/integration/nimcp_quantum_bio_async_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/integration/nimcp_security_bio_async_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../sleep/integration/nimcp_sleep_bio_async_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/integration/nimcp_swarm_bio_async_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/integration/nimcp_utils_bio_async_bridge.c
)
