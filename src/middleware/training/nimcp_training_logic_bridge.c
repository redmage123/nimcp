#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_training_logic_bridge.c - Training-Logic Bridge Implementation
//=============================================================================

#include "middleware/training/nimcp_training_logic_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "middleware/training/nimcp_perception_training_bridge.h"
#include "middleware/training/nimcp_cortical_training_bridge.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(training_logic_bridge)

static uint64_t get_time_us(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL; }
static uint64_t get_time_ms(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL; }

#ifndef BIO_MODULE_TRAINING_LOGIC
#define BIO_MODULE_TRAINING_LOGIC 0x0520
#endif
#define DEFAULT_LOSS_STABILITY_THRESHOLD 0.1f
#define DEFAULT_GRAD_NORM_MIN 1e-7f
#define DEFAULT_GRAD_NORM_MAX 10.0f
#define DEFAULT_LR_MIN 1e-8f
#define DEFAULT_LR_MAX 1.0f
#define DEFAULT_MEMORY_THRESHOLD 0.85f
#define DEFAULT_THROUGHPUT_MIN 1.0f
#define DEFAULT_LOSS_TREND_WINDOW 10

typedef struct { float loss; float grad_norm; uint64_t timestamp_ms; } training_metric_history_entry_t;

struct training_logic_bridge {
    bridge_base_t base;
    neural_logic_network_t logic_network;
    uint32_t stability_check_gate, intervention_gate, lr_increase_gate, batch_size_gate, checkpoint_gate;
    training_logic_conditions_t conditions;
    training_logic_config_t config;
    nimcp_brain_training_ctx_t* training_ctx;
    training_immune_system_t* immune_system;
    portia_logic_bridge_t* portia_logic;
    swarm_logic_bridge_t* swarm_logic;
    portia_swarm_logic_bridge_t* unified_bridge;
    perception_training_bridge_t* perception_training;
    cortical_training_bridge_t* cortical_training;
    training_logic_stats_t stats;
    uint32_t next_custom_gate_id;
    training_metric_history_entry_t* history;
    uint32_t history_head, history_count;
    float last_lr_factor;
    uint32_t last_batch_size;
};

/* Forward declaration */
training_logic_instability_t training_logic_classify_instability(const training_instability_metrics_t* metrics);

static void update_instability_from_metrics(training_logic_bridge_t* bridge) {
    training_instability_metrics_t* inst = &bridge->conditions.instability;
    if (isnan(bridge->conditions.loss_current) || isinf(bridge->conditions.loss_current)) inst->nan_inf_severity = 1.0f;
    if (bridge->conditions.grad_norm > DEFAULT_GRAD_NORM_MIN) { float gr = bridge->conditions.grad_norm / DEFAULT_GRAD_NORM_MAX; if (gr > 1.0f) inst->gradient_explosion = fminf(1.0f, (gr-1.0f)/9.0f+0.5f); else if (gr > 0.8f) inst->gradient_explosion = fmaxf(inst->gradient_explosion, (gr-0.8f)*2.5f); }
    if (bridge->conditions.grad_norm >= 0.0f && bridge->conditions.grad_norm < DEFAULT_GRAD_NORM_MAX) { float vr = bridge->conditions.grad_norm / DEFAULT_GRAD_NORM_MIN; if (vr < 1.0f) inst->gradient_vanishing = fminf(1.0f, 1.0f - vr); }
    if (bridge->history_count >= 2) { float mean=0,var=0; uint32_t cnt=(bridge->history_count<(uint32_t)DEFAULT_LOSS_TREND_WINDOW)?bridge->history_count:(uint32_t)DEFAULT_LOSS_TREND_WINDOW; for(uint32_t i=0;i<cnt;i++){uint32_t idx=(bridge->history_head+bridge->history_count-1-i)%bridge->config.history_size;mean+=bridge->history[idx].loss;}mean/=cnt;for(uint32_t i=0;i<cnt;i++){uint32_t idx=(bridge->history_head+bridge->history_count-1-i)%bridge->config.history_size;float d=bridge->history[idx].loss-mean;var+=d*d;}var/=cnt;inst->loss_volatility=fmaxf(inst->loss_volatility,fminf(1.0f,var/(DEFAULT_LOSS_STABILITY_THRESHOLD*10.0f))); }
    inst->instability_score = fmaxf(fmaxf(inst->nan_inf_severity,inst->gradient_explosion),fmaxf(fmaxf(inst->gradient_vanishing,inst->loss_volatility),fmaxf(inst->loss_plateau,inst->gradient_variance)));
    if (inst->instability_score > 1.0f) inst->instability_score = 1.0f;
    inst->derived_label = training_logic_classify_instability(inst);
}

