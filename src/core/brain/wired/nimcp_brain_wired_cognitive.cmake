# Sources for wired-module group BRAIN_WIRED_COGNITIVE (owned by this group only).
# Not listed (superseded by built files that export the same symbols):
#   cognitive/mental_health/interventions.c           -> nimcp_mental_health.c
#   cognitive/wellbeing/nimcp_wellbeing_homeostasis.c -> nimcp_wellbeing_enhanced.c
list(APPEND NIMCP_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/wired/nimcp_brain_wired_cognitive.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_motor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/learning/nimcp_reasoning_learning.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/training/nimcp_learning_signal_adapter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/training/nimcp_weight_update_adapter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/training/nimcp_training_event_adapter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/adaptive/nimcp_weights.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/checkpointing/nimcp_checkpointing.c
)
