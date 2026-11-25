# Sources for wired-module group BRAIN_WIRED_COMPUTE (owned by this group only).
#
# Not listed (see the group report):
#   swarm/enhanced_hierarchy.c, swarm/enhanced_stats.c (deleted) — orphans of an abandoned
#     SRP split; every public symbol is already defined by the built
#     swarm/nimcp_swarm_consciousness_enhanced.c (duplicate-symbol link error).
#   python/nimcp_quantum_bindings.c — Python-extension fragment; belongs to the
#     nimcp_python target (#included from bindings/python/nimcp_python.c).
list(APPEND NIMCP_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/wired/nimcp_brain_wired_compute.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/neuralnet/nimcp_neuralnet_simd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../async/nimcp_semantic_compression.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../networking/nlp/nimcp_semantic_compression.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../lnn/nimcp_lnn_ternary.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../lnn/nimcp_lnn_wiring_ternary.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/topology/nimcp_community_detection_ternary.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/algorithms/nimcp_louvain.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/algorithms/nimcp_modularity.c
)
