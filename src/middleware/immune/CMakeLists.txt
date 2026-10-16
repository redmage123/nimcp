# Middleware Immune Integration Module
# Training-Immune Coordination Layer

add_library(nimcp_middleware_immune OBJECT
    nimcp_training_immune.c
    nimcp_thalamic_immune_bridge.c
    nimcp_population_coding_immune_bridge.c
    nimcp_sequence_immune_bridge.c
    nimcp_feature_extractor_immune_bridge.c
    nimcp_buffer_immune.c
    nimcp_pattern_immune.c
    nimcp_routing_immune.c
)

target_include_directories(nimcp_middleware_immune PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(nimcp_middleware_immune PUBLIC
    nimcp_cognitive_immune
    nimcp_middleware_training
)