static int update_conditions_internal(training_logic_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    training_logic_conditions_t* cond = &bridge->conditions;
    if (bridge->history_count >= 2) { float mean=0,var=0; uint32_t count=(bridge->history_count<DEFAULT_LOSS_TREND_WINDOW)?bridge->history_count:DEFAULT_LOSS_TREND_WINDOW; for(uint32_t i=0;i<count;i++){uint32_t idx=(bridge->history_head+bridge->history_count-1-i)%bridge->config.history_size;mean+=bridge->history[idx].loss;}mean/=count;for(uint32_t i=0;i<count;i++){uint32_t idx=(bridge->history_head+bridge->history_count-1-i)%bridge->config.history_size;float diff=bridge->history[idx].loss-mean;var+=diff*diff;}var/=count;cond->loss_stable=(var<DEFAULT_LOSS_STABILITY_THRESHOLD); } else { cond->loss_stable=true; }
    cond->grad_stable=(cond->grad_norm>=DEFAULT_GRAD_NORM_MIN&&cond->grad_norm<=DEFAULT_GRAD_NORM_MAX);
    cond->lr_reasonable=(cond->learning_rate>=DEFAULT_LR_MIN&&cond->learning_rate<=DEFAULT_LR_MAX);
    cond->memory_ok=(cond->memory_usage<DEFAULT_MEMORY_THRESHOLD);
    cond->throughput_ok=(cond->throughput>=DEFAULT_THROUGHPUT_MIN);
    cond->grad_exploding=cond->grad_exploding||(cond->grad_norm>DEFAULT_GRAD_NORM_MAX);
    cond->loss_nan=cond->loss_nan||(isnan(cond->loss_current)||isinf(cond->loss_current));
    if (bridge->history_count>=2){uint32_t li=(bridge->history_head+bridge->history_count-1)%bridge->config.history_size;uint32_t pi=(bridge->history_head+bridge->history_count-2)%bridge->config.history_size;float lr=bridge->history[li].loss/(bridge->history[pi].loss+1e-10f);cond->diverging=cond->diverging||(lr>10.0f);}
    if(cond->loss_stable&&cond->grad_stable&&cond->lr_reasonable){cond->stable_step_count++;}else{cond->stable_step_count=0;}
    cond->stable_for_n_steps=(cond->stable_step_count>=bridge->config.stable_steps_required);
    if(bridge->immune_system&&bridge->config.enable_immune_integration)cond->immune_ok=true;
    if(bridge->portia_logic&&bridge->config.enable_portia_integration)cond->resource_ok=true;
    if(bridge->swarm_logic&&bridge->config.enable_swarm_integration)cond->swarm_consensus=true;
    if(bridge->perception_training){perception_training_effects_t pe;if(perception_training_get_effects(bridge->perception_training,&pe)==0&&pe.valid)cond->perception_quality=(pe.lr_factor>0.8f);}
    if(bridge->cortical_training){cortical_training_effects_t ce;if(cortical_training_get_effects(bridge->cortical_training,&ce)==0&&ce.valid){cond->cortical_stable=ce.predictions_stable;cond->predictions_ok=(ce.burst_rate>0.5f);}}
    cond->steps_since_checkpoint++;
    cond->sufficient_progress=(cond->steps_since_checkpoint>=bridge->config.checkpoint_interval);
    cond->not_mid_batch=true;
    update_instability_from_metrics(bridge);
    return NIMCP_SUCCESS;
}

static bool check_stability_internal(training_logic_bridge_t* b){return b->conditions.loss_stable&&b->conditions.grad_stable&&b->conditions.lr_reasonable;}
static bool needs_intervention_internal(training_logic_bridge_t* b){return b->conditions.grad_exploding||b->conditions.loss_nan||b->conditions.diverging;}
static bool can_increase_lr_internal(training_logic_bridge_t* b){return b->conditions.stable_for_n_steps&&b->conditions.immune_ok&&b->conditions.resource_ok;}
static bool should_adjust_batch_internal(training_logic_bridge_t* b,bool* ib){*ib=b->conditions.memory_ok&&b->conditions.throughput_ok;return true;}
static bool should_checkpoint_internal(training_logic_bridge_t* b){return b->conditions.memory_ok&&b->conditions.not_mid_batch&&b->conditions.sufficient_progress;}

static void compute_instability_response_internal(const training_logic_bridge_t* bridge, training_instability_response_t* r) {
    float s=bridge->conditions.instability.instability_score; if(s<0)s=0; if(s>1)s=1;
    r->lr_scale=expf(-3.0f*s); r->clip_threshold=DEFAULT_GRAD_NORM_MAX/(1.0f+5.0f*s); r->batch_scale=1.0f-0.5f*s*s;
    r->pause_urgency=s*s; r->checkpoint_urgency=s; r->rollback_urgency=fmaxf(0.0f,2.0f*s-1.0f);
    r->derived_label=training_logic_classify_instability(&bridge->conditions.instability);
}

static int init_decision_gates(training_logic_bridge_t* b) {
    NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");NIMCP_CHECK_THROW(b->logic_network!=NULL,NIMCP_ERROR_NULL_POINTER,"logic_network is NULL");
    b->stability_check_gate=neural_logic_create_gate(b->logic_network,LOGIC_GATE_AND,2.9f);if(b->stability_check_gate==UINT32_MAX)return NIMCP_ERROR_OPERATION_FAILED;
    b->intervention_gate=neural_logic_create_gate(b->logic_network,LOGIC_GATE_OR,0.5f);if(b->intervention_gate==UINT32_MAX)return NIMCP_ERROR_OPERATION_FAILED;
    b->lr_increase_gate=neural_logic_create_gate(b->logic_network,LOGIC_GATE_IMPLIES,0.7f);if(b->lr_increase_gate==UINT32_MAX)return NIMCP_ERROR_OPERATION_FAILED;
    b->batch_size_gate=neural_logic_create_gate(b->logic_network,LOGIC_GATE_AND,1.5f);if(b->batch_size_gate==UINT32_MAX)return NIMCP_ERROR_OPERATION_FAILED;
    b->checkpoint_gate=neural_logic_create_gate(b->logic_network,LOGIC_GATE_AND,2.5f);if(b->checkpoint_gate==UINT32_MAX)return NIMCP_ERROR_OPERATION_FAILED;
    return NIMCP_SUCCESS;
}

static void add_metric_to_history(training_logic_bridge_t* b,float loss,float gn){if(!b||!b->history)return;uint32_t idx=(b->history_head+b->history_count)%b->config.history_size;b->history[idx].loss=loss;b->history[idx].grad_norm=gn;b->history[idx].timestamp_ms=get_time_ms();if(b->history_count<b->config.history_size)b->history_count++;else b->history_head=(b->history_head+1)%b->config.history_size;}

