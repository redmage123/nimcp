#!/usr/bin/env perl
#
# NIMCP Perl bindings using FFI::Platypus
# Wraps the unified nimcp.h C API (v2.6.4)
#

package NIMCP;

use 5.010;
use strict;
use warnings;

our $VERSION = '2.6.4';

use FFI::Platypus 2.00;
use FFI::Platypus::Memory qw( malloc calloc free memcpy );
use FFI::Platypus::Buffer qw( scalar_to_buffer buffer_to_scalar );
use FFI::Platypus::Record;

# Record type for nimcp_oscillation_phasor_t (returned by value)
package NIMCP::PhasorRecord {
    use FFI::Platypus::Record;
    record_layout_1(
        float => 'amplitude',
        float => 'phase',
    );
}
package NIMCP;

# ============================================================================
# FFI Setup
# ============================================================================

my $ffi = FFI::Platypus->new(api => 2, lang => 'C');

# Find libnimcp.so: check LD_LIBRARY_PATH, then common locations
my @search_paths;
if ($ENV{LD_LIBRARY_PATH}) {
    push @search_paths, map { "$_/libnimcp.so" } split(/:/, $ENV{LD_LIBRARY_PATH});
}
push @search_paths, qw(
    libnimcp.so
    ./libnimcp.so
    ../lib/libnimcp.so
);