void training_logic_default_config(training_logic_config_t* c) {
    if(!c)return; memset(c,0,sizeof(*c)); c->mode=TRAINING_LOGIC_MODE_ADVISORY; c->stability_threshold=0.7f; c->intervention_threshold=0.5f; c->lr_increase_threshold=0.7f; c->confidence_threshold=TRAINING_LOGIC_DEFAULT_CONFIDENCE_THRESHOLD; c->lr_increase_factor=TRAINING_LOGIC_DEFAULT_LR_INCREASE_FACTOR; c->lr_decrease_factor=TRAINING_LOGIC_DEFAULT_LR_DECREASE_FACTOR; c->batch_scale_factor=TRAINING_LOGIC_DEFAULT_BATCH_SCALE_FACTOR; c->stable_steps_required=TRAINING_LOGIC_DEFAULT_STABLE_STEPS; c->checkpoint_interval=TRAINING_LOGIC_DEFAULT_CHECKPOINT_INTERVAL; c->enable_bio_async=true; c->min_learning_rate=1e-8f; c->max_learning_rate=1.0f; c->min_batch_size=1; c->max_batch_size=1024; c->consensus_timeout_ms=TRAINING_LOGIC_DEFAULT_CONSENSUS_TIMEOUT_MS; c->consensus_threshold=0.7f; c->history_size=TRAINING_LOGIC_MAX_HISTORY_SIZE;
}

training_logic_bridge_t* training_logic_create(const training_logic_config_t* config) {
    training_logic_config_t dc; if(!config){training_logic_default_config(&dc);config=&dc;}
    training_logic_bridge_t* b=(training_logic_bridge_t*)nimcp_malloc(sizeof(*b)); if(!b){NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,"bridge is NULL");return NULL;}
    memset(b,0,sizeof(*b)); memcpy(&b->config,config,sizeof(*config));
    if(bridge_base_init(&b->base,0,"training_logic")!=0){nimcp_free(b);return NULL;} if(!b->base.mutex){nimcp_free(b);NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,"mutex");return NULL;}
    b->history=(training_metric_history_entry_t*)nimcp_malloc(sizeof(training_metric_history_entry_t)*config->history_size); if(!b->history){bridge_base_cleanup(&b->base);nimcp_free(b);NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,"history");return NULL;} memset(b->history,0,sizeof(training_metric_history_entry_t)*config->history_size);
    neural_logic_config_t lc=neural_logic_default_config(TRAINING_LOGIC_MAX_CUSTOM_GATES+10);lc.enable_bio_async=config->enable_bio_async;lc.use_gpu=false; b->logic_network=neural_logic_create(&lc); if(!b->logic_network){nimcp_free(b->history);bridge_base_cleanup(&b->base);nimcp_free(b);NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,"logic");return NULL;}
    if(init_decision_gates(b)!=NIMCP_SUCCESS){neural_logic_destroy(b->logic_network);nimcp_free(b->history);bridge_base_cleanup(&b->base);nimcp_free(b);NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED,"gates");return NULL;}
    b->next_custom_gate_id=TRAINING_LOGIC_GATE_CUSTOM_START; b->last_lr_factor=1.0f; b->stats.current_mode=config->mode; return b;
}

void training_logic_destroy(training_logic_bridge_t* b) { if(!b)return; if(b->base.bio_async_enabled)training_logic_disconnect_bio_async(b); if(b->logic_network)neural_logic_destroy(b->logic_network); if(b->history)nimcp_free(b->history); if(b->base.mutex)bridge_base_cleanup(&b->base); nimcp_free(b); }
int training_logic_start(training_logic_bridge_t* b){NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");nimcp_mutex_lock(b->base.mutex);if(b->config.enable_bio_async&&!b->base.bio_async_enabled)training_logic_connect_bio_async(b);if(!b->config.disable_auto_update)update_conditions_internal(b);nimcp_mutex_unlock(b->base.mutex);return NIMCP_SUCCESS;}
int training_logic_stop(training_logic_bridge_t* b){NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");nimcp_mutex_lock(b->base.mutex);if(b->base.bio_async_enabled)training_logic_disconnect_bio_async(b);nimcp_mutex_unlock(b->base.mutex);return NIMCP_SUCCESS;}

int training_logic_connect_brain_training(training_logic_bridge_t* b,nimcp_brain_training_ctx_t* tc){NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");nimcp_mutex_lock(b->base.mutex);b->training_ctx=tc;nimcp_mutex_unlock(b->base.mutex);return NIMCP_SUCCESS;}
int training_logic_connect_training_immune(training_logic_bridge_t* b,training_immune_system_t* is){NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");nimcp_mutex_lock(b->base.mutex);b->immune_system=is;nimcp_mutex_unlock(b->base.mutex);return NIMCP_SUCCESS;}
int training_logic_connect_portia_logic(training_logic_bridge_t* b,portia_logic_bridge_t* pl){NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");nimcp_mutex_lock(b->base.mutex);b->portia_logic=pl;nimcp_mutex_unlock(b->base.mutex);return NIMCP_SUCCESS;}
int training_logic_connect_swarm_logic(training_logic_bridge_t* b,swarm_logic_bridge_t* sl){NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");nimcp_mutex_lock(b->base.mutex);b->swarm_logic=sl;nimcp_mutex_unlock(b->base.mutex);return NIMCP_SUCCESS;}
int training_logic_connect_unified(training_logic_bridge_t* b,portia_swarm_logic_bridge_t* ub){NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");nimcp_mutex_lock(b->base.mutex);b->unified_bridge=ub;nimcp_mutex_unlock(b->base.mutex);return NIMCP_SUCCESS;}
int training_logic_connect_perception_training(training_logic_bridge_t* b,perception_training_bridge_t* pt){NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");nimcp_mutex_lock(b->base.mutex);b->perception_training=pt;nimcp_mutex_unlock(b->base.mutex);return NIMCP_SUCCESS;}
int training_logic_connect_cortical_training(training_logic_bridge_t* b,cortical_training_bridge_t* ct){NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");nimcp_mutex_lock(b->base.mutex);b->cortical_training=ct;nimcp_mutex_unlock(b->base.mutex);return NIMCP_SUCCESS;}

int training_logic_update_metrics(training_logic_bridge_t* b,float loss,float gn,float lr,uint64_t step){NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");nimcp_mutex_lock(b->base.mutex);b->conditions.loss_current=loss;b->conditions.grad_norm=gn;b->conditions.learning_rate=lr;b->conditions.current_step=step;b->conditions.loss_nan=isnan(loss)||isinf(loss);b->conditions.grad_exploding=isnan(gn)||isinf(gn)||gn>b->config.intervention_threshold;add_metric_to_history(b,loss,gn);if(!b->config.disable_auto_update)update_conditions_internal(b);nimcp_mutex_unlock(b->base.mutex);return NIMCP_SUCCESS;}
int training_logic_update_batch_metrics(training_logic_bridge_t* b,uint32_t bs,float tp,float mu){NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");nimcp_mutex_lock(b->base.mutex);b->conditions.memory_usage=mu;b->conditions.throughput=tp;b->last_batch_size=bs;if(!b->config.disable_auto_update)update_conditions_internal(b);nimcp_mutex_unlock(b->base.mutex);return NIMCP_SUCCESS;}

int training_logic_signal_instability(training_logic_bridge_t* b,training_logic_instability_t it,uint32_t sev) {
    NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL"); NIMCP_CHECK_THROW(it<LOGIC_INSTABILITY_COUNT,NIMCP_ERROR_INVALID_PARAM,"type out of range");
    nimcp_mutex_lock(b->base.mutex);
    float ns=(sev>10)?1.0f:(float)sev/10.0f; training_instability_metrics_t* m=&b->conditions.instability;
    switch(it){
        case LOGIC_INSTABILITY_LOSS_NAN: case LOGIC_INSTABILITY_LOSS_INF: m->nan_inf_severity=fmaxf(m->nan_inf_severity,ns); break;
        case LOGIC_INSTABILITY_LOSS_EXPLOSION: m->loss_volatility=fmaxf(m->loss_volatility,ns); break;
        case LOGIC_INSTABILITY_GRAD_EXPLOSION: m->gradient_explosion=fmaxf(m->gradient_explosion,ns); break;
        case LOGIC_INSTABILITY_GRAD_VANISHING: m->gradient_vanishing=fmaxf(m->gradient_vanishing,ns); break;
        case LOGIC_INSTABILITY_LOSS_PLATEAU: m->loss_plateau=fmaxf(m->loss_plateau,ns); break;
        case LOGIC_INSTABILITY_OSCILLATION: m->loss_volatility=fmaxf(m->loss_volatility,ns); m->gradient_variance=fmaxf(m->gradient_variance,ns*0.8f); break;
        default: break;
    }
    m->instability_score=fmaxf(fmaxf(m->nan_inf_severity,m->gradient_explosion),fmaxf(fmaxf(m->gradient_vanishing,m->loss_volatility),fmaxf(m->loss_plateau,m->gradient_variance)));
    if(m->instability_score>1.0f)m->instability_score=1.0f; m->derived_label=training_logic_classify_instability(m);
    if(m->nan_inf_severity>0.5f)b->conditions.loss_nan=true; if(m->gradient_explosion>0.5f)b->conditions.grad_exploding=true;
    if(m->gradient_vanishing>0.5f)b->conditions.grad_stable=false; if(m->loss_plateau>0.5f)b->conditions.loss_stable=false;
    if(m->loss_volatility>0.5f){b->conditions.loss_stable=false;b->conditions.diverging=true;}
    b->stats.intervention_triggers++;
    NIMCP_LOGGING_WARN("Training instability signaled: type=%s severity=%u score=%.3f",training_logic_instability_to_string(it),sev,m->instability_score);
    nimcp_mutex_unlock(b->base.mutex); return NIMCP_SUCCESS;
}

int training_logic_compute_instability_response(const training_logic_bridge_t* b,training_instability_response_t* r){NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");NIMCP_CHECK_THROW(r!=NULL,NIMCP_ERROR_NULL_POINTER,"response is NULL");compute_instability_response_internal(b,r);return NIMCP_SUCCESS;}

training_logic_instability_t training_logic_classify_instability(const training_instability_metrics_t* m) {
    if(!m||m->instability_score<0.05f)return LOGIC_INSTABILITY_NONE;
    if(m->nan_inf_severity>0.5f)return LOGIC_INSTABILITY_LOSS_NAN; if(m->gradient_explosion>0.5f)return LOGIC_INSTABILITY_GRAD_EXPLOSION;
    if(m->gradient_vanishing>0.5f)return LOGIC_INSTABILITY_GRAD_VANISHING;
    if(m->loss_volatility>0.5f&&m->gradient_variance>0.3f)return LOGIC_INSTABILITY_OSCILLATION;
    if(m->loss_volatility>0.5f)return LOGIC_INSTABILITY_LOSS_EXPLOSION; if(m->loss_plateau>0.5f)return LOGIC_INSTABILITY_LOSS_PLATEAU;
    if(m->instability_score>0.7f)return LOGIC_INSTABILITY_LOSS_EXPLOSION; return LOGIC_INSTABILITY_NONE;
}

int training_logic_get_instability_metrics(const training_logic_bridge_t* b,training_instability_metrics_t* m){NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");NIMCP_CHECK_THROW(m!=NULL,NIMCP_ERROR_NULL_POINTER,"metrics is NULL");memcpy(m,&b->conditions.instability,sizeof(*m));return NIMCP_SUCCESS;}

const char* training_logic_instability_to_string(training_logic_instability_t t) {
    switch(t){case LOGIC_INSTABILITY_NONE:return"none";case LOGIC_INSTABILITY_LOSS_NAN:return"loss_nan";case LOGIC_INSTABILITY_LOSS_INF:return"loss_inf";case LOGIC_INSTABILITY_LOSS_EXPLOSION:return"loss_explosion";case LOGIC_INSTABILITY_GRAD_EXPLOSION:return"grad_explosion";case LOGIC_INSTABILITY_GRAD_VANISHING:return"grad_vanishing";case LOGIC_INSTABILITY_LOSS_PLATEAU:return"loss_plateau";case LOGIC_INSTABILITY_OSCILLATION:return"oscillation";default:return"unknown";}
}

bool training_logic_check_stability(training_logic_bridge_t* b){if(!b||!b->logic_network)return false;nimcp_mutex_lock(b->base.mutex);if(!b->config.disable_auto_update)update_conditions_internal(b);bool s=check_stability_internal(b);b->stats.stability_checks++;if(s)b->stats.stability_passed++;nimcp_mutex_unlock(b->base.mutex);return s;}
bool training_logic_needs_intervention(training_logic_bridge_t* b){if(!b||!b->logic_network){NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,"NULL");return false;}nimcp_mutex_lock(b->base.mutex);if(!b->config.disable_auto_update)update_conditions_internal(b);bool n=needs_intervention_internal(b);if(n)b->stats.intervention_triggers++;nimcp_mutex_unlock(b->base.mutex);return n;}
bool training_logic_can_increase_lr(training_logic_bridge_t* b){if(!b||!b->logic_network)return false;nimcp_mutex_lock(b->base.mutex);if(!b->config.disable_auto_update)update_conditions_internal(b);bool c=can_increase_lr_internal(b);if(c)b->stats.lr_increase_allowed++;nimcp_mutex_unlock(b->base.mutex);return c;}
bool training_logic_should_adjust_batch(training_logic_bridge_t* b,bool* ib){if(!b||!b->logic_network||!ib)return false;nimcp_mutex_lock(b->base.mutex);if(!b->config.disable_auto_update)update_conditions_internal(b);bool r=should_adjust_batch_internal(b,ib);b->stats.batch_adjustments++;nimcp_mutex_unlock(b->base.mutex);return r;}
bool training_logic_should_checkpoint(training_logic_bridge_t* b){if(!b||!b->logic_network)return false;nimcp_mutex_lock(b->base.mutex);if(!b->config.disable_auto_update)update_conditions_internal(b);bool r=should_checkpoint_internal(b);if(r){b->stats.checkpoints_triggered++;b->conditions.steps_since_checkpoint=0;}nimcp_mutex_unlock(b->base.mutex);return r;}

int training_logic_get_decision(training_logic_bridge_t* b,training_logic_decision_t* d) {
    NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");NIMCP_CHECK_THROW(d!=NULL,NIMCP_ERROR_NULL_POINTER,"decision is NULL");
    nimcp_mutex_lock(b->base.mutex); uint64_t t0=get_time_us();
    if(!b->config.disable_auto_update)update_conditions_internal(b); memset(d,0,sizeof(*d));
    d->stability_check_passed=check_stability_internal(b); d->intervention_needed=needs_intervention_internal(b);
    d->safe_to_increase_lr=can_increase_lr_internal(b); d->checkpoint_needed=should_checkpoint_internal(b);
    bool ib=false; d->batch_size_ok=should_adjust_batch_internal(b,&ib);
    b->stats.stability_checks++; if(d->stability_check_passed)b->stats.stability_passed++; if(d->intervention_needed)b->stats.intervention_triggers++;
    float is=b->conditions.instability.instability_score; training_instability_response_t r; compute_instability_response_internal(b,&r);
    if(is>0.8f||d->intervention_needed){d->type=TRAINING_DECISION_PAUSE;d->confidence=fminf(0.99f,0.7f+0.3f*is);d->approved=true;d->modulation_factor=r.lr_scale;snprintf(d->reason,TRAINING_LOGIC_MAX_REASON_LENGTH,"PAUSE: instability_score=%.3f (label=%s)",is,training_logic_instability_to_string(b->conditions.instability.derived_label));}
    else if(is>0.3f){d->type=TRAINING_DECISION_DECREASE_LR;d->confidence=0.70f+0.2f*is;d->approved=true;d->modulation_factor=r.lr_scale;b->stats.lr_decrease_triggered++;snprintf(d->reason,TRAINING_LOGIC_MAX_REASON_LENGTH,"Decrease LR: instability=%.3f lr_scale=%.3f",is,r.lr_scale);}
    else if(d->checkpoint_needed){d->type=TRAINING_DECISION_CHECKPOINT;d->approved=true;d->confidence=0.90f;d->modulation_factor=1.0f;snprintf(d->reason,TRAINING_LOGIC_MAX_REASON_LENGTH,"Checkpoint: %u steps",b->conditions.steps_since_checkpoint);}
    else if(d->safe_to_increase_lr&&is<0.1f){d->type=TRAINING_DECISION_INCREASE_LR;d->approved=true;d->confidence=0.85f;float bo=b->config.lr_increase_factor;if(is>0.01f)bo=1.0f+(bo-1.0f)*(1.0f-is*10.0f);d->modulation_factor=bo;snprintf(d->reason,TRAINING_LOGIC_MAX_REASON_LENGTH,"Increase LR: stable %u steps (is=%.3f)",b->conditions.stable_step_count,is);b->stats.lr_increases++;}
    else if(d->batch_size_ok&&!ib){d->type=TRAINING_DECISION_DECREASE_BATCH;d->modulation_factor=r.batch_scale;b->stats.batch_decreases++;d->approved=true;d->confidence=0.75f;snprintf(d->reason,TRAINING_LOGIC_MAX_REASON_LENGTH,"Batch decrease: mem=%.2f tput=%.2f",b->conditions.memory_usage,b->conditions.throughput);}
    else if(d->stability_check_passed){d->type=TRAINING_DECISION_CONTINUE;d->approved=true;d->confidence=0.80f;d->modulation_factor=1.0f;snprintf(d->reason,TRAINING_LOGIC_MAX_REASON_LENGTH,"Stable (iscore=%.3f)",is);}
    else{d->type=TRAINING_DECISION_CONTINUE;d->approved=true;d->confidence=0.60f;d->modulation_factor=1.0f;snprintf(d->reason,TRAINING_LOGIC_MAX_REASON_LENGTH,"Monitoring (iscore=%.3f)",is);}
    uint64_t t1=get_time_us();d->evaluation_time_us=t1-t0;b->stats.total_decisions++;b->stats.decisions_by_type[d->type]++;b->stats.total_decision_time_us+=d->evaluation_time_us;b->stats.last_decision_time_ms=get_time_ms();
    if(b->stats.total_decisions>0)b->stats.avg_decision_time_us=(float)b->stats.total_decision_time_us/b->stats.total_decisions;
    if(d->evaluation_time_us>(uint64_t)b->stats.max_decision_time_us)b->stats.max_decision_time_us=(float)d->evaluation_time_us;
    nimcp_mutex_unlock(b->base.mutex); return NIMCP_SUCCESS;
}

float training_logic_get_lr_modulation(const training_logic_bridge_t* b,float base_lr) {
    if(!b)return base_lr; float f=b->last_lr_factor; float is=b->conditions.instability.instability_score;
    if(is>0.01f){float cs=expf(-3.0f*is);f=fminf(f,cs);} float ml=base_lr*f;
    if(ml<b->config.min_learning_rate)ml=b->config.min_learning_rate; if(ml>b->config.max_learning_rate)ml=b->config.max_learning_rate; return ml;
}

uint32_t training_logic_get_batch_size_modulation(const training_logic_bridge_t* b,uint32_t bs){if(!b)return bs;uint32_t m=(b->last_batch_size>0)?b->last_batch_size:bs;if(m<b->config.min_batch_size)m=b->config.min_batch_size;if(m>b->config.max_batch_size)m=b->config.max_batch_size;return m;}

int training_logic_apply_decision(training_logic_bridge_t* b,const training_logic_decision_t* d) {
    NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");NIMCP_CHECK_THROW(d!=NULL,NIMCP_ERROR_NULL_POINTER,"decision is NULL");
    nimcp_mutex_lock(b->base.mutex); if(b->config.mode!=TRAINING_LOGIC_MODE_AUTOMATIC){nimcp_mutex_unlock(b->base.mutex);return NIMCP_SUCCESS;}
    switch(d->type){case TRAINING_DECISION_INCREASE_LR:case TRAINING_DECISION_DECREASE_LR:b->last_lr_factor=d->modulation_factor;break;case TRAINING_DECISION_INCREASE_BATCH:case TRAINING_DECISION_DECREASE_BATCH:b->last_batch_size=(uint32_t)(b->last_batch_size*d->modulation_factor);break;case TRAINING_DECISION_PAUSE:b->stats.currently_paused=true;break;case TRAINING_DECISION_RESUME:b->stats.currently_paused=false;break;default:break;}
    nimcp_mutex_unlock(b->base.mutex); return NIMCP_SUCCESS;
}

int training_logic_update_conditions(training_logic_bridge_t* b){NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");nimcp_mutex_lock(b->base.mutex);int r=update_conditions_internal(b);nimcp_mutex_unlock(b->base.mutex);return r;}
int training_logic_get_conditions(const training_logic_bridge_t* b,training_logic_conditions_t* c){NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");NIMCP_CHECK_THROW(c!=NULL,NIMCP_ERROR_NULL_POINTER,"conditions is NULL");memcpy(c,&b->conditions,sizeof(*c));return NIMCP_SUCCESS;}

int training_logic_set_condition(training_logic_bridge_t* b,training_logic_condition_t c,bool v) {
    NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");NIMCP_CHECK_THROW(c<TRAINING_COND_COUNT,NIMCP_ERROR_INVALID_PARAM,"out of range");
    nimcp_mutex_lock(b->base.mutex);
    switch(c){case TRAINING_COND_LOSS_STABLE:b->conditions.loss_stable=v;break;case TRAINING_COND_GRAD_STABLE:b->conditions.grad_stable=v;break;case TRAINING_COND_LR_REASONABLE:b->conditions.lr_reasonable=v;break;case TRAINING_COND_MEMORY_OK:b->conditions.memory_ok=v;break;case TRAINING_COND_THROUGHPUT_OK:b->conditions.throughput_ok=v;break;case TRAINING_COND_NOT_MID_BATCH:b->conditions.not_mid_batch=v;break;case TRAINING_COND_SUFFICIENT_PROGRESS:b->conditions.sufficient_progress=v;break;case TRAINING_COND_GRAD_EXPLODING:b->conditions.grad_exploding=v;break;case TRAINING_COND_LOSS_NAN:b->conditions.loss_nan=v;break;case TRAINING_COND_DIVERGING:b->conditions.diverging=v;break;case TRAINING_COND_STABLE_FOR_N_STEPS:b->conditions.stable_for_n_steps=v;break;case TRAINING_COND_IMMUNE_OK:b->conditions.immune_ok=v;break;case TRAINING_COND_RESOURCE_OK:b->conditions.resource_ok=v;break;case TRAINING_COND_SWARM_CONSENSUS:b->conditions.swarm_consensus=v;break;case TRAINING_COND_PERCEPTION_QUALITY:b->conditions.perception_quality=v;break;case TRAINING_COND_CORTICAL_STABLE:b->conditions.cortical_stable=v;break;case TRAINING_COND_PREDICTIONS_OK:b->conditions.predictions_ok=v;break;default:nimcp_mutex_unlock(b->base.mutex);return NIMCP_ERROR_INVALID_PARAM;}
    nimcp_mutex_unlock(b->base.mutex); return NIMCP_SUCCESS;
}

int training_logic_set_numeric_condition(training_logic_bridge_t* b,const char* n,float v) {
    NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");NIMCP_CHECK_THROW(n!=NULL,NIMCP_ERROR_NULL_POINTER,"name is NULL");
    nimcp_mutex_lock(b->base.mutex);
    if(!strcmp(n,"loss"))b->conditions.loss_current=v;else if(!strcmp(n,"grad_norm"))b->conditions.grad_norm=v;else if(!strcmp(n,"learning_rate"))b->conditions.learning_rate=v;else if(!strcmp(n,"memory_usage"))b->conditions.memory_usage=v;else if(!strcmp(n,"throughput"))b->conditions.throughput=v;else if(!strcmp(n,"loss_trend"))b->conditions.loss_trend=v;else{nimcp_mutex_unlock(b->base.mutex);return NIMCP_ERROR_INVALID_PARAM;}
    nimcp_mutex_unlock(b->base.mutex); return NIMCP_SUCCESS;
}

int training_logic_add_custom_gate(training_logic_bridge_t* b,const char* e,uint32_t* gid) {
    NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");NIMCP_CHECK_THROW(e!=NULL,NIMCP_ERROR_NULL_POINTER,"expression is NULL");NIMCP_CHECK_THROW(gid!=NULL,NIMCP_ERROR_NULL_POINTER,"gate_id is NULL");
    if(b->stats.custom_gate_count>=TRAINING_LOGIC_MAX_CUSTOM_GATES)return NIMCP_ERROR_INVALID_STATE;
    nimcp_mutex_lock(b->base.mutex); logic_gate_type_t gt=LOGIC_GATE_AND;
    if(strstr(e,"AND"))gt=LOGIC_GATE_AND;else if(strstr(e,"OR"))gt=LOGIC_GATE_OR;else if(strstr(e,"NOT"))gt=LOGIC_GATE_NOT;else if(strstr(e,"XOR"))gt=LOGIC_GATE_XOR;else if(strstr(e,"IMPLIES"))gt=LOGIC_GATE_IMPLIES;else{nimcp_mutex_unlock(b->base.mutex);return NIMCP_ERROR_INVALID_PARAM;}
    uint32_t ng=neural_logic_create_gate(b->logic_network,gt,0.7f);if(ng==UINT32_MAX){nimcp_mutex_unlock(b->base.mutex);return NIMCP_ERROR_OPERATION_FAILED;}
    *gid=b->next_custom_gate_id++;b->stats.custom_gate_count++;nimcp_mutex_unlock(b->base.mutex);return NIMCP_SUCCESS;
}

bool training_logic_evaluate_gate(training_logic_bridge_t* b,uint32_t gid){if(!b||!b->logic_network){NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,"NULL");return false;}nimcp_mutex_lock(b->base.mutex);if(!b->config.disable_auto_update)update_conditions_internal(b);float in[8]={b->conditions.loss_stable?1:0,b->conditions.grad_stable?1:0,b->conditions.lr_reasonable?1:0,b->conditions.memory_ok?1:0,b->conditions.grad_exploding?1:0,b->conditions.loss_nan?1:0,b->conditions.diverging?1:0,b->conditions.immune_ok?1:0};float o=0;bool s=neural_logic_evaluate(b->logic_network,gid,in,8,&o);nimcp_mutex_unlock(b->base.mutex);return s&&(o>=b->config.confidence_threshold);}

int training_logic_get_gate_decision(training_logic_bridge_t* b,uint32_t gid,training_logic_decision_t* d){NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");NIMCP_CHECK_THROW(d!=NULL,NIMCP_ERROR_NULL_POINTER,"decision is NULL");nimcp_mutex_lock(b->base.mutex);if(!b->config.disable_auto_update)update_conditions_internal(b);float in[8]={b->conditions.loss_stable?1:0,b->conditions.grad_stable?1:0,b->conditions.lr_reasonable?1:0,b->conditions.memory_ok?1:0,b->conditions.grad_exploding?1:0,b->conditions.loss_nan?1:0,b->conditions.diverging?1:0,b->conditions.immune_ok?1:0};float o=0;uint64_t t0=get_time_us();bool s=neural_logic_evaluate(b->logic_network,gid,in,8,&o);uint64_t t1=get_time_us();memset(d,0,sizeof(*d));d->type=TRAINING_DECISION_CONTINUE;d->approved=s&&(o>=b->config.confidence_threshold);d->confidence=o;d->modulation_factor=1.0f;d->evaluation_time_us=t1-t0;snprintf(d->reason,TRAINING_LOGIC_MAX_REASON_LENGTH,"Gate %u: %.2f (thresh %.2f)",gid,o,b->config.confidence_threshold);nimcp_mutex_unlock(b->base.mutex);return NIMCP_SUCCESS;}

int training_logic_connect_bio_async(training_logic_bridge_t* b){NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");if(b->base.bio_async_enabled)return NIMCP_SUCCESS;bio_module_info_t i={.module_id=BIO_MODULE_TRAINING_LOGIC,.module_name="training_logic_bridge",.inbox_capacity=32,.user_data=b};b->base.bio_ctx=bio_router_register_module(&i);if(b->base.bio_ctx){b->base.bio_async_enabled=true;return NIMCP_SUCCESS;}return NIMCP_ERROR_OPERATION_FAILED;}
int training_logic_disconnect_bio_async(training_logic_bridge_t* b){NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");if(!b->base.bio_async_enabled)return NIMCP_SUCCESS;if(b->base.bio_ctx){bio_router_unregister_module(b->base.bio_ctx);b->base.bio_ctx=NULL;}b->base.bio_async_enabled=false;return NIMCP_SUCCESS;}
bool training_logic_is_bio_async_connected(const training_logic_bridge_t* b){return b?b->base.bio_async_enabled:false;}
int training_logic_process_inbox(training_logic_bridge_t* b){NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");if(!b->base.bio_async_enabled||!b->base.bio_ctx)return 0;return bio_router_process_inbox(b->base.bio_ctx,10);}
int training_logic_broadcast_decision(training_logic_bridge_t* b,const training_logic_decision_t* d){NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");NIMCP_CHECK_THROW(d!=NULL,NIMCP_ERROR_NULL_POINTER,"decision is NULL");if(!b->base.bio_async_enabled||!b->base.bio_ctx)return NIMCP_ERROR_INVALID_STATE;bio_msg_logic_gate_result_t msg={0};bio_msg_init_header(&msg.header,BIO_MSG_LOGIC_GATE_RESULT,bio_module_context_get_id(b->base.bio_ctx),0,sizeof(msg));msg.header.flags|=BIO_MSG_FLAG_BROADCAST;msg.output=d->approved?1.0f:0.0f;msg.spiked=d->approved;msg.spike_time_us=d->evaluation_time_us;msg.threshold_used=d->confidence;bio_router_broadcast(b->base.bio_ctx,&msg,sizeof(msg));return NIMCP_SUCCESS;}

int training_logic_get_stats(const training_logic_bridge_t* b,training_logic_stats_t* s){NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");NIMCP_CHECK_THROW(s!=NULL,NIMCP_ERROR_NULL_POINTER,"stats is NULL");memcpy(s,&b->stats,sizeof(*s));return NIMCP_SUCCESS;}
int training_logic_reset_stats(training_logic_bridge_t* b){NIMCP_CHECK_THROW(b!=NULL,NIMCP_ERROR_NULL_POINTER,"bridge is NULL");nimcp_mutex_lock(b->base.mutex);training_logic_mode_t mo=b->stats.current_mode;uint32_t cg=b->stats.custom_gate_count;memset(&b->stats,0,sizeof(b->stats));b->stats.current_mode=mo;b->stats.custom_gate_count=cg;nimcp_mutex_unlock(b->base.mutex);return NIMCP_SUCCESS;}

const char* training_logic_condition_to_string(training_logic_condition_t c){switch(c){case TRAINING_COND_LOSS_STABLE:return"loss_stable";case TRAINING_COND_GRAD_STABLE:return"grad_stable";case TRAINING_COND_LR_REASONABLE:return"lr_reasonable";case TRAINING_COND_MEMORY_OK:return"memory_ok";case TRAINING_COND_THROUGHPUT_OK:return"throughput_ok";case TRAINING_COND_NOT_MID_BATCH:return"not_mid_batch";case TRAINING_COND_SUFFICIENT_PROGRESS:return"sufficient_progress";case TRAINING_COND_GRAD_EXPLODING:return"grad_exploding";case TRAINING_COND_LOSS_NAN:return"loss_nan";case TRAINING_COND_DIVERGING:return"diverging";case TRAINING_COND_STABLE_FOR_N_STEPS:return"stable_for_n_steps";case TRAINING_COND_IMMUNE_OK:return"immune_ok";case TRAINING_COND_RESOURCE_OK:return"resource_ok";case TRAINING_COND_SWARM_CONSENSUS:return"swarm_consensus";case TRAINING_COND_PERCEPTION_QUALITY:return"perception_quality";case TRAINING_COND_CORTICAL_STABLE:return"cortical_stable";case TRAINING_COND_PREDICTIONS_OK:return"predictions_ok";default:return"unknown";}}
const char* training_logic_decision_type_to_string(training_logic_decision_type_t t){switch(t){case TRAINING_DECISION_CONTINUE:return"continue";case TRAINING_DECISION_PAUSE:return"pause";case TRAINING_DECISION_RESUME:return"resume";case TRAINING_DECISION_INCREASE_LR:return"increase_lr";case TRAINING_DECISION_DECREASE_LR:return"decrease_lr";case TRAINING_DECISION_INCREASE_BATCH:return"increase_batch";case TRAINING_DECISION_DECREASE_BATCH:return"decrease_batch";case TRAINING_DECISION_CHECKPOINT:return"checkpoint";case TRAINING_DECISION_ROLLBACK:return"rollback";case TRAINING_DECISION_TERMINATE:return"terminate";default:return"unknown";}}
const char* training_logic_mode_to_string(training_logic_mode_t m){switch(m){case TRAINING_LOGIC_MODE_DISABLED:return"disabled";case TRAINING_LOGIC_MODE_MONITOR_ONLY:return"monitor_only";case TRAINING_LOGIC_MODE_ADVISORY:return"advisory";case TRAINING_LOGIC_MODE_AUTOMATIC:return"automatic";case TRAINING_LOGIC_MODE_CONSENSUS_REQUIRED:return"consensus_required";default:return"unknown";}}
void training_logic_dump_state(const training_logic_bridge_t* b){if(!b)return;NIMCP_LOGGING_INFO("=== Training-Logic Bridge ===");NIMCP_LOGGING_INFO("Mode: %s Instability: %.3f (%s)",training_logic_mode_to_string(b->stats.current_mode),b->conditions.instability.instability_score,training_logic_instability_to_string(b->conditions.instability.derived_label));NIMCP_LOGGING_INFO("Conditions: loss_s=%d grad_s=%d lr_r=%d grad_e=%d loss_nan=%d div=%d",b->conditions.loss_stable,b->conditions.grad_stable,b->conditions.lr_reasonable,b->conditions.grad_exploding,b->conditions.loss_nan,b->conditions.diverging);NIMCP_LOGGING_INFO("Metrics: loss=%.6f grad=%.6f lr=%.6e step=%lu",b->conditions.loss_current,b->conditions.grad_norm,b->conditions.learning_rate,b->conditions.current_step);NIMCP_LOGGING_INFO("Stats: dec=%lu stab=%lu/%lu int=%lu lr+=%lu lr-=%lu",b->stats.total_decisions,b->stats.stability_passed,b->stats.stability_checks,b->stats.intervention_triggers,b->stats.lr_increases,b->stats.lr_decreases);}