my $found_lib;
for my $path (@search_paths) {
    if (-f $path) {
        $found_lib = $path;
        last;
    }
}
$ffi->lib($found_lib // 'libnimcp.so');

# ============================================================================
# Constants
# ============================================================================

use constant {
    # Brain sizes
    BRAIN_TINY   => 0,
    BRAIN_SMALL  => 1,
    BRAIN_MEDIUM => 2,
    BRAIN_LARGE  => 3,

    # Task types
    TASK_CLASSIFICATION   => 0,
    TASK_REGRESSION       => 1,
    TASK_PATTERN_MATCHING => 2,
    TASK_SEQUENCE         => 3,
    TASK_ASSOCIATION      => 4,

    # Network types
    NETWORK_ADAPTIVE => 0,
    NETWORK_SNN      => 1,
    NETWORK_LNN      => 2,
    NETWORK_CNN      => 3,
    NETWORK_HYBRID   => 4,

    # SNN training methods
    SNN_TRAIN_STDP        => 0,
    SNN_TRAIN_R_STDP      => 1,
    SNN_TRAIN_EPROP       => 2,
    SNN_TRAIN_SURROGATE   => 3,
    SNN_TRAIN_HOMEOSTATIC => 4,

    # LNN training methods
    LNN_TRAIN_ADJOINT => 0,
    LNN_TRAIN_BPTT    => 1,
    LNN_TRAIN_RTRL    => 2,
    LNN_TRAIN_EPROP   => 3,

    # Status codes
    OK            => 0,
    ERROR         => 1000,
    ERROR_NULL    => 1003,
    ERROR_INVALID => 1004,
    ERROR_MEMORY  => 2000,
    ERROR_IO      => 4000,

    # Loss types
    LOSS_MSE           => 0,
    LOSS_CROSS_ENTROPY => 1,
    LOSS_BINARY_CE     => 2,
    LOSS_HUBER         => 3,
    LOSS_MAE           => 4,
    LOSS_FOCAL         => 5,
    LOSS_KL_DIV        => 6,

    # Optimizer types
    OPT_SGD      => 0,
    OPT_MOMENTUM => 1,
    OPT_ADAM     => 2,
    OPT_ADAMW    => 3,
    OPT_RMSPROP  => 4,
    OPT_ADAGRAD  => 5,

    # Scheduler types
    SCHED_CONSTANT          => 0,
    SCHED_STEP              => 1,
    SCHED_EXPONENTIAL       => 2,
    SCHED_COSINE            => 3,
    SCHED_WARMUP_COSINE     => 4,
    SCHED_REDUCE_ON_PLATEAU => 5,
    SCHED_CYCLIC            => 6,

    # Callback events
    CB_STEP_COMPLETE   => 0,
    CB_EPOCH_COMPLETE  => 1,
    CB_LOSS_COMPUTED   => 2,
    CB_WEIGHTS_UPDATED => 3,
    CB_LR_CHANGED      => 4,
    CB_CONVERGENCE     => 5,
    CB_DIVERGENCE      => 6,
    CB_CHECKPOINT      => 7,
    CB_EVENT_COUNT     => 8,

    # Callback actions
    CB_ACTION_CONTINUE    => 0,
    CB_ACTION_STOP        => 1,
    CB_ACTION_SKIP        => 2,
    CB_ACTION_ROLLBACK    => 3,
    CB_ACTION_REDUCE_LR   => 4,
    CB_ACTION_INCREASE_LR => 5,

    # Cognitive modules
    MODULE_NONE            => 0,
    MODULE_PERCEPTION      => 1,
    MODULE_WORKING_MEMORY  => 2,
    MODULE_EXECUTIVE       => 3,
    MODULE_THEORY_OF_MIND  => 4,
    MODULE_ETHICS          => 5,
    MODULE_ATTENTION       => 6,
    MODULE_EMOTION         => 7,
    MODULE_SALIENCE        => 8,
    MODULE_MOTOR           => 9,
    MODULE_LANGUAGE        => 10,
    MODULE_METACOGNITION   => 11,
    MODULE_CURIOSITY       => 12,
    MODULE_INTROSPECTION   => 13,
    MODULE_PREDICTIVE      => 14,
    MODULE_CONSOLIDATION   => 15,
    MODULE_EPISODIC_MEMORY => 16,
    MODULE_SEMANTIC_MEMORY => 17,
    MODULE_WELLBEING       => 18,
    MODULE_MENTAL_HEALTH   => 19,
    MODULE_GOAL_MOTIVATION => 20,
    MODULE_COGNITIVE_CONTROL => 21,
    MODULE_CUSTOM_START    => 100,
};

# ============================================================================
# FFI Function Declarations
# ============================================================================
# Types: float[] = input float array (pass arrayref)
#        float*  = output pointer to single float (pass \$scalar)
#        opaque  = raw C pointer (handle types, buffers)
#        string  = input const char* (pass Perl string)

# Library lifecycle
$ffi->attach('nimcp_init'        => []           => 'int');
$ffi->attach('nimcp_shutdown'    => []           => 'void');
$ffi->attach('nimcp_version'     => []           => 'string');
$ffi->attach('nimcp_version_int' => []           => 'int');
$ffi->attach('nimcp_get_error'   => []           => 'string');

# Brain core
$ffi->attach('nimcp_brain_create'  => ['string', 'int', 'int', 'uint32', 'uint32'] => 'opaque');
$ffi->attach('nimcp_brain_destroy' => ['opaque'] => 'void');
$ffi->attach('nimcp_brain_learn_example' =>
    ['opaque', 'float[]', 'uint32', 'string', 'float'] => 'int');
$ffi->attach('nimcp_brain_predict' =>
    ['opaque', 'float[]', 'uint32', 'opaque', 'float*'] => 'int');
$ffi->attach('nimcp_brain_infer' =>
    ['opaque', 'float[]', 'uint32', 'float[]', 'uint32'] => 'int');
$ffi->attach('nimcp_brain_save' => ['opaque', 'string'] => 'int');
$ffi->attach('nimcp_brain_load' => ['string'] => 'opaque');
$ffi->attach('nimcp_brain_create_from_config' => ['string'] => 'opaque');

# Brain creation variants
$ffi->attach('nimcp_brain_create_full' =>
    ['string', 'int', 'uint32', 'uint32', 'uint32'] => 'opaque');
$ffi->attach('nimcp_brain_create_with_neurons' =>
    ['string', 'int', 'uint32', 'uint32', 'uint32'] => 'opaque');

# Brain training
$ffi->attach('nimcp_brain_configure_training' => ['opaque', 'opaque'] => 'int');
$ffi->attach('nimcp_brain_train_step' =>
    ['opaque', 'float[]', 'uint32', 'float[]', 'uint32', 'opaque'] => 'int');
$ffi->attach('nimcp_brain_train_batch' =>
    ['opaque', 'float[]', 'float[]', 'uint32', 'uint32', 'uint32', 'opaque'] => 'int');
$ffi->attach('nimcp_brain_get_training_stats' =>
    ['opaque', 'uint64*', 'float*', 'float*'] => 'int');
$ffi->attach('nimcp_brain_step_scheduler' => ['opaque', 'float'] => 'float');

# Brain learning — vector / batch / knowledge / language
$ffi->attach('nimcp_brain_learn_vector' =>
    ['opaque', 'float[]', 'uint32', 'float[]', 'uint32', 'string', 'float'] => 'int');
$ffi->attach('nimcp_brain_learn_vector_batch' =>
    ['opaque', 'opaque', 'opaque', 'uint32', 'uint32', 'uint32', 'float'] => 'float');
$ffi->attach('nimcp_brain_learn_batch' =>
    ['opaque', 'opaque', 'opaque', 'opaque', 'opaque', 'uint32', 'opaque'] => 'int');
$ffi->attach('nimcp_brain_learn_knowledge' =>
    ['opaque', 'string', 'int'] => 'int');
$ffi->attach('nimcp_brain_learn_language' =>
    ['opaque', 'string', 'float*'] => 'int');
$ffi->attach('nimcp_brain_learn_language_pair' =>
    ['opaque', 'string', 'string', 'float', 'float*'] => 'int');

# Brain cognitive training
$ffi->attach('nimcp_brain_train_cognitive' =>
    ['opaque', 'string', 'int', 'string', 'float', 'float*'] => 'int');
$ffi->attach('nimcp_brain_train_language' =>
    ['opaque', 'string', 'string', 'float', 'float*'] => 'int');

# Brain experience
$ffi->attach('nimcp_brain_experience' =>
    ['opaque', 'float[]', 'uint32', 'float[]', 'uint32', 'float', 'opaque'] => 'int');
$ffi->attach('nimcp_brain_experience_configure' =>
    ['opaque', 'opaque'] => 'int');
$ffi->attach('nimcp_brain_experience_correct' =>
    ['opaque', 'float[]', 'uint32'] => 'float');
$ffi->attach('nimcp_brain_experience_attend' =>
    ['opaque', 'string', 'float'] => 'int');

# Brain metrics / stats
$ffi->attach('nimcp_brain_get_last_loss'          => ['opaque'] => 'float');
$ffi->attach('nimcp_brain_get_last_gradient_norm'  => ['opaque'] => 'float');
$ffi->attach('nimcp_brain_get_accuracy'            => ['opaque'] => 'float');
$ffi->attach('nimcp_brain_get_avatar_state'        => ['opaque', 'opaque'] => 'int');
$ffi->attach('nimcp_brain_get_cognitive_stats'     =>
    ['opaque', 'opaque', 'opaque', 'uint32*'] => 'int');
$ffi->attach('nimcp_brain_get_last_transcript'     =>
    ['opaque', 'opaque', 'opaque', 'opaque', 'opaque', 'uint32'] => 'uint32');
$ffi->attach('nimcp_brain_get_network_metrics'     =>
    ['opaque', 'float*', 'float*', 'float*', 'float*',
     'uint64*', 'uint64*', 'uint64*', 'uint64*'] => 'int');
$ffi->attach('nimcp_brain_get_cortex_cnn_metrics'  =>
    ['opaque', 'opaque', 'opaque', 'opaque', 'opaque', 'opaque', 'uint32*'] => 'int');

# Brain prediction variants
$ffi->attach('nimcp_brain_predict_fast' =>
    ['opaque', 'float[]', 'uint32', 'opaque', 'float*'] => 'int');
$ffi->attach('nimcp_brain_predict_in_domain' =>
    ['opaque', 'float[]', 'uint32', 'string', 'opaque', 'float*'] => 'int');

# Brain decide / speak / generate / comprehend / produce / grounded / creative
$ffi->attach('nimcp_brain_decide_full' =>
    ['opaque', 'float[]', 'uint32',
     'opaque', 'float*', 'opaque',
     'float[]', 'uint32*', 'uint32*', 'float*', 'uint64*'] => 'int');
$ffi->attach('nimcp_brain_speak' =>
    ['opaque', 'float[]', 'uint32', 'opaque', 'uint32', 'float*', 'float*'] => 'int');
$ffi->attach('nimcp_brain_generate_text' =>
    ['opaque', 'string', 'float[]', 'uint32', 'opaque', 'uint32', 'float*', 'float*'] => 'int');
$ffi->attach('nimcp_brain_comprehend' =>
    ['opaque', 'string', 'float[]', 'uint32', 'float*'] => 'int');
$ffi->attach('nimcp_brain_produce_text' =>
    ['opaque', 'float[]', 'uint32', 'opaque', 'uint32', 'float*'] => 'int');
$ffi->attach('nimcp_brain_grounded_respond' =>
    ['opaque', 'string', 'opaque', 'uint32', 'float*'] => 'int');
$ffi->attach('nimcp_brain_creative_blend' =>
    ['opaque', 'float[]', 'float[]', 'uint32', 'float', 'opaque', 'uint32'] => 'int');
$ffi->attach('nimcp_brain_ground_word' =>
    ['opaque', 'string', 'float[]', 'uint32', 'uint32', 'float'] => 'int');

# Brain freeze / frozen
$ffi->attach('nimcp_brain_freeze'    => ['opaque'] => 'int');
$ffi->attach('nimcp_brain_is_frozen' => ['opaque'] => 'int');

# Brain enable flags
$ffi->attach('nimcp_brain_enable_mixed_precision' =>
    ['opaque', 'int'] => 'int');
$ffi->attach('nimcp_brain_enable_gradient_checkpointing' =>
    ['opaque', 'int', 'uint32'] => 'int');
$ffi->attach('nimcp_brain_enable_hemispheric' =>
    ['opaque', 'int'] => 'int');
$ffi->attach('nimcp_brain_enable_recurrent' =>
    ['opaque', 'int', 'uint32', 'float', 'float'] => 'int');
$ffi->attach('nimcp_brain_enable_bptt' =>
    ['opaque', 'int', 'uint32', 'float'] => 'int');
$ffi->attach('nimcp_brain_enable_biological_plasticity' =>
    ['opaque', 'int'] => 'int');
$ffi->attach('nimcp_brain_enable_multi_network' =>
    ['opaque'] => 'int');

# Brain set / config
$ffi->attach('nimcp_brain_set_fast_training'  => ['opaque', 'int'] => 'int');
$ffi->attach('nimcp_brain_set_task_type'      => ['opaque', 'string'] => 'int');
$ffi->attach('nimcp_brain_set_training_mode'  => ['opaque', 'int'] => 'void');
$ffi->attach('nimcp_brain_set_network_ablation' =>
    ['opaque', 'int', 'int', 'int'] => 'void');

# Per-network training toggles (runtime-dynamic, no rebuild required)
$ffi->attach('nimcp_brain_set_train_ann' => ['opaque', 'int'] => 'void');
$ffi->attach('nimcp_brain_get_train_ann' => ['opaque'] => 'int');
$ffi->attach('nimcp_brain_set_train_cnn' => ['opaque', 'int'] => 'void');
$ffi->attach('nimcp_brain_get_train_cnn' => ['opaque'] => 'int');
$ffi->attach('nimcp_brain_set_train_snn' => ['opaque', 'int'] => 'void');
$ffi->attach('nimcp_brain_get_train_snn' => ['opaque'] => 'int');
$ffi->attach('nimcp_brain_set_train_lnn' => ['opaque', 'int'] => 'void');
$ffi->attach('nimcp_brain_get_train_lnn' => ['opaque'] => 'int');
$ffi->attach('nimcp_brain_set_snn_only_recovery' => ['opaque', 'int'] => 'void');
$ffi->attach('nimcp_brain_get_snn_only_recovery' => ['opaque'] => 'int');
$ffi->attach('nimcp_brain_set_ensemble_warmup_scale' => ['opaque', 'float'] => 'void');
$ffi->attach('nimcp_brain_get_ensemble_warmup_scale' => ['opaque'] => 'float');

# Brain sensory / cortex
$ffi->attach('nimcp_brain_submit_sensory' =>
    ['opaque', 'string', 'float[]', 'uint32', 'uint32', 'uint32', 'uint32', 'uint32'] => 'int');
$ffi->attach('nimcp_brain_visual_cortex_process' =>
    ['opaque', 'float[]', 'uint32', 'uint32', 'uint32', 'uint32',
     'float[]', 'uint32', 'uint32*'] => 'int');

# Brain neuromodulation / sleep / substrate
$ffi->attach('nimcp_brain_medulla_get_arousal'    => ['opaque'] => 'float');
$ffi->attach('nimcp_brain_bg_get_dopamine'        => ['opaque'] => 'float');
$ffi->attach('nimcp_brain_sleep_get_pressure'     => ['opaque'] => 'float');
$ffi->attach('nimcp_brain_substrate_get_health'   =>
    ['opaque', 'opaque', 'uint32'] => 'int');

# Brain sub-network creation / stats
$ffi->attach('nimcp_brain_lnn_create' =>
    ['opaque', 'uint32', 'uint32', 'uint32', 'uint32'] => 'int');
$ffi->attach('nimcp_brain_lnn_get_stats' =>
    ['opaque', 'uint64*', 'uint64*', 'uint64*', 'float*',
     'float*', 'float*', 'uint32*', 'uint32*'] => 'int');
$ffi->attach('nimcp_brain_snn_get_stats' =>
    ['opaque', 'uint64*', 'uint64*', 'float*', 'float*',
     'float*', 'uint32*', 'uint32*', 'opaque', 'opaque'] => 'int');
$ffi->attach('nimcp_snn_set_input_scale' => ['float'] => 'void');
$ffi->attach('nimcp_snn_get_input_scale' => [] => 'float');
$ffi->attach('nimcp_brain_cnn_get_stats' =>
    ['opaque', 'uint32*', 'opaque', 'uint32*', 'opaque'] => 'int');

# Brain rubric
$ffi->attach('nimcp_brain_rubric' => ['opaque', 'opaque'] => 'int');

# Brain cloud
$ffi->attach('nimcp_brain_connect_cloud' =>
    ['opaque', 'opaque', 'float', 'int'] => 'int');
$ffi->attach('nimcp_brain_disconnect_cloud' => ['opaque'] => 'int');

# Brain callbacks
$ffi->attach('nimcp_brain_enable_callbacks'    => ['opaque', 'opaque'] => 'int');
$ffi->attach('nimcp_brain_disable_callbacks'   => ['opaque'] => 'int');
$ffi->attach('nimcp_brain_register_callback'   =>
    ['opaque', 'int', 'opaque', 'opaque', 'string'] => 'uint32');
$ffi->attach('nimcp_brain_unregister_callback' => ['opaque', 'uint32'] => 'int');
$ffi->attach('nimcp_brain_get_callback_stats'  =>
    ['opaque', 'uint64*', 'float*', 'uint32*'] => 'int');

# Brain resize
$ffi->attach('nimcp_brain_resize'    => ['opaque', 'uint32'] => 'int');
$ffi->attach('nimcp_brain_auto_resize' => ['opaque'] => 'int');
$ffi->attach('nimcp_brain_get_neuron_count' => ['opaque'] => 'uint32');
$ffi->attach('nimcp_brain_get_utilization_metrics' =>
    ['opaque', 'float*', 'float*'] => 'int');

# Brain named snapshots
$ffi->attach('nimcp_brain_snapshot_save'    => ['opaque', 'string', 'string'] => 'int');
$ffi->attach('nimcp_brain_snapshot_restore' => ['opaque', 'string'] => 'opaque');
$ffi->attach('nimcp_brain_snapshot_list'    =>
    ['opaque', 'opaque', 'uint32', 'uint32*'] => 'int');
$ffi->attach('nimcp_brain_snapshot_delete'  => ['opaque', 'string'] => 'int');

# Brain probe
$ffi->attach('nimcp_brain_probe'           => ['opaque', 'opaque'] => 'int');
$ffi->attach('nimcp_brain_broadcast_probe' => ['opaque'] => 'int');

# Brain COW
$ffi->attach('nimcp_brain_clone_cow'        => ['opaque'] => 'opaque');
$ffi->attach('nimcp_brain_snapshot_cow'     => ['opaque'] => 'opaque');
$ffi->attach('nimcp_brain_restore_cow'      => ['opaque', 'opaque'] => 'int');
$ffi->attach('nimcp_brain_snapshot_destroy' => ['opaque'] => 'void');

# Brain working memory
$ffi->attach('nimcp_brain_working_memory_add' =>
    ['opaque', 'float[]', 'uint32', 'float'] => 'int');
$ffi->attach('nimcp_brain_working_memory_get' =>
    ['opaque', 'uint32', 'uint32*'] => 'opaque');
$ffi->attach('nimcp_brain_working_memory_stats' =>
    ['opaque', 'uint32*', 'uint32*'] => 'int');
$ffi->attach('nimcp_brain_working_memory_refresh' => ['opaque', 'uint32'] => 'int');

# Brain workspace
$ffi->attach('nimcp_brain_workspace_compete' =>
    ['opaque', 'int', 'float[]', 'uint32', 'float'] => 'int');
$ffi->attach('nimcp_brain_workspace_read' =>
    ['opaque', 'float[]', 'uint32', 'uint32*', 'int*'] => 'int');
$ffi->attach('nimcp_brain_workspace_subscribe'   => ['opaque', 'int'] => 'int');
$ffi->attach('nimcp_brain_workspace_unsubscribe' => ['opaque', 'int'] => 'int');
$ffi->attach('nimcp_brain_workspace_has_broadcast' =>
    ['opaque', 'opaque'] => 'int');
$ffi->attach('nimcp_brain_workspace_stats' =>
    ['opaque', 'uint32*', 'uint32*', 'float*'] => 'int');

# Brain oscillations
$ffi->attach('nimcp_enable_complex_oscillations'    => ['opaque', 'int'] => 'int');
$ffi->attach('nimcp_is_complex_oscillations_enabled' => ['opaque'] => 'int');
$ffi->type('record(NIMCP::PhasorRecord)' => 'nimcp_phasor_t');
$ffi->attach('nimcp_get_oscillation_phasor' => ['opaque', 'uint32'] => 'nimcp_phasor_t');
$ffi->attach('nimcp_get_phase_coherence' => ['opaque', 'uint32[]', 'uint32'] => 'float');
$ffi->attach('nimcp_get_pac_modulation'  => ['opaque', 'float', 'float'] => 'float');

# Network API
$ffi->attach('nimcp_network_create'  =>
    ['uint32', 'uint32', 'uint32', 'float'] => 'opaque');
$ffi->attach('nimcp_network_destroy' => ['opaque'] => 'void');
$ffi->attach('nimcp_network_forward' =>
    ['opaque', 'float[]', 'uint32', 'float[]', 'uint32'] => 'int');
$ffi->attach('nimcp_network_train' =>
    ['opaque', 'float[]', 'uint32', 'float[]', 'uint32'] => 'int');

# Ethics API
$ffi->attach('nimcp_ethics_create'  => []     => 'opaque');
$ffi->attach('nimcp_ethics_destroy' => ['opaque'] => 'void');
$ffi->attach('nimcp_ethics_check'   =>
    ['opaque', 'float[]', 'uint32', 'float*'] => 'int');

# Knowledge API
$ffi->attach('nimcp_knowledge_create'   => []     => 'opaque');
$ffi->attach('nimcp_knowledge_destroy'  => ['opaque'] => 'void');
$ffi->attach('nimcp_knowledge_add_fact' =>
    ['opaque', 'string', 'string', 'string'] => 'int');
$ffi->attach('nimcp_knowledge_query' =>
    ['opaque', 'string', 'opaque', 'uint32'] => 'int');

# Edge Brain
$ffi->attach('nimcp_edge_brain_resize' => ['opaque', 'opaque'] => 'int');
$ffi->attach('nimcp_edge_brain_resize_check' =>
    ['opaque', 'opaque', 'opaque'] => 'int');
$ffi->attach('nimcp_edge_score_neuron_importance' =>
    ['opaque', 'opaque', 'uint32'] => 'int');
$ffi->attach('nimcp_brain_distill' =>
    ['opaque', 'opaque', 'opaque', 'opaque'] => 'int');
$ffi->attach('nimcp_brain_optimize_for_device' =>
    ['opaque', 'opaque', 'opaque', 'opaque'] => 'int');
$ffi->attach('nimcp_brain_quantize' => ['opaque', 'opaque'] => 'int');
$ffi->attach('nimcp_resize_config_default' => [] => 'opaque');
$ffi->attach('nimcp_distill_config_default' => [] => 'opaque');
$ffi->attach('nimcp_quantize_config_default' => [] => 'opaque');
$ffi->attach('nimcp_device_profile_default' => [] => 'opaque');

# Swarm Runtime
$ffi->attach('nimcp_swarm_master_create' => ['opaque', 'opaque'] => 'opaque');
$ffi->attach('nimcp_swarm_master_destroy' => ['opaque'] => 'void');
$ffi->attach('nimcp_swarm_master_start' => ['opaque'] => 'int');
$ffi->attach('nimcp_swarm_master_stop' => ['opaque'] => 'int');
$ffi->attach('nimcp_swarm_master_kick' => ['opaque', 'uint32'] => 'int');
$ffi->attach('nimcp_swarm_master_force_sync' => ['opaque'] => 'int');
$ffi->attach('nimcp_swarm_master_get_peer_count' => ['opaque'] => 'uint32');
$ffi->attach('nimcp_swarm_master_config_default' => [] => 'opaque');
$ffi->attach('nimcp_swarm_edge_create' => ['opaque', 'opaque'] => 'opaque');
$ffi->attach('nimcp_swarm_edge_destroy' => ['opaque'] => 'void');
$ffi->attach('nimcp_swarm_edge_start' => ['opaque'] => 'int');
$ffi->attach('nimcp_swarm_edge_stop' => ['opaque'] => 'int');
$ffi->attach('nimcp_swarm_edge_is_connected' => ['opaque'] => 'int');
$ffi->attach('nimcp_swarm_edge_submit_gradients' => ['opaque', 'opaque', 'uint32'] => 'int');
$ffi->attach('nimcp_swarm_edge_config_default' => [] => 'opaque');

# Sensor Hub
$ffi->attach('nimcp_sensor_hub_create' => ['uint32'] => 'opaque');
$ffi->attach('nimcp_sensor_hub_destroy' => ['opaque'] => 'void');
$ffi->attach('nimcp_sensor_register' => ['opaque', 'opaque'] => 'int');
$ffi->attach('nimcp_sensor_submit_reading' => ['opaque', 'opaque'] => 'int');
$ffi->attach('nimcp_sensor_compose_feature_vector' => ['opaque', 'opaque', 'uint32'] => 'int');
$ffi->attach('nimcp_sensor_get_count' => ['opaque'] => 'uint32');

# Safety Watchdog
$ffi->attach('nimcp_watchdog_create' => ['opaque'] => 'opaque');
$ffi->attach('nimcp_watchdog_destroy' => ['opaque'] => 'void');
$ffi->attach('nimcp_watchdog_arm' => ['opaque'] => 'int');
$ffi->attach('nimcp_watchdog_disarm' => ['opaque'] => 'int');
$ffi->attach('nimcp_watchdog_heartbeat' => ['opaque'] => 'void');
$ffi->attach('nimcp_watchdog_validate_output' => ['opaque', 'opaque', 'uint32'] => 'int');
$ffi->attach('nimcp_watchdog_get_safe_output' => ['opaque', 'opaque', 'uint32'] => 'int');
$ffi->attach('nimcp_watchdog_estop' => ['opaque'] => 'void');
$ffi->attach('nimcp_watchdog_reset' => ['opaque'] => 'int');
$ffi->attach('nimcp_watchdog_get_state' => ['opaque'] => 'int');
$ffi->attach('nimcp_watchdog_state_name' => ['int'] => 'string');
$ffi->attach('nimcp_watchdog_config_default' => [] => 'opaque');

# ROS 2 Bridge
$ffi->attach('nimcp_ros2_bridge_create' => ['opaque', 'opaque'] => 'opaque');
$ffi->attach('nimcp_ros2_bridge_destroy' => ['opaque'] => 'void');
$ffi->attach('nimcp_ros2_bridge_start' => ['opaque'] => 'int');
$ffi->attach('nimcp_ros2_bridge_stop' => ['opaque'] => 'int');
$ffi->attach('nimcp_ros2_bridge_inject_sensor' => ['opaque', 'string', 'opaque', 'uint32'] => 'int');
$ffi->attach('nimcp_ros2_bridge_get_last_cmd' => ['opaque', 'opaque', 'uint32'] => 'int');
$ffi->attach('nimcp_ros2_config_default' => [] => 'opaque');

# MAVLink Bridge
$ffi->attach('nimcp_mavlink_bridge_create' => ['opaque'] => 'opaque');
$ffi->attach('nimcp_mavlink_bridge_destroy' => ['opaque'] => 'void');
$ffi->attach('nimcp_mavlink_bridge_connect' => ['opaque'] => 'int');
$ffi->attach('nimcp_mavlink_bridge_disconnect' => ['opaque'] => 'int');
$ffi->attach('nimcp_mavlink_bridge_start' => ['opaque'] => 'int');
$ffi->attach('nimcp_mavlink_bridge_stop' => ['opaque'] => 'int');
$ffi->attach('nimcp_mavlink_set_velocity' => ['opaque', 'float', 'float', 'float', 'float'] => 'int');
$ffi->attach('nimcp_mavlink_arm' => ['opaque', 'int'] => 'int');
$ffi->attach('nimcp_mavlink_takeoff' => ['opaque', 'float'] => 'int');
$ffi->attach('nimcp_mavlink_land' => ['opaque'] => 'int');
$ffi->attach('nimcp_mavlink_goto' => ['opaque', 'double', 'double', 'float'] => 'int');
$ffi->attach('nimcp_mavlink_rtl' => ['opaque'] => 'int');
$ffi->attach('nimcp_mavlink_compose_features' => ['opaque', 'opaque', 'uint32'] => 'int');
$ffi->attach('nimcp_mavlink_config_default' => [] => 'opaque');

# Memory Store / OOD / Audit (brain-handle wrappers)
$ffi->attach('nimcp_brain_memory_store_stats' => ['opaque', 'opaque'] => 'int');
$ffi->attach('nimcp_brain_memory_search_text' =>
    ['opaque', 'string', 'uint32', 'opaque', 'uint32*'] => 'int');
$ffi->attach('nimcp_brain_memory_search_similar' =>
    ['opaque', 'float[]', 'uint32', 'uint32', 'opaque', 'opaque', 'uint32*'] => 'int');
$ffi->attach('nimcp_brain_memory_is_healthy' => ['opaque'] => 'int');
$ffi->attach('nimcp_brain_ood_stats' => ['opaque', 'opaque'] => 'int');
$ffi->attach('nimcp_brain_audit_log' =>
    ['opaque', 'string', 'uint32', 'string'] => 'int');
$ffi->attach('nimcp_brain_audit_search' =>
    ['opaque', 'uint32', 'uint32', 'opaque', 'opaque', 'uint32*'] => 'int');

# ============================================================================
# Helper functions
# ============================================================================

sub _check_status {
    my ($status, $context) = @_;
    return if $status == 0;
    my $msg = nimcp_get_error() // 'Unknown error';
    die "NIMCP error ($status) in $context: $msg\n";
}

# Allocate a zeroed C buffer
sub _alloc_buf {
    my ($size) = @_;
    return calloc(1, $size);
}

# ============================================================================
# Library Lifecycle
# ============================================================================

sub init {
    my $status = nimcp_init();
    _check_status($status, 'init');
    return 1;
}

sub shutdown { nimcp_shutdown() }

sub version { return nimcp_version() }

sub version_int { return nimcp_version_int() }

sub get_error { return nimcp_get_error() }

# ============================================================================
# NIMCP::Brain
# ============================================================================

package NIMCP::Brain;

sub new {
    my ($class, %args) = @_;
    my $name        = $args{name}        // 'default';
    my $size        = $args{size}        // NIMCP::BRAIN_SMALL;
    my $task        = $args{task}        // NIMCP::TASK_CLASSIFICATION;
    my $num_inputs  = $args{num_inputs}  // 10;
    my $num_outputs = $args{num_outputs} // 10;

    my $handle = NIMCP::nimcp_brain_create($name, $size, $task, $num_inputs, $num_outputs);
    die "Failed to create brain: " . NIMCP::nimcp_get_error() unless $handle;

    return bless {
        _handle     => $handle,
        _num_inputs => $num_inputs,
        _num_outputs => $num_outputs,
        _callbacks  => [],
    }, $class;
}

sub create_with_neurons {
    my ($class, %args) = @_;
    my $name         = $args{name}         // 'default';
    my $task         = $args{task}         // NIMCP::TASK_CLASSIFICATION;
    my $num_inputs   = $args{num_inputs}   // 10;
    my $num_outputs  = $args{num_outputs}  // 10;
    my $neuron_count = $args{neuron_count} // die "neuron_count required";

    my $handle = NIMCP::nimcp_brain_create_with_neurons(
        $name, $task, $num_inputs, $num_outputs, $neuron_count
    );
    die "Failed to create brain: " . NIMCP::nimcp_get_error() unless $handle;

    return bless {
        _handle      => $handle,
        _num_inputs  => $num_inputs,
        _num_outputs => $num_outputs,
        _callbacks   => [],
    }, $class;
}

sub create_full {
    my ($class, %args) = @_;
    my $name         = $args{name}         // 'default';
    my $task         = $args{task}         // NIMCP::TASK_CLASSIFICATION;
    my $num_inputs   = $args{num_inputs}   // 10;
    my $num_outputs  = $args{num_outputs}  // 10;
    my $neuron_count = $args{neuron_count} // die "neuron_count required";

    my $handle = NIMCP::nimcp_brain_create_full(
        $name, $task, $num_inputs, $num_outputs, $neuron_count
    );
    die "Failed to create brain: " . NIMCP::nimcp_get_error() unless $handle;

    return bless {
        _handle      => $handle,
        _num_inputs  => $num_inputs,
        _num_outputs => $num_outputs,
        _callbacks   => [],
    }, $class;
}

sub load {
    my ($class, $filepath) = @_;
    die "filepath required" unless $filepath;
    my $handle = NIMCP::nimcp_brain_load($filepath);
    die "Failed to load brain: " . NIMCP::nimcp_get_error() unless $handle;
    return bless { _handle => $handle, _callbacks => [] }, $class;
}

sub create_from_config {
    my ($class, $config_path) = @_;
    die "config_path required" unless $config_path;
    my $handle = NIMCP::nimcp_brain_create_from_config($config_path);
    die "Failed to create brain from config: " . NIMCP::nimcp_get_error() unless $handle;
    return bless { _handle => $handle, _callbacks => [] }, $class;
}

sub learn {
    my ($self, $features, $label, $confidence) = @_;
    $confidence //= 1.0;
    die "features must be array ref" unless ref $features eq 'ARRAY';
    my $n = scalar @$features;
    my $status = NIMCP::nimcp_brain_learn_example(
        $self->{_handle}, $features, $n, $label, $confidence
    );
    NIMCP::_check_status($status, 'learn');
    return $self;
}

sub learn_vector {
    my ($self, $features, $target, $label, $confidence) = @_;
    die "features must be array ref" unless ref $features eq 'ARRAY';
    die "target must be array ref" unless ref $target eq 'ARRAY';
    $label //= undef;
    $confidence //= 1.0;
    my $nf = scalar @$features;
    my $nt = scalar @$target;
    my $status = NIMCP::nimcp_brain_learn_vector(
        $self->{_handle}, $features, $nf, $target, $nt, $label, $confidence
    );
    NIMCP::_check_status($status, 'learn_vector');
    return $self;
}

sub learn_vector_batch {
    my ($self, $features_2d, $targets_2d, $learning_rate) = @_;
    die "features_2d must be array ref" unless ref $features_2d eq 'ARRAY';
    die "targets_2d must be array ref"  unless ref $targets_2d eq 'ARRAY';
    $learning_rate //= 0.001;
    my $num_examples  = scalar @$features_2d;
    my $num_features  = scalar @{$features_2d->[0]};
    my $target_size   = scalar @{$targets_2d->[0]};
    # Pack feature pointers and target pointers into contiguous arrays
    # For FFI we flatten and pass as opaque pointers to pointer arrays
    my @feat_flat = map { @$_ } @$features_2d;
    my @tgt_flat  = map { @$_ } @$targets_2d;
    my $feat_packed = pack("f*", @feat_flat);
    my $tgt_packed  = pack("f*", @tgt_flat);
    # Build array of pointers (each pointing into the packed buffer)
    my ($feat_buf, $feat_buf_sz) = FFI::Platypus::Buffer::scalar_to_buffer($feat_packed);
    my ($tgt_buf, $tgt_buf_sz)   = FFI::Platypus::Buffer::scalar_to_buffer($tgt_packed);
    my $ptr_size = $ffi->sizeof('opaque');
    my $feat_ptrs = '';
    my $tgt_ptrs  = '';
    for my $i (0 .. $num_examples - 1) {
        $feat_ptrs .= pack($ptr_size == 8 ? 'Q' : 'L', $feat_buf + $i * $num_features * 4);
        $tgt_ptrs  .= pack($ptr_size == 8 ? 'Q' : 'L', $tgt_buf  + $i * $target_size * 4);
    }
    my ($feat_ptrs_buf, undef) = FFI::Platypus::Buffer::scalar_to_buffer($feat_ptrs);
    my ($tgt_ptrs_buf, undef)  = FFI::Platypus::Buffer::scalar_to_buffer($tgt_ptrs);
    my $avg_loss = NIMCP::nimcp_brain_learn_vector_batch(
        $self->{_handle}, $feat_ptrs_buf, $tgt_ptrs_buf,
        $num_features, $target_size, $num_examples, $learning_rate
    );
    return $avg_loss;
}

sub learn_batch {
    my ($self, $features_2d, $labels, $confidences) = @_;
    die "features_2d must be array ref" unless ref $features_2d eq 'ARRAY';
    die "labels must be array ref"      unless ref $labels eq 'ARRAY';
    my $num_examples = scalar @$features_2d;
    my $num_features = scalar @{$features_2d->[0]};
    # Build pointer arrays for features, labels, confidences
    my $ptr_size = $ffi->sizeof('opaque');
    # Pack each feature row into a buffer and collect pointers
    my @feat_bufs;
    my $feat_ptrs = '';
    for my $i (0 .. $num_examples - 1) {
        my $packed = pack("f*", @{$features_2d->[$i]});
        my ($buf, undef) = FFI::Platypus::Buffer::scalar_to_buffer($packed);
        push @feat_bufs, $packed;  # prevent GC
        $feat_ptrs .= pack($ptr_size == 8 ? 'Q' : 'L', $buf);
    }
    my ($feat_ptrs_buf, undef) = FFI::Platypus::Buffer::scalar_to_buffer($feat_ptrs);
    # num_features_array = NULL (uniform)
    # Build label string pointer array
    my @label_bufs;
    my $label_ptrs = '';
    for my $i (0 .. $num_examples - 1) {
        my $str = $labels->[$i] . "\0";
        my ($buf, undef) = FFI::Platypus::Buffer::scalar_to_buffer($str);
        push @label_bufs, $str;
        $label_ptrs .= pack($ptr_size == 8 ? 'Q' : 'L', $buf);
    }
    my ($label_ptrs_buf, undef) = FFI::Platypus::Buffer::scalar_to_buffer($label_ptrs);
    # Confidences
    my $conf_buf;
    if ($confidences && ref $confidences eq 'ARRAY') {
        my $conf_packed = pack("f*", @$confidences);
        ($conf_buf, undef) = FFI::Platypus::Buffer::scalar_to_buffer($conf_packed);
    } else {
        $conf_buf = undef;
    }
    # losses_out
    my $losses_packed = pack("f*", (0.0) x $num_examples);
    my ($losses_buf, undef) = FFI::Platypus::Buffer::scalar_to_buffer($losses_packed);
    my $status = NIMCP::nimcp_brain_learn_batch(
        $self->{_handle}, $feat_ptrs_buf, undef, $label_ptrs_buf,
        $conf_buf, $num_examples, $losses_buf
    );
    NIMCP::_check_status($status, 'learn_batch');
    my $losses_data = FFI::Platypus::Buffer::buffer_to_scalar($losses_buf, $num_examples * 4);
    return [unpack("f$num_examples", $losses_data)];
}

sub learn_knowledge {
    my ($self, $text, $domain) = @_;
    $domain //= 10;  # general
    my $status = NIMCP::nimcp_brain_learn_knowledge($self->{_handle}, $text, $domain);
    NIMCP::_check_status($status, 'learn_knowledge');
    return $self;
}

sub learn_language {
    my ($self, $text) = @_;
    my $loss = 0.0;
    my $status = NIMCP::nimcp_brain_learn_language($self->{_handle}, $text, \$loss);
    NIMCP::_check_status($status, 'learn_language');
    return $loss;
}

sub learn_language_pair {
    my ($self, $input_text, $target_text, $learning_rate) = @_;
    $learning_rate //= 0.0;
    my $loss = 0.0;
    my $status = NIMCP::nimcp_brain_learn_language_pair(
        $self->{_handle}, $input_text, $target_text, $learning_rate, \$loss
    );
    NIMCP::_check_status($status, 'learn_language_pair');
    return $loss;
}

sub train_cognitive {
    my ($self, $text, $domain, $target_text, $learning_rate) = @_;
    $domain //= -1;
    $learning_rate //= 0.0;
    my $loss = 0.0;
    my $status = NIMCP::nimcp_brain_train_cognitive(
        $self->{_handle}, $text, $domain, $target_text, $learning_rate, \$loss
    );
    NIMCP::_check_status($status, 'train_cognitive');
    return $loss;
}

sub train_language {
    my ($self, $input_text, $target_text, $learning_rate) = @_;
    $learning_rate //= 0.0;
    my $loss = 0.0;
    my $status = NIMCP::nimcp_brain_train_language(
        $self->{_handle}, $input_text, $target_text, $learning_rate, \$loss
    );
    NIMCP::_check_status($status, 'train_language');
    return $loss;
}

sub predict {
    my ($self, $features) = @_;
    die "features must be array ref" unless ref $features eq 'ARRAY';
    my $n = scalar @$features;
    # Allocate output buffer for label (64 bytes)
    my $label_ptr = FFI::Platypus::Memory::calloc(1, 64);
    my $conf = 0.0;
    my $status = NIMCP::nimcp_brain_predict(
        $self->{_handle}, $features, $n, $label_ptr, \$conf
    );
    my $label = FFI::Platypus::Buffer::buffer_to_scalar($label_ptr, 64);
    FFI::Platypus::Memory::free($label_ptr);
    NIMCP::_check_status($status, 'predict');
    $label =~ s/\0.*//s;  # trim after null terminator
    return wantarray ? ($label, $conf) : { label => $label, confidence => $conf };
}

sub predict_fast {
    my ($self, $features) = @_;
    die "features must be array ref" unless ref $features eq 'ARRAY';
    my $n = scalar @$features;
    my $label_ptr = FFI::Platypus::Memory::calloc(1, 64);
    my $conf = 0.0;
    my $status = NIMCP::nimcp_brain_predict_fast(
        $self->{_handle}, $features, $n, $label_ptr, \$conf
    );
    my $label = FFI::Platypus::Buffer::buffer_to_scalar($label_ptr, 64);
    FFI::Platypus::Memory::free($label_ptr);
    NIMCP::_check_status($status, 'predict_fast');
    $label =~ s/\0.*//s;
    return wantarray ? ($label, $conf) : { label => $label, confidence => $conf };
}

sub predict_in_domain {
    my ($self, $features, $domain_prefix) = @_;
    die "features must be array ref" unless ref $features eq 'ARRAY';
    my $n = scalar @$features;
    my $label_ptr = FFI::Platypus::Memory::calloc(1, 64);
    my $conf = 0.0;
    my $status = NIMCP::nimcp_brain_predict_in_domain(
        $self->{_handle}, $features, $n, $domain_prefix, $label_ptr, \$conf
    );
    my $label = FFI::Platypus::Buffer::buffer_to_scalar($label_ptr, 64);
    FFI::Platypus::Memory::free($label_ptr);
    NIMCP::_check_status($status, 'predict_in_domain');
    $label =~ s/\0.*//s;
    return wantarray ? ($label, $conf) : { label => $label, confidence => $conf };
}

sub decide_full {
    my ($self, $features) = @_;
    die "features must be array ref" unless ref $features eq 'ARRAY';
    my $n = scalar @$features;
    my $label_ptr = FFI::Platypus::Memory::calloc(1, 64);
    my $expl_ptr  = FFI::Platypus::Memory::calloc(1, 512);
    my $conf = 0.0;
    my $num_outputs = $self->{_num_outputs} // 128;
    my @out_vec = (0.0) x $num_outputs;
    my $out_size = 0;
    my $active_neurons = 0;
    my $sparsity = 0.0;
    my $inference_us = 0;
    my $status = NIMCP::nimcp_brain_decide_full(
        $self->{_handle}, $features, $n,
        $label_ptr, \$conf, $expl_ptr,
        \@out_vec, \$out_size, \$active_neurons, \$sparsity, \$inference_us
    );
    my $label = FFI::Platypus::Buffer::buffer_to_scalar($label_ptr, 64);
    my $expl  = FFI::Platypus::Buffer::buffer_to_scalar($expl_ptr, 512);
    FFI::Platypus::Memory::free($label_ptr);
    FFI::Platypus::Memory::free($expl_ptr);
    NIMCP::_check_status($status, 'decide_full');
    $label =~ s/\0.*//s;
    $expl  =~ s/\0.*//s;
    return {
        label             => $label,
        confidence        => $conf,
        explanation       => $expl,
        output_vector     => [@out_vec[0 .. ($out_size > 0 ? $out_size - 1 : 0)]],
        active_neurons    => $active_neurons,
        sparsity          => $sparsity,
        inference_time_us => $inference_us,
    };
}

sub speak {
    my ($self, $semantic_input, $text_max_len) = @_;
    $text_max_len //= 1024;
    my $text_ptr = FFI::Platypus::Memory::calloc(1, $text_max_len);
    my $conf = 0.0;
    my $fluency = 0.0;
    my ($sem_arr, $sem_dim);
    if ($semantic_input && ref $semantic_input eq 'ARRAY') {
        $sem_arr = $semantic_input;
        $sem_dim = scalar @$semantic_input;
    } else {
        $sem_arr = [];
        $sem_dim = 0;
    }
    my $status = NIMCP::nimcp_brain_speak(
        $self->{_handle}, $sem_arr, $sem_dim, $text_ptr, $text_max_len, \$conf, \$fluency
    );
    my $text = FFI::Platypus::Buffer::buffer_to_scalar($text_ptr, $text_max_len);
    FFI::Platypus::Memory::free($text_ptr);
    NIMCP::_check_status($status, 'speak');
    $text =~ s/\0.*//s;
    return {
        text       => $text,
        confidence => $conf,
        fluency    => $fluency,
    };
}

sub generate_text {
    my ($self, %args) = @_;
    my $prompt        = $args{prompt};
    my $semantic_input = $args{semantic_input};
    my $text_max_len  = $args{text_max_len} // 1024;
    my $text_ptr = FFI::Platypus::Memory::calloc(1, $text_max_len);
    my $conf = 0.0;
    my $perplexity = 0.0;
    my ($sem_arr, $sem_dim);
    if ($semantic_input && ref $semantic_input eq 'ARRAY') {
        $sem_arr = $semantic_input;
        $sem_dim = scalar @$semantic_input;
    } else {
        $sem_arr = [];
        $sem_dim = 0;
    }
    my $status = NIMCP::nimcp_brain_generate_text(
        $self->{_handle}, $prompt, $sem_arr, $sem_dim,
        $text_ptr, $text_max_len, \$conf, \$perplexity
    );
    my $text = FFI::Platypus::Buffer::buffer_to_scalar($text_ptr, $text_max_len);
    FFI::Platypus::Memory::free($text_ptr);
    NIMCP::_check_status($status, 'generate_text');
    $text =~ s/\0.*//s;
    return {
        text       => $text,
        confidence => $conf,
        perplexity => $perplexity,
    };
}

sub comprehend {
    my ($self, $text, $semantic_dim) = @_;
    $semantic_dim //= 128;
    my @semantic = (0.0) x $semantic_dim;
    my $conf = 0.0;
    my $status = NIMCP::nimcp_brain_comprehend(
        $self->{_handle}, $text, \@semantic, $semantic_dim, \$conf
    );
    NIMCP::_check_status($status, 'comprehend');
    return {
        semantic   => \@semantic,
        confidence => $conf,
    };
}

sub produce_text {
    my ($self, $intent, $text_max_len) = @_;
    die "intent must be array ref" unless ref $intent eq 'ARRAY';
    $text_max_len //= 1024;
    my $intent_dim = scalar @$intent;
    my $text_ptr = FFI::Platypus::Memory::calloc(1, $text_max_len);
    my $conf = 0.0;
    my $status = NIMCP::nimcp_brain_produce_text(
        $self->{_handle}, $intent, $intent_dim, $text_ptr, $text_max_len, \$conf
    );
    my $text = FFI::Platypus::Buffer::buffer_to_scalar($text_ptr, $text_max_len);
    FFI::Platypus::Memory::free($text_ptr);
    NIMCP::_check_status($status, 'produce_text');
    $text =~ s/\0.*//s;
    return {
        text       => $text,
        confidence => $conf,
    };
}

sub grounded_respond {
    my ($self, $input_text, $response_max) = @_;
    $response_max //= 1024;
    my $resp_ptr = FFI::Platypus::Memory::calloc(1, $response_max);
    my $conf = 0.0;
    my $status = NIMCP::nimcp_brain_grounded_respond(
        $self->{_handle}, $input_text, $resp_ptr, $response_max, \$conf
    );
    my $response = FFI::Platypus::Buffer::buffer_to_scalar($resp_ptr, $response_max);
    FFI::Platypus::Memory::free($resp_ptr);
    NIMCP::_check_status($status, 'grounded_respond');
    $response =~ s/\0.*//s;
    return {
        response   => $response,
        confidence => $conf,
    };
}

sub creative_blend {
    my ($self, $vector_a, $vector_b, $blend_ratio, $text_max) = @_;
    die "vector_a must be array ref" unless ref $vector_a eq 'ARRAY';
    die "vector_b must be array ref" unless ref $vector_b eq 'ARRAY';
    $blend_ratio //= 0.5;
    $text_max //= 1024;
    my $vec_dim = scalar @$vector_a;
    my $text_ptr = FFI::Platypus::Memory::calloc(1, $text_max);
    my $status = NIMCP::nimcp_brain_creative_blend(
        $self->{_handle}, $vector_a, $vector_b, $vec_dim, $blend_ratio, $text_ptr, $text_max
    );
    my $text = FFI::Platypus::Buffer::buffer_to_scalar($text_ptr, $text_max);
    FFI::Platypus::Memory::free($text_ptr);
    NIMCP::_check_status($status, 'creative_blend');
    $text =~ s/\0.*//s;
    return $text;
}

sub ground_word {
    my ($self, $word, $features, $modality, $attention) = @_;
    die "features must be array ref" unless ref $features eq 'ARRAY';
    $modality  //= 0;
    $attention //= 1.0;
    my $dim = scalar @$features;
    my $status = NIMCP::nimcp_brain_ground_word(
        $self->{_handle}, $word, $features, $dim, $modality, $attention
    );
    NIMCP::_check_status($status, 'ground_word');
    return $self;
}

sub infer {
    my ($self, $features, $num_outputs) = @_;
    die "features must be array ref" unless ref $features eq 'ARRAY';
    $num_outputs //= $self->{_num_outputs} // 10;
    my $n = scalar @$features;
    my @out = (0.0) x $num_outputs;
    my $status = NIMCP::nimcp_brain_infer(
        $self->{_handle}, $features, $n, \@out, $num_outputs
    );
    NIMCP::_check_status($status, 'infer');
    return \@out;
}

sub save {
    my ($self, $filepath) = @_;
    my $status = NIMCP::nimcp_brain_save($self->{_handle}, $filepath);
    NIMCP::_check_status($status, 'save');
    return $self;
}

# --- Metrics / Stats ---

sub get_accuracy {
    my ($self) = @_;
    return NIMCP::nimcp_brain_get_accuracy($self->{_handle});
}

sub get_last_gradient_norm {
    my ($self) = @_;
    return NIMCP::nimcp_brain_get_last_gradient_norm($self->{_handle});
}

sub get_last_loss {
    my ($self) = @_;
    return NIMCP::nimcp_brain_get_last_loss($self->{_handle});
}

sub get_network_metrics {
    my ($self) = @_;
    my $ema_ann = 0.0; my $ema_cnn = 0.0; my $ema_snn = 0.0; my $ema_lnn = 0.0;
    my $ann_steps = 0; my $cnn_steps = 0; my $snn_steps = 0; my $lnn_steps = 0;
    my $ok = NIMCP::nimcp_brain_get_network_metrics(
        $self->{_handle},
        \$ema_ann, \$ema_cnn, \$ema_snn, \$ema_lnn,
        \$ann_steps, \$cnn_steps, \$snn_steps, \$lnn_steps
    );
    return undef unless $ok;
    return {
        ema_ann => $ema_ann, ema_cnn => $ema_cnn,
        ema_snn => $ema_snn, ema_lnn => $ema_lnn,
        ann_steps => $ann_steps, cnn_steps => $cnn_steps,
        snn_steps => $snn_steps, lnn_steps => $lnn_steps,
    };
}

sub get_cognitive_stats {
    my ($self) = @_;
    my $max_modules = 13;
    my $stats_buf  = FFI::Platypus::Memory::calloc($max_modules, 4);
    my $losses_buf = FFI::Platypus::Memory::calloc($max_modules, 4);
    my $count = 0;
    my $status = NIMCP::nimcp_brain_get_cognitive_stats(
        $self->{_handle}, $stats_buf, $losses_buf, \$count
    );
    my $stats_data  = FFI::Platypus::Buffer::buffer_to_scalar($stats_buf,  $max_modules * 4);
    my $losses_data = FFI::Platypus::Buffer::buffer_to_scalar($losses_buf, $max_modules * 4);
    FFI::Platypus::Memory::free($stats_buf);
    FFI::Platypus::Memory::free($losses_buf);
    NIMCP::_check_status($status, 'get_cognitive_stats');
    my @steps  = unpack("I$count", $stats_data);
    my @losses = unpack("f$count", $losses_data);
    return {
        steps  => \@steps,
        losses => \@losses,
        count  => $count,
    };
}

sub get_avatar_state {
    my ($self) = @_;
    # nimcp_avatar_state_t is a large struct; allocate generously
    my $buf_size = 2048;
    my $buf_ptr = FFI::Platypus::Memory::calloc(1, $buf_size);
    my $status = NIMCP::nimcp_brain_get_avatar_state($self->{_handle}, $buf_ptr);
    my $buf = FFI::Platypus::Buffer::buffer_to_scalar($buf_ptr, $buf_size);
    FFI::Platypus::Memory::free($buf_ptr);
    NIMCP::_check_status($status, 'get_avatar_state');
    return $buf;  # raw bytes; caller must unpack per struct layout
}

sub get_last_transcript {
    my ($self, $max_entries) = @_;
    $max_entries //= 32;
    # Each entry: char[256], plus float salience, float confidence, const char* module
    my $entry_buf = FFI::Platypus::Memory::calloc($max_entries, 256);
    my $sal_buf   = FFI::Platypus::Memory::calloc($max_entries, 4);
    my $conf_buf  = FFI::Platypus::Memory::calloc($max_entries, 4);
    my $mod_buf   = FFI::Platypus::Memory::calloc($max_entries, $ffi->sizeof('opaque'));
    my $count = NIMCP::nimcp_brain_get_last_transcript(
        $self->{_handle}, $entry_buf, $sal_buf, $conf_buf, $mod_buf, $max_entries
    );
    my @entries;
    if ($count > 0) {
        my $e_data = FFI::Platypus::Buffer::buffer_to_scalar($entry_buf, $max_entries * 256);
        my $s_data = FFI::Platypus::Buffer::buffer_to_scalar($sal_buf,   $count * 4);
        my $c_data = FFI::Platypus::Buffer::buffer_to_scalar($conf_buf,  $count * 4);
        my @sals  = unpack("f$count", $s_data);
        my @confs = unpack("f$count", $c_data);
        for my $i (0 .. $count - 1) {
            my $text = unpack('Z256', substr($e_data, $i * 256, 256));
            push @entries, {
                text       => $text,
                salience   => $sals[$i],
                confidence => $confs[$i],
            };
        }
    }
    FFI::Platypus::Memory::free($entry_buf);
    FFI::Platypus::Memory::free($sal_buf);
    FFI::Platypus::Memory::free($conf_buf);
    FFI::Platypus::Memory::free($mod_buf);
    return \@entries;
}

sub get_cortex_cnn_metrics {
    my ($self) = @_;
    my $max_cortices = 4;
    my $types_buf   = FFI::Platypus::Memory::calloc($max_cortices, 4);
    my $losses_buf  = FFI::Platypus::Memory::calloc($max_cortices, 4);
    my $fwd_buf     = FFI::Platypus::Memory::calloc($max_cortices, 8);
    my $bwd_buf     = FFI::Platypus::Memory::calloc($max_cortices, 8);
    my $norms_buf   = FFI::Platypus::Memory::calloc($max_cortices, 4);
    my $count = 0;
    my $status = NIMCP::nimcp_brain_get_cortex_cnn_metrics(
        $self->{_handle}, $types_buf, $losses_buf, $fwd_buf, $bwd_buf, $norms_buf, \$count
    );
    my @metrics;
    if ($status == 0 && $count > 0) {
        my $t_data = FFI::Platypus::Buffer::buffer_to_scalar($types_buf,  $count * 4);
        my $l_data = FFI::Platypus::Buffer::buffer_to_scalar($losses_buf, $count * 4);
        my $f_data = FFI::Platypus::Buffer::buffer_to_scalar($fwd_buf,    $count * 8);
        my $b_data = FFI::Platypus::Buffer::buffer_to_scalar($bwd_buf,    $count * 8);
        my $n_data = FFI::Platypus::Buffer::buffer_to_scalar($norms_buf,  $count * 4);
        my @types  = unpack("i$count", $t_data);
        my @losses = unpack("f$count", $l_data);
        my @fwds   = unpack("Q$count", $f_data);
        my @bwds   = unpack("Q$count", $b_data);
        my @norms  = unpack("f$count", $n_data);
        for my $i (0 .. $count - 1) {
            push @metrics, {
                type       => $types[$i],
                loss       => $losses[$i],
                fwd_steps  => $fwds[$i],
                bwd_steps  => $bwds[$i],
                embed_norm => $norms[$i],
            };
        }
    }
    FFI::Platypus::Memory::free($types_buf);
    FFI::Platypus::Memory::free($losses_buf);
    FFI::Platypus::Memory::free($fwd_buf);
    FFI::Platypus::Memory::free($bwd_buf);
    FFI::Platypus::Memory::free($norms_buf);
    NIMCP::_check_status($status, 'get_cortex_cnn_metrics');
    return \@metrics;
}

# --- Experience ---

sub experience {
    my ($self, $input, $output_size, $teacher_reward) = @_;
    die "input must be array ref" unless ref $input eq 'ARRAY';
    $output_size //= $self->{_num_outputs} // 10;
    $teacher_reward //= 0.0;
    my $input_size = scalar @$input;
    my @output = (0.0) x $output_size;
    # brain_experience_result_t: allocate generously
    my $result_buf = FFI::Platypus::Memory::calloc(1, 256);
    my $status = NIMCP::nimcp_brain_experience(
        $self->{_handle}, $input, $input_size, \@output, $output_size,
        $teacher_reward, $result_buf
    );
    FFI::Platypus::Memory::free($result_buf);
    NIMCP::_check_status($status, 'experience');
    return \@output;
}

sub experience_configure {
    my ($self, $config_ptr) = @_;
    my $status = NIMCP::nimcp_brain_experience_configure($self->{_handle}, $config_ptr);
    NIMCP::_check_status($status, 'experience_configure');
    return $self;
}

sub experience_correct {
    my ($self, $expected) = @_;
    die "expected must be array ref" unless ref $expected eq 'ARRAY';
    my $n = scalar @$expected;
    return NIMCP::nimcp_brain_experience_correct($self->{_handle}, $expected, $n);
}

sub experience_attend {
    my ($self, $modality, $strength) = @_;
    $strength //= 1.0;
    my $status = NIMCP::nimcp_brain_experience_attend($self->{_handle}, $modality, $strength);
    NIMCP::_check_status($status, 'experience_attend');
    return $self;
}

# --- Freeze ---

sub freeze {
    my ($self) = @_;
    my $status = NIMCP::nimcp_brain_freeze($self->{_handle});
    NIMCP::_check_status($status, 'freeze');
    return $self;
}

sub is_frozen {
    my ($self) = @_;
    return NIMCP::nimcp_brain_is_frozen($self->{_handle}) ? 1 : 0;
}

# --- Enable flags ---

sub enable_mixed_precision {
    my ($self, $enable) = @_;
    $enable //= 1;
    my $status = NIMCP::nimcp_brain_enable_mixed_precision($self->{_handle}, $enable ? 1 : 0);
    NIMCP::_check_status($status, 'enable_mixed_precision');
    return $self;
}

sub enable_gradient_checkpointing {
    my ($self, $enable, $checkpoint_interval) = @_;
    $enable //= 1;
    $checkpoint_interval //= 0;
    my $status = NIMCP::nimcp_brain_enable_gradient_checkpointing(
        $self->{_handle}, $enable ? 1 : 0, $checkpoint_interval
    );
    NIMCP::_check_status($status, 'enable_gradient_checkpointing');
    return $self;
}

sub enable_hemispheric {
    my ($self, $enable) = @_;
    $enable //= 1;
    my $status = NIMCP::nimcp_brain_enable_hemispheric($self->{_handle}, $enable ? 1 : 0);
    NIMCP::_check_status($status, 'enable_hemispheric');
    return $self;
}

sub enable_recurrent {
    my ($self, $enable, $max_iterations, $confidence_threshold, $blend_alpha) = @_;
    $enable //= 1;
    $max_iterations //= 3;
    $confidence_threshold //= 0.9;
    $blend_alpha //= 0.5;
    my $status = NIMCP::nimcp_brain_enable_recurrent(
        $self->{_handle}, $enable ? 1 : 0, $max_iterations, $confidence_threshold, $blend_alpha
    );
    NIMCP::_check_status($status, 'enable_recurrent');
    return $self;
}

sub enable_bptt {
    my ($self, $enable, $window_size, $discount) = @_;
    $enable //= 1;
    $window_size //= 8;
    $discount //= 0.95;
    my $status = NIMCP::nimcp_brain_enable_bptt(
        $self->{_handle}, $enable ? 1 : 0, $window_size, $discount
    );
    NIMCP::_check_status($status, 'enable_bptt');
    return $self;
}

sub enable_multi_network {
    my ($self) = @_;
    my $status = NIMCP::nimcp_brain_enable_multi_network($self->{_handle});
    NIMCP::_check_status($status, 'enable_multi_network');
    return $self;
}

sub enable_biological_plasticity {
    my ($self, $enable) = @_;
    $enable //= 1;
    my $status = NIMCP::nimcp_brain_enable_biological_plasticity($self->{_handle}, $enable ? 1 : 0);
    NIMCP::_check_status($status, 'enable_biological_plasticity');
    return $self;
}

# --- Set / Config ---

sub set_fast_training {
    my ($self, $enabled) = @_;
    $enabled //= 1;
    my $status = NIMCP::nimcp_brain_set_fast_training($self->{_handle}, $enabled ? 1 : 0);
    NIMCP::_check_status($status, 'set_fast_training');
    return $self;
}

sub set_task_type {
    my ($self, $task_type) = @_;
    my $status = NIMCP::nimcp_brain_set_task_type($self->{_handle}, $task_type);
    NIMCP::_check_status($status, 'set_task_type');
    return $self;
}

sub set_training_mode {
    my ($self, $active) = @_;
    $active //= 1;
    NIMCP::nimcp_brain_set_training_mode($self->{_handle}, $active ? 1 : 0);
    return $self;
}

sub set_network_ablation {
    my ($self, $train_cnn, $train_snn, $train_lnn) = @_;
    $train_cnn //= -1;
    $train_snn //= -1;
    $train_lnn //= -1;
    NIMCP::nimcp_brain_set_network_ablation(
        $self->{_handle}, $train_cnn, $train_snn, $train_lnn
    );
    return $self;
}

# --- Per-network training toggles (runtime-dynamic, no rebuild required) ---

sub set_train_ann {
    my ($self, $enabled) = @_;
    $enabled //= 1;
    NIMCP::nimcp_brain_set_train_ann($self->{_handle}, $enabled ? 1 : 0);
    return $self;
}
sub get_train_ann {
    my ($self) = @_;
    return NIMCP::nimcp_brain_get_train_ann($self->{_handle}) ? 1 : 0;
}

sub set_train_cnn {
    my ($self, $enabled) = @_;
    $enabled //= 1;
    NIMCP::nimcp_brain_set_train_cnn($self->{_handle}, $enabled ? 1 : 0);
    return $self;
}
sub get_train_cnn {
    my ($self) = @_;
    return NIMCP::nimcp_brain_get_train_cnn($self->{_handle}) ? 1 : 0;
}

sub set_train_snn {
    my ($self, $enabled) = @_;
    $enabled //= 1;
    NIMCP::nimcp_brain_set_train_snn($self->{_handle}, $enabled ? 1 : 0);
    return $self;
}
sub get_train_snn {
    my ($self) = @_;
    return NIMCP::nimcp_brain_get_train_snn($self->{_handle}) ? 1 : 0;
}

sub set_train_lnn {
    my ($self, $enabled) = @_;
    $enabled //= 1;
    NIMCP::nimcp_brain_set_train_lnn($self->{_handle}, $enabled ? 1 : 0);
    return $self;
}
sub get_train_lnn {
    my ($self) = @_;
    return NIMCP::nimcp_brain_get_train_lnn($self->{_handle}) ? 1 : 0;
}

# Convenience preset: freeze ANN/CNN/LNN while keeping SNN training.
sub set_snn_only_recovery {
    my ($self, $enabled) = @_;
    $enabled //= 1;
    NIMCP::nimcp_brain_set_snn_only_recovery($self->{_handle}, $enabled ? 1 : 0);
    return $self;
}
sub get_snn_only_recovery {
    my ($self) = @_;
    return NIMCP::nimcp_brain_get_snn_only_recovery($self->{_handle}) ? 1 : 0;
}

# Ensemble warmup scale [0.0, 1.0] — probabilistic gate on non-SNN training.
sub set_ensemble_warmup_scale {
    my ($self, $scale) = @_;
    $scale //= 1.0;
    NIMCP::nimcp_brain_set_ensemble_warmup_scale($self->{_handle}, $scale + 0.0);
    return $self;
}
sub get_ensemble_warmup_scale {
    my ($self) = @_;
    return NIMCP::nimcp_brain_get_ensemble_warmup_scale($self->{_handle});
}

# --- Sensory / Cortex ---

sub submit_sensory {
    my ($self, $modality, $data, $width, $height, $channels, $n_segments) = @_;
    die "data must be array ref" unless ref $data eq 'ARRAY';
    $width    //= 0;
    $height   //= 0;
    $channels //= 0;
    $n_segments //= 0;
    my $num_elements = scalar @$data;
    my $status = NIMCP::nimcp_brain_submit_sensory(
        $self->{_handle}, $modality, $data, $num_elements,
        $width, $height, $channels, $n_segments
    );
    NIMCP::_check_status($status, 'submit_sensory');
    return $self;
}

sub visual_cortex_process {
    my ($self, $pixels, $width, $height, $channels, $max_features) = @_;
    die "pixels must be array ref" unless ref $pixels eq 'ARRAY';
    $max_features //= 256;
    my $num_pixels = scalar @$pixels;
    my @features = (0.0) x $max_features;
    my $feature_count = 0;
    my $status = NIMCP::nimcp_brain_visual_cortex_process(
        $self->{_handle}, $pixels, $num_pixels, $width, $height, $channels,
        \@features, $max_features, \$feature_count
    );
    NIMCP::_check_status($status, 'visual_cortex_process');
    return [@features[0 .. ($feature_count > 0 ? $feature_count - 1 : 0)]];
}

# --- Neuromodulation / Sleep / Substrate ---

sub medulla_get_arousal {
    my ($self) = @_;
    return NIMCP::nimcp_brain_medulla_get_arousal($self->{_handle});
}

sub bg_get_dopamine {
    my ($self) = @_;
    return NIMCP::nimcp_brain_bg_get_dopamine($self->{_handle});
}

sub sleep_get_pressure {
    my ($self) = @_;
    return NIMCP::nimcp_brain_sleep_get_pressure($self->{_handle});
}

sub substrate_get_health {
    my ($self) = @_;
    my $max_len = 64;
    my $buf_ptr = FFI::Platypus::Memory::calloc(1, $max_len);
    my $status = NIMCP::nimcp_brain_substrate_get_health($self->{_handle}, $buf_ptr, $max_len);
    my $health = FFI::Platypus::Buffer::buffer_to_scalar($buf_ptr, $max_len);
    FFI::Platypus::Memory::free($buf_ptr);
    NIMCP::_check_status($status, 'substrate_get_health');
    $health =~ s/\0.*//s;
    return $health;
}

# --- Sub-network creation / stats ---

sub lnn_create {
    my ($self, $n_sensory, $n_inter, $n_command, $n_output) = @_;
    my $status = NIMCP::nimcp_brain_lnn_create(
        $self->{_handle}, $n_sensory, $n_inter, $n_command, $n_output
    );
    NIMCP::_check_status($status, 'lnn_create');
    return $self;
}

sub lnn_get_stats {
    my ($self) = @_;
    my $fwd = 0; my $bwd = 0; my $ode = 0;
    my $avg_tau = 0.0; my $state_norm = 0.0; my $grad_norm = 0.0;
    my $nan_count = 0; my $inf_count = 0;
    my $status = NIMCP::nimcp_brain_lnn_get_stats(
        $self->{_handle}, \$fwd, \$bwd, \$ode, \$avg_tau,
        \$state_norm, \$grad_norm, \$nan_count, \$inf_count
    );
    NIMCP::_check_status($status, 'lnn_get_stats');
    return {
        forward_steps  => $fwd,
        backward_steps => $bwd,
        ode_evals      => $ode,
        avg_tau        => $avg_tau,
        state_norm     => $state_norm,
        gradient_norm  => $grad_norm,
        nan_count      => $nan_count,
        inf_count      => $inf_count,
    };
}

sub snn_get_stats {
    my ($self) = @_;
    my $total_steps = 0; my $total_spikes = 0;
    my $mean_rate = 0.0; my $sparsity = 0.0; my $synchrony = 0.0;
    my $silent = 0; my $hyperactive = 0;
    # health (int) and memory_bytes (size_t) via opaque buffers
    my $health_buf = FFI::Platypus::Memory::calloc(1, 4);
    my $mem_buf    = FFI::Platypus::Memory::calloc(1, 8);
    my $status = NIMCP::nimcp_brain_snn_get_stats(
        $self->{_handle}, \$total_steps, \$total_spikes,
        \$mean_rate, \$sparsity, \$synchrony,
        \$silent, \$hyperactive, $health_buf, $mem_buf
    );
    my $health_data = FFI::Platypus::Buffer::buffer_to_scalar($health_buf, 4);
    my $mem_data    = FFI::Platypus::Buffer::buffer_to_scalar($mem_buf, 8);
    FFI::Platypus::Memory::free($health_buf);
    FFI::Platypus::Memory::free($mem_buf);
    NIMCP::_check_status($status, 'snn_get_stats');
    my ($health) = unpack('i', $health_data);
    my ($memory) = unpack('Q', $mem_data);
    return {
        total_steps         => $total_steps,
        total_spikes        => $total_spikes,
        mean_firing_rate    => $mean_rate,
        sparsity            => $sparsity,
        synchrony           => $synchrony,
        silent_neurons      => $silent,
        hyperactive_neurons => $hyperactive,
        health              => $health,
        memory_bytes        => $memory,
    };
}

sub cnn_get_stats {
    my ($self) = @_;
    my $num_layers = 0;
    my $num_labels = 0;
    my $params_buf = FFI::Platypus::Memory::calloc(1, 8);
    my $active_buf = FFI::Platypus::Memory::calloc(1, 4);
    my $status = NIMCP::nimcp_brain_cnn_get_stats(
        $self->{_handle}, \$num_layers, $params_buf, \$num_labels, $active_buf
    );
    my $params_data = FFI::Platypus::Buffer::buffer_to_scalar($params_buf, 8);
    my $active_data = FFI::Platypus::Buffer::buffer_to_scalar($active_buf, 1);
    FFI::Platypus::Memory::free($params_buf);
    FFI::Platypus::Memory::free($active_buf);
    NIMCP::_check_status($status, 'cnn_get_stats');
    my ($num_params) = unpack('Q', $params_data);
    my ($active) = unpack('C', $active_data);
    return {
        num_layers     => $num_layers,
        num_parameters => $num_params,
        num_labels     => $num_labels,
        active         => $active ? 1 : 0,
    };
}

# --- Rubric ---

sub rubric {
    my ($self) = @_;
    # nimcp_rubric_t is a complex struct; allocate generously
    my $buf_size = 4096;
    my $buf_ptr = FFI::Platypus::Memory::calloc(1, $buf_size);
    my $status = NIMCP::nimcp_brain_rubric($self->{_handle}, $buf_ptr);
    my $buf = FFI::Platypus::Buffer::buffer_to_scalar($buf_ptr, $buf_size);
    FFI::Platypus::Memory::free($buf_ptr);
    NIMCP::_check_status($status, 'rubric');
    return $buf;  # raw bytes; caller must unpack per struct layout
}

# --- Cloud ---

sub connect_cloud {
    my ($self, $cloud_brain, $confidence_threshold, $enable_distillation) = @_;
    die "cloud_brain must be NIMCP::Brain" unless ref $cloud_brain eq 'NIMCP::Brain';
    $confidence_threshold //= 0.5;
    $enable_distillation  //= 1;
    my $status = NIMCP::nimcp_brain_connect_cloud(
        $self->{_handle}, $cloud_brain->{_handle},
        $confidence_threshold, $enable_distillation ? 1 : 0
    );
    NIMCP::_check_status($status, 'connect_cloud');
    return $self;
}

sub disconnect_cloud {
    my ($self) = @_;
    my $status = NIMCP::nimcp_brain_disconnect_cloud($self->{_handle});
    NIMCP::_check_status($status, 'disconnect_cloud');
    return $self;
}

# --- Training Pipeline ---

sub configure_training {
    my ($self, %config) = @_;
    # Build nimcp_training_config_t struct in memory
    # Layout verified from C struct definition in nimcp.h
    my $packed = pack('i i i f f f f f f I f I C x x x f C x x x f i i f f f i I C x x x',
        $config{loss_type}       // NIMCP::LOSS_CROSS_ENTROPY,
        $config{optimizer_type}  // NIMCP::OPT_ADAM,
        $config{scheduler_type}  // NIMCP::SCHED_COSINE,
        $config{learning_rate}   // 0.001,
        $config{weight_decay}    // 0.0,
        $config{momentum}        // 0.9,
        $config{beta1}           // 0.9,
        $config{beta2}           // 0.999,
        $config{epsilon}         // 1e-8,
        $config{scheduler_step_size} // 100,
        $config{scheduler_gamma}     // 0.1,
        $config{warmup_steps}        // 0,
        $config{enable_gradient_clipping} ? 1 : 0,
        $config{gradient_clip_value} // 1.0,
        $config{enable_biological_modulation} ? 1 : 0,
        $config{biological_blend}    // 0.5,
        $config{network_type}        // NIMCP::NETWORK_ADAPTIVE,
        $config{snn_method}          // NIMCP::SNN_TRAIN_STDP,
        $config{snn_eligibility_tau} // 20.0,
        $config{snn_reward_tau}      // 100.0,
        $config{snn_surrogate_beta}  // 5.0,
        $config{lnn_method}          // NIMCP::LNN_TRAIN_ADJOINT,
        $config{lnn_bptt_truncation} // 100,
        $config{lnn_use_adjoint_checkpointing} // 1 ? 1 : 0,
    );
    my ($config_ptr, $config_size) = FFI::Platypus::Buffer::scalar_to_buffer($packed);
    my $status = NIMCP::nimcp_brain_configure_training($self->{_handle}, $config_ptr);
    NIMCP::_check_status($status, 'configure_training');
    return $self;
}

sub train_step {
    my ($self, $features, $targets) = @_;
    die "features must be array ref" unless ref $features eq 'ARRAY';
    die "targets must be array ref" unless ref $targets eq 'ARRAY';
    my $nf = scalar @$features;
    my $nt = scalar @$targets;
    # nimcp_training_result_t: float loss, float lr, uint32 step, bool early_stopped(+pad), float grad_norm
    my $result_packed = pack('f f I C x x x f', 0.0, 0.0, 0, 0, 0.0);
    my ($result_ptr, $result_size) = FFI::Platypus::Buffer::scalar_to_buffer($result_packed);
    my $status = NIMCP::nimcp_brain_train_step(
        $self->{_handle}, $features, $nf, $targets, $nt, $result_ptr
    );
    NIMCP::_check_status($status, 'train_step');
    my $result_data = FFI::Platypus::Buffer::buffer_to_scalar($result_ptr, $result_size);
    my ($loss, $lr, $step, $early, $grad) = unpack('f f I C x x x f', $result_data);
    return {
        loss          => $loss,
        learning_rate => $lr,
        step          => $step,
        early_stopped => $early ? 1 : 0,
        gradient_norm => $grad,
    };
}

sub train_batch {
    my ($self, $features_2d, $targets_2d, $num_features, $num_targets) = @_;
    die "features_2d must be array ref" unless ref $features_2d eq 'ARRAY';
    die "targets_2d must be array ref" unless ref $targets_2d eq 'ARRAY';
    my $batch_size = scalar @$features_2d;
    $num_features //= scalar @{$features_2d->[0]};
    $num_targets  //= scalar @{$targets_2d->[0]};
    my @feat_flat = map { @$_ } @$features_2d;
    my @tgt_flat  = map { @$_ } @$targets_2d;
    my $result_packed = pack('f f I C x x x f', 0.0, 0.0, 0, 0, 0.0);
    my ($result_ptr, $result_size) = FFI::Platypus::Buffer::scalar_to_buffer($result_packed);
    my $status = NIMCP::nimcp_brain_train_batch(
        $self->{_handle}, \@feat_flat, \@tgt_flat,
        $batch_size, $num_features, $num_targets, $result_ptr
    );
    NIMCP::_check_status($status, 'train_batch');
    my $result_data = FFI::Platypus::Buffer::buffer_to_scalar($result_ptr, $result_size);
    my ($loss, $lr, $step, $early, $grad) = unpack('f f I C x x x f', $result_data);
    return {
        loss          => $loss,
        learning_rate => $lr,
        step          => $step,
        early_stopped => $early ? 1 : 0,
        gradient_norm => $grad,
    };
}

sub get_training_stats {
    my ($self) = @_;
    my $steps = 0;
    my $loss  = 0.0;
    my $lr    = 0.0;
    my $status = NIMCP::nimcp_brain_get_training_stats(
        $self->{_handle}, \$steps, \$loss, \$lr
    );
    NIMCP::_check_status($status, 'get_training_stats');
    return {
        total_steps => $steps,
        total_loss  => $loss,
        current_lr  => $lr,
    };
}

sub step_scheduler {
    my ($self, $validation_metric) = @_;
    $validation_metric //= 0.0;
    return NIMCP::nimcp_brain_step_scheduler($self->{_handle}, $validation_metric);
}

# --- Callbacks ---

sub enable_callbacks {
    my ($self, %config) = @_;
    # nimcp_callback_config_t:
    # bool(+pad) enable_auto_checkpoint, uint32 checkpoint_interval,
    # bool(+pad) enable_early_stopping, uint32 patience, float min_delta,
    # float divergence_threshold, uint32 log_interval
    my $packed = pack('C x x x I C x x x I f f I',
        $config{enable_auto_checkpoint} ? 1 : 0,
        $config{checkpoint_interval} // 1000,
        $config{enable_early_stopping} ? 1 : 0,
        $config{patience}            // 10,
        $config{min_delta}           // 0.0001,
        $config{divergence_threshold} // 10.0,
        $config{log_interval}        // 0,
    );
    my ($cfg_ptr, $cfg_size) = FFI::Platypus::Buffer::scalar_to_buffer($packed);
    my $status = NIMCP::nimcp_brain_enable_callbacks($self->{_handle}, $cfg_ptr);
    NIMCP::_check_status($status, 'enable_callbacks');
    return $self;
}

sub disable_callbacks {
    my ($self) = @_;
    my $status = NIMCP::nimcp_brain_disable_callbacks($self->{_handle});
    NIMCP::_check_status($status, 'disable_callbacks');
    return $self;
}

sub register_callback {
    my ($self, $event, $callback, $name) = @_;
    die "callback must be a code ref" unless ref $callback eq 'CODE';
    $name //= 'perl_callback';

    # Create a C closure for the callback trampoline
    my $closure = $ffi->closure(sub {
        my ($evt, $metrics_ptr, $user_data) = @_;
        my $action = $callback->($evt);
        return $action // NIMCP::CB_ACTION_CONTINUE;
    });
    $closure->sticky;  # prevent GC

    my $cb_ptr = $ffi->cast('(int,opaque,opaque)->int', 'opaque', $closure);
    my $cb_id = NIMCP::nimcp_brain_register_callback(
        $self->{_handle}, $event, $cb_ptr, undef, $name
    );
    die "Failed to register callback" unless $cb_id > 0;
    push @{$self->{_callbacks}}, $closure;
    return $cb_id;
}

sub unregister_callback {
    my ($self, $callback_id) = @_;
    my $status = NIMCP::nimcp_brain_unregister_callback($self->{_handle}, $callback_id);
    NIMCP::_check_status($status, 'unregister_callback');
    return $self;
}

sub get_callback_stats {
    my ($self) = @_;
    my $total = 0;
    my $avg   = 0.0;
    my $stops = 0;
    my $status = NIMCP::nimcp_brain_get_callback_stats(
        $self->{_handle}, \$total, \$avg, \$stops
    );
    NIMCP::_check_status($status, 'get_callback_stats');
    return {
        total_fired  => $total,
        avg_time_us  => $avg,
        early_stops  => $stops,
    };
}

# --- Resize ---

sub resize {
    my ($self, $new_count) = @_;
    return NIMCP::nimcp_brain_resize($self->{_handle}, $new_count);
}

sub auto_resize {
    my ($self) = @_;
    return NIMCP::nimcp_brain_auto_resize($self->{_handle});
}

sub get_neuron_count {
    my ($self) = @_;
    return NIMCP::nimcp_brain_get_neuron_count($self->{_handle});
}

sub get_utilization_metrics {
    my ($self) = @_;
    my $util = 0.0;
    my $sat  = 0.0;
    my $ok = NIMCP::nimcp_brain_get_utilization_metrics($self->{_handle}, \$util, \$sat);
    return undef unless $ok == 0;
    return {
        utilization => $util,
        saturation  => $sat,
    };
}

# --- Named Snapshots ---

sub snapshot_save {
    my ($self, $name, $description) = @_;
    $description //= '';
    my $status = NIMCP::nimcp_brain_snapshot_save($self->{_handle}, $name, $description);
    NIMCP::_check_status($status, 'snapshot_save');
    return $self;
}

sub snapshot_restore {
    my ($self, $name) = @_;
    my $new_handle = NIMCP::nimcp_brain_snapshot_restore($self->{_handle}, $name);
    die "Failed to restore snapshot: " . NIMCP::nimcp_get_error() unless $new_handle;
    return bless { _handle => $new_handle, _callbacks => [] }, ref($self);
}

sub snapshot_list {
    my ($self, $max_count) = @_;
    $max_count //= 32;
    # nimcp_brain_snapshot_info_t: char[128] + char[512] + uint64 + uint32 + bool + bool + pad = 656
    my $info_size = 656;
    my $buf_size = $info_size * $max_count;
    my $buf_ptr = FFI::Platypus::Memory::calloc(1, $buf_size);
    my $count = 0;
    my $status = NIMCP::nimcp_brain_snapshot_list(
        $self->{_handle}, $buf_ptr, $max_count, \$count
    );
    my @snapshots;
    if ($status == 0 && $count > 0) {
        my $buf = FFI::Platypus::Buffer::buffer_to_scalar($buf_ptr, $buf_size);
        for my $i (0 .. $count - 1) {
            my $offset = $i * $info_size;
            my $name = unpack('Z128', substr($buf, $offset, 128));
            my $desc = unpack('Z512', substr($buf, $offset + 128, 512));
            push @snapshots, { name => $name, description => $desc };
        }
    }
    FFI::Platypus::Memory::free($buf_ptr);
    NIMCP::_check_status($status, 'snapshot_list');
    return \@snapshots;
}

sub snapshot_delete {
    my ($self, $name) = @_;
    my $status = NIMCP::nimcp_brain_snapshot_delete($self->{_handle}, $name);
    NIMCP::_check_status($status, 'snapshot_delete');
    return $self;
}

# --- Probe ---

sub probe {
    my ($self) = @_;
    # nimcp_brain_probe_t is ~200 bytes. Allocate generously.
    my $buf_size = 512;
    my $buf_ptr = FFI::Platypus::Memory::calloc(1, $buf_size);
    my $status = NIMCP::nimcp_brain_probe($self->{_handle}, $buf_ptr);
    my $buf = FFI::Platypus::Buffer::buffer_to_scalar($buf_ptr, $buf_size);
    FFI::Platypus::Memory::free($buf_ptr);
    NIMCP::_check_status($status, 'probe');
    # Parse fields: task_name[64], size(int), task(int), num_neurons(uint32),
    # num_synapses(uint32), num_active_synapses(uint32), pad(4),
    # total_inferences(uint64), total_learning_steps(uint64)
    my $task_name = unpack('Z64', substr($buf, 0, 64));
    my ($size_val, $task_val, $neurons, $synapses, $active_syn) =
        unpack('i i I I I', substr($buf, 64, 20));
    my ($total_inf, $total_learn) = unpack('Q Q', substr($buf, 88, 16));
    return {
        task_name             => $task_name,
        size                  => $size_val,
        task                  => $task_val,
        num_neurons           => $neurons,
        num_synapses          => $synapses,
        num_active_synapses   => $active_syn,
        total_inferences      => $total_inf,
        total_learning_steps  => $total_learn,
    };
}

sub broadcast_probe {
    my ($self) = @_;
    my $status = NIMCP::nimcp_brain_broadcast_probe($self->{_handle});
    NIMCP::_check_status($status, 'broadcast_probe');
    return $self;
}

# --- COW (Copy-on-Write) ---

sub clone_cow {
    my ($self) = @_;
    my $handle = NIMCP::nimcp_brain_clone_cow($self->{_handle});
    die "Failed to clone brain: " . NIMCP::nimcp_get_error() unless $handle;
    return bless { _handle => $handle, _callbacks => [] }, ref($self);
}

sub snapshot_cow {
    my ($self) = @_;
    my $snap = NIMCP::nimcp_brain_snapshot_cow($self->{_handle});
    die "Failed to create COW snapshot" unless $snap;
    return NIMCP::BrainSnapshot->_new($snap);
}

sub restore_cow {
    my ($self, $snapshot) = @_;
    die "snapshot must be NIMCP::BrainSnapshot" unless ref $snapshot eq 'NIMCP::BrainSnapshot';
    my $status = NIMCP::nimcp_brain_restore_cow($self->{_handle}, $snapshot->{_handle});
    NIMCP::_check_status($status, 'restore_cow');
    return $self;
}

# --- Working Memory ---

sub working_memory_add {
    my ($self, $data, $salience) = @_;
    die "data must be array ref" unless ref $data eq 'ARRAY';
    $salience //= 0.5;
    my $n = scalar @$data;
    my $status = NIMCP::nimcp_brain_working_memory_add(
        $self->{_handle}, $data, $n, $salience
    );
    NIMCP::_check_status($status, 'working_memory_add');
    return $self;
}

sub working_memory_get {
    my ($self, $index) = @_;
    $index //= 0;
    my $size_out = 0;
    my $ptr = NIMCP::nimcp_brain_working_memory_get($self->{_handle}, $index, \$size_out);
    return undef unless $ptr;
    return [] unless $size_out > 0;
    my $bytes = $size_out * 4;
    my $buf = FFI::Platypus::Buffer::buffer_to_scalar($ptr, $bytes);
    return [unpack("f$size_out", $buf)];
}

sub working_memory_stats {
    my ($self) = @_;
    my $current  = 0;
    my $capacity = 0;
    my $status = NIMCP::nimcp_brain_working_memory_stats(
        $self->{_handle}, \$current, \$capacity
    );
    NIMCP::_check_status($status, 'working_memory_stats');
    return {
        current_size => $current,
        capacity     => $capacity,
    };
}

sub working_memory_refresh {
    my ($self, $index) = @_;
    my $status = NIMCP::nimcp_brain_working_memory_refresh($self->{_handle}, $index);
    NIMCP::_check_status($status, 'working_memory_refresh');
    return $self;
}

# --- Workspace ---

sub workspace_compete {
    my ($self, $module, $content, $strength) = @_;
    die "content must be array ref" unless ref $content eq 'ARRAY';
    $strength //= 0.5;
    my $dim = scalar @$content;
    return NIMCP::nimcp_brain_workspace_compete(
        $self->{_handle}, $module, $content, $dim, $strength
    );
}

sub workspace_read {
    my ($self, $max_dim) = @_;
    $max_dim //= 256;
    my @buf = (0.0) x $max_dim;
    my $actual_dim = 0;
    my $source = 0;
    my $status = NIMCP::nimcp_brain_workspace_read(
        $self->{_handle}, \@buf, $max_dim, \$actual_dim, \$source
    );
    return undef unless $status == 0;
    return {
        content => [@buf[0 .. ($actual_dim > 0 ? $actual_dim - 1 : 0)]],
        source_module => $source,
    };
}

sub workspace_subscribe {
    my ($self, $module) = @_;
    my $status = NIMCP::nimcp_brain_workspace_subscribe($self->{_handle}, $module);
    NIMCP::_check_status($status, 'workspace_subscribe');
    return $self;
}

sub workspace_unsubscribe {
    my ($self, $module) = @_;
    my $status = NIMCP::nimcp_brain_workspace_unsubscribe($self->{_handle}, $module);
    NIMCP::_check_status($status, 'workspace_unsubscribe');
    return $self;
}

sub workspace_has_broadcast {
    my ($self) = @_;
    # bool* output - use opaque buffer
    my $buf_ptr = FFI::Platypus::Memory::calloc(1, 4);
    my $status = NIMCP::nimcp_brain_workspace_has_broadcast($self->{_handle}, $buf_ptr);
    my $data = FFI::Platypus::Buffer::buffer_to_scalar($buf_ptr, 1);
    FFI::Platypus::Memory::free($buf_ptr);
    return 0 unless $status == 0;
    return unpack('C', $data) ? 1 : 0;
}

sub workspace_stats {
    my ($self) = @_;
    my $broadcasts  = 0;
    my $competitions = 0;
    my $avg_strength = 0.0;
    my $status = NIMCP::nimcp_brain_workspace_stats(
        $self->{_handle}, \$broadcasts, \$competitions, \$avg_strength
    );
    NIMCP::_check_status($status, 'workspace_stats');
    return {
        total_broadcasts   => $broadcasts,
        total_competitions => $competitions,
        avg_strength       => $avg_strength,
    };
}

# --- Oscillations ---

sub enable_oscillations {
    my ($self, $enable) = @_;
    $enable //= 1;
    return NIMCP::nimcp_enable_complex_oscillations($self->{_handle}, $enable ? 1 : 0);
}

sub is_oscillations_enabled {
    my ($self) = @_;
    return NIMCP::nimcp_is_complex_oscillations_enabled($self->{_handle});
}

sub get_phasor {
    my ($self, $neuron_id) = @_;
    my $phasor = NIMCP::nimcp_get_oscillation_phasor($self->{_handle}, $neuron_id);
    return {
        amplitude => $phasor->amplitude,
        phase     => $phasor->phase,
    };
}

sub get_phase_coherence {
    my ($self, $neuron_ids) = @_;
    die "neuron_ids must be array ref" unless ref $neuron_ids eq 'ARRAY';
    my $count = scalar @$neuron_ids;
    return NIMCP::nimcp_get_phase_coherence($self->{_handle}, $neuron_ids, $count);
}

sub get_pac_modulation {
    my ($self, $theta_freq, $gamma_freq) = @_;
    return NIMCP::nimcp_get_pac_modulation($self->{_handle}, $theta_freq, $gamma_freq);
}

# --- Edge Brain ---

sub edge_resize {
    my ($self, $target_neurons, %opts) = @_;
    my $mode_str = $opts{mode} // 'contract';
    my $transfer = $opts{knowledge_transfer} // 1;
    # Allocate config struct (use calloc for zero-init, write fields)
    my $config_sz = 64;  # oversized for safety
    my $config = FFI::Platypus::Memory::calloc(1, $config_sz);
    # target_neuron_count at offset 0 (uint32)
    $ffi->cast('opaque' => 'uint32*', $config)->[0] = $target_neurons;
    # mode at offset 4 (int32): 0=contract, 1=expand, 2=rebalance
    my $mode_val = ($mode_str eq 'expand') ? 1 : ($mode_str eq 'rebalance') ? 2 : 0;
    my $mode_ptr = $config + 4;
    $ffi->cast('opaque' => 'sint32*', $mode_ptr)->[0] = $mode_val;
    # enable_knowledge_transfer at offset 8 (bool/byte)
    my $kt_packed = pack('C', $transfer ? 1 : 0);
    my ($kt_src, undef) = FFI::Platypus::Buffer::scalar_to_buffer($kt_packed);
    # Write byte at offset 8
    FFI::Platypus::Memory::memcpy($config + 8, $kt_src, 1);
    my $ret = NIMCP::nimcp_edge_brain_resize($self->{_handle}, $config);
    FFI::Platypus::Memory::free($config);
    return { status => $ret, target_neurons => $target_neurons, mode => $mode_str };
}

sub edge_resize_check {
    my ($self, $target_neurons) = @_;
    my $config = FFI::Platypus::Memory::calloc(1, 64);
    $ffi->cast('opaque' => 'uint32*', $config)->[0] = $target_neurons;
    my $report = FFI::Platypus::Memory::calloc(1, 512);
    NIMCP::nimcp_edge_brain_resize_check($self->{_handle}, $config, $report);
    # Parse report: feasible(bool@0), neurons_before(u32@4), neurons_after(u32@8), ram_delta(f32@12)
    my $report_data = FFI::Platypus::Buffer::buffer_to_scalar($report, 16);
    my ($feasible_byte, $nb, $na, $ram) = unpack('CxxxLLf', $report_data);
    FFI::Platypus::Memory::free($config);
    FFI::Platypus::Memory::free($report);
    return {
        feasible => $feasible_byte ? 1 : 0,
        neurons_before => $nb,
        neurons_after  => $na,
        ram_delta_mb   => $ram,
    };
}

sub edge_distill {
    my ($self, $target_neurons, %opts) = @_;
    my $temp  = $opts{temperature} // 2.0;
    my $steps = $opts{steps} // 5000;
    my $inc_snn = $opts{include_snn} // 0;
    my $inc_lnn = $opts{include_lnn} // 0;
    my $inc_cnn = $opts{include_cnn} // 1;
    my $config = FFI::Platypus::Memory::calloc(1, 64);
    $ffi->cast('opaque' => 'uint32*', $config)->[0] = $target_neurons;
    my $packed = pack('fL', $temp, $steps);
    my ($src, undef) = FFI::Platypus::Buffer::scalar_to_buffer($packed);
    FFI::Platypus::Memory::memcpy($config + 4, $src, 8);
    my $bools = pack('CCC', $inc_snn ? 1 : 0, $inc_lnn ? 1 : 0, $inc_cnn ? 1 : 0);
    ($src, undef) = FFI::Platypus::Buffer::scalar_to_buffer($bools);
    FFI::Platypus::Memory::memcpy($config + 12, $src, 3);
    my $student_ptr = FFI::Platypus::Memory::calloc(1, 8);
    my $report = FFI::Platypus::Memory::calloc(1, 256);
    my $ret = NIMCP::nimcp_brain_distill($self->{_handle}, $student_ptr, $config, $report);
    # Parse report: accuracy_retention(f32@0), neurons_selected(u32@4), compression_ratio(f32@8)
    my $rdata = FFI::Platypus::Buffer::buffer_to_scalar($report, 24);
    my ($acc, $ns, $cr, $tl, $sl, $st) = unpack('fLfffL', $rdata);
    FFI::Platypus::Memory::free($config);
    FFI::Platypus::Memory::free($student_ptr);
    FFI::Platypus::Memory::free($report);
    return {
        status => $ret, accuracy_retention => $acc,
        neurons_selected => $ns, compression_ratio => $cr,
        teacher_loss => $tl, student_loss => $sl, steps_trained => $st,
    };
}

sub edge_optimize_for_device {
    my ($self, $ram_mb, %opts) = @_;
    my $cores  = $opts{cpu_cores} // 2;
    my $camera = $opts{has_camera} // 0;
    my $imu    = $opts{has_imu} // 0;
    my $motor  = $opts{has_motor_control} // 0;
    my $net    = $opts{has_network} // 1;
    my $role   = $opts{role} // 'general';
    my $profile = FFI::Platypus::Memory::calloc(1, 64);
    $ffi->cast('opaque' => 'uint32*', $profile)->[0] = $ram_mb;
    $ffi->cast('opaque' => 'uint32*', $profile + 4)->[0] = $cores;
    my $flags = pack('CCCC', $camera ? 1 : 0, $imu ? 1 : 0, $motor ? 1 : 0, $net ? 1 : 0);
    my ($src, undef) = FFI::Platypus::Buffer::scalar_to_buffer($flags);
    FFI::Platypus::Memory::memcpy($profile + 8, $src, 4);
    my $role_val = ($role eq 'sensor') ? 1 : ($role eq 'actuator') ? 2 : ($role eq 'coordinator') ? 3 : 0;
    $ffi->cast('opaque' => 'sint32*', $profile + 12)->[0] = $role_val;
    my $child_ptr = FFI::Platypus::Memory::calloc(1, 8);
    my $report = FFI::Platypus::Memory::calloc(1, 256);
    my $ret = NIMCP::nimcp_brain_optimize_for_device($self->{_handle}, $profile, $child_ptr, $report);
    my $rdata = FFI::Platypus::Buffer::buffer_to_scalar($report, 20);
    my ($nc, $se, $erm, $eim, $ar) = unpack('LLfff', $rdata);
    FFI::Platypus::Memory::free($profile);
    FFI::Platypus::Memory::free($child_ptr);
    FFI::Platypus::Memory::free($report);
    return {
        status => $ret, neuron_count => $nc, subsystems_enabled => $se,
        estimated_ram_mb => $erm, estimated_inference_ms => $eim,
        accuracy_retention => $ar,
    };
}

sub edge_quantize {
    my ($self, %opts) = @_;
    my $prec_str = $opts{precision} // 'int8_symmetric';
    my $cal      = $opts{calibration_samples} // 100;
    my $config = FFI::Platypus::Memory::calloc(1, 64);
    my $prec_val = ($prec_str eq 'fp16') ? 1 : ($prec_str eq 'int8_affine') ? 2 :
                   ($prec_str eq 'int4') ? 3 : ($prec_str eq 'ternary') ? 4 : 0;
    $ffi->cast('opaque' => 'sint32*', $config)->[0] = $prec_val;
    $ffi->cast('opaque' => 'uint32*', $config + 4)->[0] = $cal;
    my $ret = NIMCP::nimcp_brain_quantize($self->{_handle}, $config);
    FFI::Platypus::Memory::free($config);
    return { status => $ret, precision => $prec_str };
}

sub edge_score_importance {
    my ($self, $num_neurons) = @_;
    $num_neurons //= 1000;
    my $buf = FFI::Platypus::Memory::calloc($num_neurons, 4);
    NIMCP::nimcp_edge_score_neuron_importance($self->{_handle}, $buf, $num_neurons);
    my $data = FFI::Platypus::Buffer::buffer_to_scalar($buf, $num_neurons * 4);
    FFI::Platypus::Memory::free($buf);
    return [unpack("f$num_neurons", $data)];
}

# --- Swarm Master ---

sub swarm_master_create { my ($self, %opts) = @_; my $cfg = NIMCP::nimcp_swarm_master_config_default(); $self->{_swarm_master} = NIMCP::nimcp_swarm_master_create($self->{_handle}, $cfg); return $self->{_swarm_master} ? 1 : 0; }
sub swarm_master_destroy { my ($self) = @_; if ($self->{_swarm_master}) { NIMCP::nimcp_swarm_master_destroy($self->{_swarm_master}); $self->{_swarm_master} = undef; } }
sub swarm_master_start { my ($self) = @_; return -1 unless $self->{_swarm_master}; return NIMCP::nimcp_swarm_master_start($self->{_swarm_master}); }
sub swarm_master_stop { my ($self) = @_; return -1 unless $self->{_swarm_master}; return NIMCP::nimcp_swarm_master_stop($self->{_swarm_master}); }
sub swarm_master_kick { my ($self, $device_id) = @_; return -1 unless $self->{_swarm_master}; return NIMCP::nimcp_swarm_master_kick($self->{_swarm_master}, $device_id); }
sub swarm_master_force_sync { my ($self) = @_; return -1 unless $self->{_swarm_master}; return NIMCP::nimcp_swarm_master_force_sync($self->{_swarm_master}); }
sub swarm_master_get_peer_count { my ($self) = @_; return 0 unless $self->{_swarm_master}; return NIMCP::nimcp_swarm_master_get_peer_count($self->{_swarm_master}); }

# --- Swarm Edge ---

sub swarm_edge_create { my ($self, %opts) = @_; my $cfg = NIMCP::nimcp_swarm_edge_config_default(); $self->{_swarm_edge} = NIMCP::nimcp_swarm_edge_create($self->{_handle}, $cfg); return $self->{_swarm_edge} ? 1 : 0; }
sub swarm_edge_destroy { my ($self) = @_; if ($self->{_swarm_edge}) { NIMCP::nimcp_swarm_edge_destroy($self->{_swarm_edge}); $self->{_swarm_edge} = undef; } }
sub swarm_edge_start { my ($self) = @_; return -1 unless $self->{_swarm_edge}; return NIMCP::nimcp_swarm_edge_start($self->{_swarm_edge}); }
sub swarm_edge_stop { my ($self) = @_; return -1 unless $self->{_swarm_edge}; return NIMCP::nimcp_swarm_edge_stop($self->{_swarm_edge}); }
sub swarm_edge_is_connected { my ($self) = @_; return 0 unless $self->{_swarm_edge}; return NIMCP::nimcp_swarm_edge_is_connected($self->{_swarm_edge}) ? 1 : 0; }
sub swarm_edge_submit_gradients { my ($self, $gradients) = @_; return -1 unless $self->{_swarm_edge}; die "gradients must be array ref" unless ref $gradients eq 'ARRAY'; my $n = scalar @$gradients; my $buf = FFI::Platypus::Memory::calloc($n, 4); my $packed = pack("f$n", @$gradients); my ($src, undef) = FFI::Platypus::Buffer::scalar_to_buffer($packed); FFI::Platypus::Memory::memcpy($buf, $src, $n * 4); my $ret = NIMCP::nimcp_swarm_edge_submit_gradients($self->{_swarm_edge}, $buf, $n); FFI::Platypus::Memory::free($buf); return $ret; }

# --- Sensor Hub ---

sub sensor_hub_create { my ($self, $max_sensors) = @_; $max_sensors //= 32; $self->{_sensor_hub} = NIMCP::nimcp_sensor_hub_create($max_sensors); return $self->{_sensor_hub} ? 1 : 0; }
sub sensor_hub_destroy { my ($self) = @_; if ($self->{_sensor_hub}) { NIMCP::nimcp_sensor_hub_destroy($self->{_sensor_hub}); $self->{_sensor_hub} = undef; } }
sub sensor_get_count { my ($self) = @_; return 0 unless $self->{_sensor_hub}; return NIMCP::nimcp_sensor_get_count($self->{_sensor_hub}); }
sub sensor_compose_features { my ($self, $max_features) = @_; $max_features //= 1024; return [] unless $self->{_sensor_hub}; my $buf = FFI::Platypus::Memory::calloc($max_features, 4); my $count = NIMCP::nimcp_sensor_compose_feature_vector($self->{_sensor_hub}, $buf, $max_features); if ($count < 0) { FFI::Platypus::Memory::free($buf); return []; } my $data = FFI::Platypus::Buffer::buffer_to_scalar($buf, $count * 4); FFI::Platypus::Memory::free($buf); return [unpack("f$count", $data)]; }

# --- Safety Watchdog ---

sub watchdog_create { my ($self, %opts) = @_; my $cfg = NIMCP::nimcp_watchdog_config_default(); $self->{_watchdog} = NIMCP::nimcp_watchdog_create($cfg); return $self->{_watchdog} ? 1 : 0; }
sub watchdog_destroy { my ($self) = @_; if ($self->{_watchdog}) { NIMCP::nimcp_watchdog_destroy($self->{_watchdog}); $self->{_watchdog} = undef; } }
sub watchdog_arm { my ($self) = @_; return -1 unless $self->{_watchdog}; return NIMCP::nimcp_watchdog_arm($self->{_watchdog}); }
sub watchdog_disarm { my ($self) = @_; return -1 unless $self->{_watchdog}; return NIMCP::nimcp_watchdog_disarm($self->{_watchdog}); }
sub watchdog_heartbeat { my ($self) = @_; NIMCP::nimcp_watchdog_heartbeat($self->{_watchdog}) if $self->{_watchdog}; }
sub watchdog_estop { my ($self) = @_; NIMCP::nimcp_watchdog_estop($self->{_watchdog}) if $self->{_watchdog}; }
sub watchdog_reset { my ($self) = @_; return -1 unless $self->{_watchdog}; return NIMCP::nimcp_watchdog_reset($self->{_watchdog}); }
sub watchdog_get_state { my ($self) = @_; return 'NONE' unless $self->{_watchdog}; my $state = NIMCP::nimcp_watchdog_get_state($self->{_watchdog}); return NIMCP::nimcp_watchdog_state_name($state); }
sub watchdog_validate_output { my ($self, $output) = @_; return 0 unless $self->{_watchdog}; die "output must be array ref" unless ref $output eq 'ARRAY'; my $n = scalar @$output; my $buf = FFI::Platypus::Memory::calloc($n, 4); my $packed = pack("f$n", @$output); my ($src, undef) = FFI::Platypus::Buffer::scalar_to_buffer($packed); FFI::Platypus::Memory::memcpy($buf, $src, $n * 4); my $ret = NIMCP::nimcp_watchdog_validate_output($self->{_watchdog}, $buf, $n); FFI::Platypus::Memory::free($buf); return $ret == 0 ? 1 : 0; }
sub watchdog_get_safe_output { my ($self, $num_outputs) = @_; $num_outputs //= 32; return [] unless $self->{_watchdog}; my $buf = FFI::Platypus::Memory::calloc($num_outputs, 4); NIMCP::nimcp_watchdog_get_safe_output($self->{_watchdog}, $buf, $num_outputs); my $data = FFI::Platypus::Buffer::buffer_to_scalar($buf, $num_outputs * 4); FFI::Platypus::Memory::free($buf); return [unpack("f$num_outputs", $data)]; }

# --- ROS 2 Bridge ---

sub ros2_bridge_create { my ($self, %opts) = @_; my $cfg = NIMCP::nimcp_ros2_config_default(); $self->{_ros2_bridge} = NIMCP::nimcp_ros2_bridge_create($self->{_handle}, $cfg); return $self->{_ros2_bridge} ? 1 : 0; }
sub ros2_bridge_destroy { my ($self) = @_; if ($self->{_ros2_bridge}) { NIMCP::nimcp_ros2_bridge_destroy($self->{_ros2_bridge}); $self->{_ros2_bridge} = undef; } }
sub ros2_bridge_start { my ($self) = @_; return -1 unless $self->{_ros2_bridge}; return NIMCP::nimcp_ros2_bridge_start($self->{_ros2_bridge}); }
sub ros2_bridge_stop { my ($self) = @_; return -1 unless $self->{_ros2_bridge}; return NIMCP::nimcp_ros2_bridge_stop($self->{_ros2_bridge}); }
sub ros2_bridge_inject_sensor { my ($self, $topic, $data) = @_; return -1 unless $self->{_ros2_bridge}; die "data must be array ref" unless ref $data eq 'ARRAY'; my $n = scalar @$data; my $buf = FFI::Platypus::Memory::calloc($n, 4); my $packed = pack("f$n", @$data); my ($src, undef) = FFI::Platypus::Buffer::scalar_to_buffer($packed); FFI::Platypus::Memory::memcpy($buf, $src, $n * 4); my $ret = NIMCP::nimcp_ros2_bridge_inject_sensor($self->{_ros2_bridge}, $topic, $buf, $n); FFI::Platypus::Memory::free($buf); return $ret; }
sub ros2_bridge_get_last_cmd { my ($self, $max_count) = @_; $max_count //= 32; return [] unless $self->{_ros2_bridge}; my $buf = FFI::Platypus::Memory::calloc($max_count, 4); my $got = NIMCP::nimcp_ros2_bridge_get_last_cmd($self->{_ros2_bridge}, $buf, $max_count); if ($got < 0) { FFI::Platypus::Memory::free($buf); return []; } my $data = FFI::Platypus::Buffer::buffer_to_scalar($buf, $got * 4); FFI::Platypus::Memory::free($buf); return [unpack("f$got", $data)]; }

# --- MAVLink Bridge ---

sub mavlink_create { my ($self, %opts) = @_; my $cfg = NIMCP::nimcp_mavlink_config_default(); $self->{_mavlink_bridge} = NIMCP::nimcp_mavlink_bridge_create($cfg); return $self->{_mavlink_bridge} ? 1 : 0; }
sub mavlink_destroy { my ($self) = @_; if ($self->{_mavlink_bridge}) { NIMCP::nimcp_mavlink_bridge_destroy($self->{_mavlink_bridge}); $self->{_mavlink_bridge} = undef; } }
sub mavlink_connect { my ($self) = @_; return -1 unless $self->{_mavlink_bridge}; return NIMCP::nimcp_mavlink_bridge_connect($self->{_mavlink_bridge}); }
sub mavlink_disconnect { my ($self) = @_; return -1 unless $self->{_mavlink_bridge}; return NIMCP::nimcp_mavlink_bridge_disconnect($self->{_mavlink_bridge}); }
sub mavlink_start { my ($self) = @_; return -1 unless $self->{_mavlink_bridge}; return NIMCP::nimcp_mavlink_bridge_start($self->{_mavlink_bridge}); }
sub mavlink_stop { my ($self) = @_; return -1 unless $self->{_mavlink_bridge}; return NIMCP::nimcp_mavlink_bridge_stop($self->{_mavlink_bridge}); }
sub mavlink_set_velocity { my ($self, $vx, $vy, $vz, $yr) = @_; return -1 unless $self->{_mavlink_bridge}; return NIMCP::nimcp_mavlink_set_velocity($self->{_mavlink_bridge}, $vx, $vy, $vz, $yr); }
sub mavlink_arm { my ($self, $arm) = @_; $arm //= 1; return -1 unless $self->{_mavlink_bridge}; return NIMCP::nimcp_mavlink_arm($self->{_mavlink_bridge}, $arm ? 1 : 0); }
sub mavlink_takeoff { my ($self, $alt) = @_; $alt //= 5.0; return -1 unless $self->{_mavlink_bridge}; return NIMCP::nimcp_mavlink_takeoff($self->{_mavlink_bridge}, $alt); }
sub mavlink_land { my ($self) = @_; return -1 unless $self->{_mavlink_bridge}; return NIMCP::nimcp_mavlink_land($self->{_mavlink_bridge}); }
sub mavlink_goto { my ($self, $lat, $lon, $alt) = @_; $alt //= 10.0; return -1 unless $self->{_mavlink_bridge}; return NIMCP::nimcp_mavlink_goto($self->{_mavlink_bridge}, $lat, $lon, $alt); }
sub mavlink_rtl { my ($self) = @_; return -1 unless $self->{_mavlink_bridge}; return NIMCP::nimcp_mavlink_rtl($self->{_mavlink_bridge}); }
sub mavlink_compose_features { my ($self) = @_; return [] unless $self->{_mavlink_bridge}; my $buf = FFI::Platypus::Memory::calloc(14, 4); my $count = NIMCP::nimcp_mavlink_compose_features($self->{_mavlink_bridge}, $buf, 14); if ($count < 0) { FFI::Platypus::Memory::free($buf); return []; } my $data = FFI::Platypus::Buffer::buffer_to_scalar($buf, $count * 4); FFI::Platypus::Memory::free($buf); return [unpack("f$count", $data)]; }

# --- Memory Store ---

sub memory_store_stats {
    my ($self) = @_;
    # nimcp_memory_store_stats_t has 13 fields: 10 x uint64 + 2 x float + 1 x uint64
    my $stats_sz = 10 * 8 + 2 * 4 + 1 * 8;  # 96 bytes
    my $stats = FFI::Platypus::Memory::calloc(1, $stats_sz);
    my $ret = NIMCP::nimcp_brain_memory_store_stats($self->{_handle}, $stats);
    return undef if $ret != 0;
    my $data = FFI::Platypus::Buffer::buffer_to_scalar($stats, $stats_sz);
    my @vals = unpack('Q10f2Q', $data);
    FFI::Platypus::Memory::free($stats);
    return {
        total_engrams => $vals[0], total_concepts => $vals[1],
        total_relations => $vals[2], total_autobio => $vals[3],
        total_writes => $vals[4], total_reads => $vals[5],
        cache_hits => $vals[6], cache_misses => $vals[7],
        write_buffer_flushes => $vals[8], bloom_filter_hits => $vals[9],
        avg_write_latency_ms => $vals[10], avg_read_latency_ms => $vals[11],
        db_size_bytes => $vals[12],
    };
}

sub memory_search_text {
    my ($self, $query, $max_results) = @_;
    $max_results //= 10;
    my $ids_buf = FFI::Platypus::Memory::calloc($max_results, 8);
    my $count = 0;
    NIMCP::nimcp_brain_memory_search_text(
        $self->{_handle}, $query, $max_results, $ids_buf, \$count);
    return [] if $count == 0;
    my $data = FFI::Platypus::Buffer::buffer_to_scalar($ids_buf, $count * 8);
    FFI::Platypus::Memory::free($ids_buf);
    return [unpack("Q$count", $data)];
}

sub memory_search_similar {
    my ($self, $embedding, $top_k) = @_;
    die "embedding must be array ref" unless ref $embedding eq 'ARRAY';
    $top_k //= 5;
    my $dim = scalar @$embedding;
    my $ids_buf = FFI::Platypus::Memory::calloc($top_k, 8);
    my $dist_buf = FFI::Platypus::Memory::calloc($top_k, 4);
    my $count = 0;
    NIMCP::nimcp_brain_memory_search_similar(
        $self->{_handle}, $embedding, $dim, $top_k,
        $ids_buf, $dist_buf, \$count);
    my @results;
    if ($count > 0) {
        my $id_data = FFI::Platypus::Buffer::buffer_to_scalar($ids_buf, $count * 8);
        my $dist_data = FFI::Platypus::Buffer::buffer_to_scalar($dist_buf, $count * 4);
        my @ids = unpack("Q$count", $id_data);
        my @dists = unpack("f$count", $dist_data);
        for my $i (0 .. $count - 1) {
            push @results, { id => $ids[$i], distance => $dists[$i] };
        }
    }
    FFI::Platypus::Memory::free($ids_buf);
    FFI::Platypus::Memory::free($dist_buf);
    return \@results;
}

sub memory_is_healthy {
    my ($self) = @_;
    return NIMCP::nimcp_brain_memory_is_healthy($self->{_handle}) ? 1 : 0;
}

# --- OOD Detection ---

sub ood_stats {
    my ($self) = @_;
    # nimcp_ood_stats_t: 3 x uint64 + 3 x float = 36 bytes
    my $stats_sz = 3 * 8 + 3 * 4;
    my $stats = FFI::Platypus::Memory::calloc(1, $stats_sz);
    my $ret = NIMCP::nimcp_brain_ood_stats($self->{_handle}, $stats);
    return undef if $ret != 0;
    my $data = FFI::Platypus::Buffer::buffer_to_scalar($stats, $stats_sz);
    my @vals = unpack('Q3f3', $data);
    FFI::Platypus::Memory::free($stats);
    return {
        total_checks => $vals[0], ood_detected => $vals[1],
        in_distribution => $vals[2], avg_ood_score => $vals[3],
        max_ood_score => $vals[4], ood_rate => $vals[5],
    };
}

# --- Audit Trail ---

sub audit_log {
    my ($self, $description, %opts) = @_;
    my $severity = $opts{severity} // 0;
    my $details  = $opts{details} // '';
    return NIMCP::nimcp_brain_audit_log(
        $self->{_handle}, $description, $severity, $details);
}

sub audit_search {
    my ($self, %opts) = @_;
    my $min_sev = $opts{min_severity} // 0;
    my $max_res = $opts{max_results} // 100;
    my $ids_buf = FFI::Platypus::Memory::calloc($max_res, 8);
    my $sev_buf = FFI::Platypus::Memory::calloc($max_res, 4);
    my $count = 0;
    NIMCP::nimcp_brain_audit_search(
        $self->{_handle}, $min_sev, $max_res,
        $ids_buf, $sev_buf, \$count);
    my @results;
    if ($count > 0) {
        my $id_data = FFI::Platypus::Buffer::buffer_to_scalar($ids_buf, $count * 8);
        my $sev_data = FFI::Platypus::Buffer::buffer_to_scalar($sev_buf, $count * 4);
        my @ids = unpack("Q$count", $id_data);
        my @sevs = unpack("f$count", $sev_data);
        for my $i (0 .. $count - 1) {
            push @results, { id => $ids[$i], severity => $sevs[$i] };
        }
    }
    FFI::Platypus::Memory::free($ids_buf);
    FFI::Platypus::Memory::free($sev_buf);
    return \@results;
}

sub DESTROY {
    my ($self) = @_;
    if ($self->{_handle}) {
        NIMCP::nimcp_brain_destroy($self->{_handle});
        $self->{_handle} = undef;
    }
}

# ============================================================================
# NIMCP::BrainSnapshot
# ============================================================================

package NIMCP::BrainSnapshot;

sub _new {
    my ($class, $handle) = @_;
    return bless { _handle => $handle }, $class;
}

sub DESTROY {
    my ($self) = @_;
    if ($self->{_handle}) {
        NIMCP::nimcp_brain_snapshot_destroy($self->{_handle});
        $self->{_handle} = undef;
    }
}

# ============================================================================
# NIMCP::Network
# ============================================================================

package NIMCP::Network;

sub new {
    my ($class, %args) = @_;
    my $num_inputs  = $args{num_inputs}  // die "num_inputs required";
    my $num_outputs = $args{num_outputs} // die "num_outputs required";
    my $num_hidden  = $args{num_hidden}  // 100;
    my $lr          = $args{learning_rate} // 0.01;

    my $handle = NIMCP::nimcp_network_create($num_inputs, $num_outputs, $num_hidden, $lr);
    die "Failed to create network: " . NIMCP::nimcp_get_error() unless $handle;

    return bless {
        _handle      => $handle,
        _num_inputs  => $num_inputs,
        _num_outputs => $num_outputs,
    }, $class;
}

sub forward {
    my ($self, $inputs) = @_;
    die "inputs must be array ref" unless ref $inputs eq 'ARRAY';
    my $ni = scalar @$inputs;
    my $no = $self->{_num_outputs};
    my @out = (0.0) x $no;
    my $status = NIMCP::nimcp_network_forward($self->{_handle}, $inputs, $ni, \@out, $no);
    NIMCP::_check_status($status, 'network_forward');
    return \@out;
}

sub train {
    my ($self, $inputs, $targets) = @_;
    die "inputs must be array ref" unless ref $inputs eq 'ARRAY';
    die "targets must be array ref" unless ref $targets eq 'ARRAY';
    my $ni = scalar @$inputs;
    my $nt = scalar @$targets;
    my $status = NIMCP::nimcp_network_train($self->{_handle}, $inputs, $ni, $targets, $nt);
    NIMCP::_check_status($status, 'network_train');
    return $self;
}

sub DESTROY {
    my ($self) = @_;
    if ($self->{_handle}) {
        NIMCP::nimcp_network_destroy($self->{_handle});
        $self->{_handle} = undef;
    }
}

# ============================================================================
# NIMCP::Ethics
# ============================================================================

package NIMCP::Ethics;

sub new {
    my ($class) = @_;
    my $handle = NIMCP::nimcp_ethics_create();
    die "Failed to create ethics module: " . NIMCP::nimcp_get_error() unless $handle;
    return bless { _handle => $handle }, $class;
}

sub check {
    my ($self, $situation) = @_;
    die "situation must be array ref" unless ref $situation eq 'ARRAY';
    my $n = scalar @$situation;
    my $score = 0.0;
    my $status = NIMCP::nimcp_ethics_check($self->{_handle}, $situation, $n, \$score);
    NIMCP::_check_status($status, 'ethics_check');
    return $score;
}

sub DESTROY {
    my ($self) = @_;
    if ($self->{_handle}) {
        NIMCP::nimcp_ethics_destroy($self->{_handle});
        $self->{_handle} = undef;
    }
}

# ============================================================================
# NIMCP::KnowledgeGraph
# ============================================================================

package NIMCP::KnowledgeGraph;

sub new {
    my ($class) = @_;
    my $handle = NIMCP::nimcp_knowledge_create();
    die "Failed to create knowledge graph: " . NIMCP::nimcp_get_error() unless $handle;
    return bless { _handle => $handle }, $class;
}

sub add_fact {
    my ($self, $subject, $predicate, $object) = @_;
    my $status = NIMCP::nimcp_knowledge_add_fact($self->{_handle}, $subject, $predicate, $object);
    NIMCP::_check_status($status, 'knowledge_add_fact');
    return $self;
}

sub query {
    my ($self, $query_str) = @_;
    my $buf_size = 1024;
    my $buf_ptr = FFI::Platypus::Memory::calloc(1, $buf_size);
    my $status = NIMCP::nimcp_knowledge_query($self->{_handle}, $query_str, $buf_ptr, $buf_size);
    my $result = FFI::Platypus::Buffer::buffer_to_scalar($buf_ptr, $buf_size);
    FFI::Platypus::Memory::free($buf_ptr);
    NIMCP::_check_status($status, 'knowledge_query');
    $result =~ s/\0.*//s;
    return $result;
}

sub DESTROY {
    my ($self) = @_;
    if ($self->{_handle}) {
        NIMCP::nimcp_knowledge_destroy($self->{_handle});
        $self->{_handle} = undef;
    }
}

1;

__END__

=head1 NAME

NIMCP - Perl bindings for Neural Interface Message Communication Protocol (v2.6.4)

=head1 SYNOPSIS

    use NIMCP;

    NIMCP::init();
    print "NIMCP version: " . NIMCP::version() . "\n";

    my $brain = NIMCP::Brain->new(
        name => 'classifier', size => NIMCP::BRAIN_SMALL,
        task => NIMCP::TASK_CLASSIFICATION, num_inputs => 5, num_outputs => 3,
    );

    $brain->learn([1.0, 2.0, 3.0, 4.0, 5.0], 'class_a', 0.95);
    my ($label, $confidence) = $brain->predict([1.5, 2.5, 3.5, 4.5, 5.5]);

    # Fast prediction (training loop)
    my ($label2, $conf2) = $brain->predict_fast([1.0, 2.0, 3.0, 4.0, 5.0]);

    # Language
    $brain->learn_language("The cat sat on the mat");
    my $result = $brain->speak();
    print "Brain says: $result->{text}\n";

    # Cloud
    my $cloud = NIMCP::Brain->create_full(
        name => 'cloud', task => NIMCP::TASK_CLASSIFICATION,
        num_inputs => 5, num_outputs => 3, neuron_count => 100000,
    );
    $brain->connect_cloud($cloud, 0.5, 1);

    my $net = NIMCP::Network->new(num_inputs => 10, num_outputs => 5);
    my $outputs = $net->forward([1..10]);

    NIMCP::shutdown();

=head1 DESCRIPTION

Complete Perl bindings for the NIMCP C library using FFI::Platypus.
Wraps the entire public C API including Brain, Network, Ethics,
KnowledgeGraph, training pipeline, callbacks, COW snapshots,
working memory, global workspace, oscillations, language production,
grounded language, cognitive training, sub-network stats, sensory
processing, cloud inference, and biological plasticity controls.

=head1 LICENSE

MIT License

=cut
