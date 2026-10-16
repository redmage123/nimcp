/**
 * @file nimcp_jni.c
 * @brief JNI bridge implementation for NIMCP Java bindings
 * @version 2.6.3
 *
 * Maps all Java native methods in com.nimcp.NIMCP to the NIMCP C API.
 * Uses JNI conventions: Java_com_nimcp_NIMCP_00024ClassName_methodName
 * where 00024 encodes the '$' separator for inner classes.
 */

#include <jni.h>
#include <nimcp.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

/* Internal headers for methods that require brain internals */
#include "api/nimcp_api_internal.h"  /* struct nimcp_brain_handle for brain->internal_brain deref */
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/accessors/nimcp_brain_accessors.h"
#include "perception/nimcp_audio_cortex.h"
#include "perception/nimcp_visual_cortex.h"
#include "perception/nimcp_speech_cortex.h"
#include "lnn/nimcp_lnn.h"
#include "lnn/nimcp_lnn_network.h"
#include "lnn/nimcp_lnn_training.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_types.h"
#include "training/nimcp_cnn_training.h"
#include "training/nimcp_cortex_cnn.h"
#include "cognitive/nimcp_sleep_wake.h"
#include "cognitive/training/nimcp_training_integration.h"
#include "core/brain/factory/init/nimcp_brain_init_medulla.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "edge/nimcp_edge.h"
#include "edge/nimcp_swarm_runtime.h"
#include "edge/nimcp_sensor.h"
#include "edge/nimcp_safety_watchdog.h"
#include "edge/nimcp_ros2_bridge.h"
#include "edge/nimcp_mavlink_bridge.h"
#include "memory/nimcp_memory_store.h"
#include "cognitive/nimcp_ood_detector.h"
#include "utils/time/nimcp_time.h"

// ============================================================================
// Helper Functions
// ============================================================================

static void throw_nimcp_exception(JNIEnv *env, int code, const char *msg) {
    const char *class_name;
    switch (code) {
        case 1003: class_name = "com/nimcp/NIMCP$NullArgException"; break;
        case 1004: class_name = "com/nimcp/NIMCP$InvalidException"; break;
        case 2000: class_name = "com/nimcp/NIMCP$MemoryException"; break;
        case 4000: class_name = "com/nimcp/NIMCP$IOExceptionNIMCP"; break;
        default:   class_name = "com/nimcp/NIMCP$NIMCPException"; break;
    }
    jclass cls = (*env)->FindClass(env, class_name);
    if (cls) {
        (*env)->ThrowNew(env, cls, msg ? msg : "NIMCP error");
    }
}

static int check_status(JNIEnv *env, nimcp_status_t s) {
    if (s == NIMCP_OK) return 0;
    const char *msg = nimcp_get_error();
    throw_nimcp_exception(env, (int)s, msg && msg[0] ? msg : "NIMCP error");
    return -1;
}

// ============================================================================
// Library Lifecycle
// ============================================================================

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_nativeInit(JNIEnv *env, jclass cls) {
    (void)env; (void)cls;
    return (jint)nimcp_init();
}

JNIEXPORT void JNICALL Java_com_nimcp_NIMCP_nativeShutdown(JNIEnv *env, jclass cls) {
    (void)env; (void)cls;
    nimcp_shutdown();
}

JNIEXPORT jstring JNICALL Java_com_nimcp_NIMCP_nativeVersion(JNIEnv *env, jclass cls) {
    (void)cls;
    const char *v = nimcp_version();
    return (*env)->NewStringUTF(env, v ? v : "unknown");
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_nativeVersionInt(JNIEnv *env, jclass cls) {
    (void)env; (void)cls;
    return (jint)nimcp_version_int();
}

// ============================================================================
// Brain - Core
// ============================================================================

JNIEXPORT jlong JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeCreate(
    JNIEnv *env, jclass cls, jstring name, jint size, jint task,
    jint numInputs, jint numOutputs)
{
    (void)cls;
    const char *c_name = (*env)->GetStringUTFChars(env, name, NULL);
    if (!c_name) return 0; /* OOM — JVM already threw OutOfMemoryError */
    nimcp_brain_t brain = nimcp_brain_create(
        c_name, (nimcp_brain_size_t)size, (nimcp_brain_task_t)task,
        (uint32_t)numInputs, (uint32_t)numOutputs);
    (*env)->ReleaseStringUTFChars(env, name, c_name);
    return (jlong)(uintptr_t)brain;
}

JNIEXPORT void JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeDestroy(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)env; (void)cls;
    if (h) nimcp_brain_destroy((nimcp_brain_t)(uintptr_t)h);
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeLearn(
    JNIEnv *env, jclass cls, jlong h, jfloatArray features,
    jstring label, jfloat confidence)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    jint len = (*env)->GetArrayLength(env, features);
    jfloat *feats = (*env)->GetFloatArrayElements(env, features, NULL);
    if (!feats) return (jint)NIMCP_OK; /* OOM — JVM already threw */
    const char *c_label = (*env)->GetStringUTFChars(env, label, NULL);
    if (!c_label) {
        (*env)->ReleaseFloatArrayElements(env, features, feats, JNI_ABORT);
        return (jint)NIMCP_OK; /* OOM — JVM already threw */
    }

    nimcp_status_t rc = nimcp_brain_learn_example(
        brain, feats, (uint32_t)len, c_label, confidence);

    (*env)->ReleaseStringUTFChars(env, label, c_label);
    (*env)->ReleaseFloatArrayElements(env, features, feats, JNI_ABORT);
    return (jint)rc;
}

JNIEXPORT jstring JNICALL Java_com_nimcp_NIMCP_00024Brain_nativePredict(
    JNIEnv *env, jclass cls, jlong h, jfloatArray features,
    jfloatArray outConf)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    jint len = (*env)->GetArrayLength(env, features);
    jfloat *feats = (*env)->GetFloatArrayElements(env, features, NULL);
    if (!feats) return NULL; /* OOM — JVM already threw */

    char label_buf[NIMCP_MAX_LABEL_SIZE];
    float conf = 0.0f;
    nimcp_status_t rc = nimcp_brain_predict(
        brain, feats, (uint32_t)len, label_buf, &conf);

    (*env)->ReleaseFloatArrayElements(env, features, feats, JNI_ABORT);

    if (rc != NIMCP_OK) return NULL;

    jfloat c = conf;
    (*env)->SetFloatArrayRegion(env, outConf, 0, 1, &c);
    return (*env)->NewStringUTF(env, label_buf);
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeInfer(
    JNIEnv *env, jclass cls, jlong h, jfloatArray features,
    jfloatArray outputs)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    jint flen = (*env)->GetArrayLength(env, features);
    jint olen = (*env)->GetArrayLength(env, outputs);
    jfloat *feats = (*env)->GetFloatArrayElements(env, features, NULL);
    if (!feats) return (jint)NIMCP_OK; /* OOM — JVM already threw */
    jfloat *outs = (*env)->GetFloatArrayElements(env, outputs, NULL);
    if (!outs) {
        (*env)->ReleaseFloatArrayElements(env, features, feats, JNI_ABORT);
        return (jint)NIMCP_OK; /* OOM — JVM already threw */
    }

    nimcp_status_t rc = nimcp_brain_infer(
        brain, feats, (uint32_t)flen, outs, (uint32_t)olen);

    (*env)->ReleaseFloatArrayElements(env, features, feats, JNI_ABORT);
    (*env)->ReleaseFloatArrayElements(env, outputs, outs, 0); // copy back
    return (jint)rc;
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSave(
    JNIEnv *env, jclass cls, jlong h, jstring path)
{
    (void)cls;
    const char *c_path = (*env)->GetStringUTFChars(env, path, NULL);
    if (!c_path) return (jint)NIMCP_OK; /* OOM — JVM already threw */
    nimcp_status_t rc = nimcp_brain_save(
        (nimcp_brain_t)(uintptr_t)h, c_path);
    (*env)->ReleaseStringUTFChars(env, path, c_path);
    return (jint)rc;
}

JNIEXPORT jlong JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeLoad(
    JNIEnv *env, jclass cls, jstring path)
{
    (void)cls;
    const char *c_path = (*env)->GetStringUTFChars(env, path, NULL);
    if (!c_path) return 0; /* OOM — JVM already threw */
    nimcp_brain_t brain = nimcp_brain_load(c_path);
    (*env)->ReleaseStringUTFChars(env, path, c_path);
    return (jlong)(uintptr_t)brain;
}

JNIEXPORT jlong JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeCreateFromConfig(
    JNIEnv *env, jclass cls, jstring path)
{
    (void)cls;
    const char *c_path = (*env)->GetStringUTFChars(env, path, NULL);
    if (!c_path) return 0; /* OOM — JVM already threw */
    nimcp_brain_t brain = nimcp_brain_create_from_config(c_path);
    (*env)->ReleaseStringUTFChars(env, path, c_path);
    return (jlong)(uintptr_t)brain;
}

// ============================================================================
// Brain - Training Pipeline
// ============================================================================

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeConfigureTraining(
    JNIEnv *env, jclass cls, jlong h,
    jint loss, jint opt, jint sched,
    jfloat lr, jfloat wd, jfloat mom, jfloat b1, jfloat b2, jfloat eps,
    jint schedStep, jfloat schedGamma, jint warmup,
    jboolean gradClip, jfloat gradClipVal,
    jboolean bioMod, jfloat bioBlend,
    jint netType, jint snnMethod, jfloat snnEligTau, jfloat snnRewTau,
    jfloat snnSurrBeta, jint lnnMethod, jint lnnBptt, jboolean lnnAdjCheck)
{
    (void)cls;
    nimcp_training_config_t cfg = nimcp_training_config_default();
    cfg.loss_type      = (nimcp_api_loss_t)loss;
    cfg.optimizer_type = (nimcp_api_optimizer_t)opt;
    cfg.scheduler_type = (nimcp_api_scheduler_t)sched;
    cfg.learning_rate  = lr;
    cfg.weight_decay   = wd;
    cfg.momentum       = mom;
    cfg.beta1          = b1;
    cfg.beta2          = b2;
    cfg.epsilon        = eps;
    cfg.scheduler_step_size = (uint32_t)schedStep;
    cfg.scheduler_gamma     = schedGamma;
    cfg.warmup_steps        = (uint32_t)warmup;
    cfg.enable_gradient_clipping  = gradClip;
    cfg.gradient_clip_value       = gradClipVal;
    cfg.enable_biological_modulation = bioMod;
    cfg.biological_blend             = bioBlend;
    cfg.network_type    = (nimcp_network_type_t)netType;
    cfg.snn_method      = (nimcp_snn_train_method_t)snnMethod;
    cfg.snn_eligibility_tau = snnEligTau;
    cfg.snn_reward_tau      = snnRewTau;
    cfg.snn_surrogate_beta  = snnSurrBeta;
    cfg.lnn_method          = (nimcp_lnn_train_method_t)lnnMethod;
    cfg.lnn_bptt_truncation = (uint32_t)lnnBptt;
    cfg.lnn_use_adjoint_checkpointing = lnnAdjCheck;

    return (jint)nimcp_brain_configure_training(
        (nimcp_brain_t)(uintptr_t)h, &cfg);
}

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeTrainStep(
    JNIEnv *env, jclass cls, jlong h, jfloatArray features,
    jfloatArray targets)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    jint flen = (*env)->GetArrayLength(env, features);
    jint tlen = (*env)->GetArrayLength(env, targets);
    jfloat *feats = (*env)->GetFloatArrayElements(env, features, NULL);
    if (!feats) return NULL; /* OOM — JVM already threw */
    jfloat *tgts = (*env)->GetFloatArrayElements(env, targets, NULL);
    if (!tgts) {
        (*env)->ReleaseFloatArrayElements(env, features, feats, JNI_ABORT);
        return NULL; /* OOM — JVM already threw */
    }

    nimcp_training_result_t result = {0};
    nimcp_status_t rc = nimcp_brain_train_step(
        brain, feats, (uint32_t)flen, tgts, (uint32_t)tlen, &result);

    (*env)->ReleaseFloatArrayElements(env, features, feats, JNI_ABORT);
    (*env)->ReleaseFloatArrayElements(env, targets, tgts, JNI_ABORT);

    if (rc != NIMCP_OK) return NULL;

    jfloatArray out = (*env)->NewFloatArray(env, 5);
    jfloat vals[5] = { result.loss, result.learning_rate,
                       (float)result.step, result.early_stopped ? 1.0f : 0.0f,
                       result.gradient_norm };
    (*env)->SetFloatArrayRegion(env, out, 0, 5, vals);
    return out;
}

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeTrainBatch(
    JNIEnv *env, jclass cls, jlong h, jfloatArray features,
    jfloatArray targets, jint batchSize, jint numFeatures, jint numTargets)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    jfloat *feats = (*env)->GetFloatArrayElements(env, features, NULL);
    if (!feats) return NULL; /* OOM — JVM already threw */
    jfloat *tgts = (*env)->GetFloatArrayElements(env, targets, NULL);
    if (!tgts) {
        (*env)->ReleaseFloatArrayElements(env, features, feats, JNI_ABORT);
        return NULL; /* OOM — JVM already threw */
    }

    nimcp_training_result_t result = {0};
    nimcp_status_t rc = nimcp_brain_train_batch(
        brain, feats, tgts, (uint32_t)batchSize,
        (uint32_t)numFeatures, (uint32_t)numTargets, &result);

    (*env)->ReleaseFloatArrayElements(env, features, feats, JNI_ABORT);
    (*env)->ReleaseFloatArrayElements(env, targets, tgts, JNI_ABORT);

    if (rc != NIMCP_OK) return NULL;

    jfloatArray out = (*env)->NewFloatArray(env, 5);
    jfloat vals[5] = { result.loss, result.learning_rate,
                       (float)result.step, result.early_stopped ? 1.0f : 0.0f,
                       result.gradient_norm };
    (*env)->SetFloatArrayRegion(env, out, 0, 5, vals);
    return out;
}

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeGetTrainingStats(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)cls;
    uint64_t steps = 0; float loss = 0, lr = 0;
    nimcp_status_t rc = nimcp_brain_get_training_stats(
        (nimcp_brain_t)(uintptr_t)h, &steps, &loss, &lr);
    if (rc != NIMCP_OK) return NULL;

    jfloatArray out = (*env)->NewFloatArray(env, 3);
    jfloat vals[3] = { (float)steps, loss, lr };
    (*env)->SetFloatArrayRegion(env, out, 0, 3, vals);
    return out;
}

JNIEXPORT jfloat JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeStepScheduler(
    JNIEnv *env, jclass cls, jlong h, jfloat valMetric)
{
    (void)env; (void)cls;
    return nimcp_brain_step_scheduler(
        (nimcp_brain_t)(uintptr_t)h, valMetric);
}

// ============================================================================
// Brain - Callbacks
// ============================================================================

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeEnableCallbacks(
    JNIEnv *env, jclass cls, jlong h, jboolean autoCP, jint cpInterval,
    jboolean earlyStop, jint patience, jfloat minDelta,
    jfloat divThresh, jint logInterval)
{
    (void)cls;
    nimcp_callback_config_t cfg = nimcp_callback_config_default();
    cfg.enable_auto_checkpoint = autoCP;
    cfg.checkpoint_interval = (uint32_t)cpInterval;
    cfg.enable_early_stopping = earlyStop;
    cfg.patience = (uint32_t)patience;
    cfg.min_delta = minDelta;
    cfg.divergence_threshold = divThresh;
    cfg.log_interval = (uint32_t)logInterval;
    return (jint)nimcp_brain_enable_callbacks(
        (nimcp_brain_t)(uintptr_t)h, &cfg);
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeDisableCallbacks(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)env; (void)cls;
    return (jint)nimcp_brain_disable_callbacks(
        (nimcp_brain_t)(uintptr_t)h);
}

// Callback trampoline data
typedef struct {
    JavaVM *jvm;
    jobject callback_ref; // Global ref to TrainingCallback
} jni_callback_data_t;

static nimcp_callback_action_t jni_callback_trampoline(
    nimcp_callback_event_t event,
    const nimcp_callback_metrics_t *metrics,
    void *user_data)
{
    jni_callback_data_t *data = (jni_callback_data_t *)user_data;
    if (!data) return NIMCP_CB_ACTION_CONTINUE;

    JNIEnv *env = NULL;
    int attached = 0;
    if ((*data->jvm)->GetEnv(data->jvm, (void **)&env, JNI_VERSION_1_8) != JNI_OK) {
        if ((*data->jvm)->AttachCurrentThread(data->jvm, (void **)&env, NULL) != JNI_OK)
            return NIMCP_CB_ACTION_CONTINUE;
        attached = 1;
    }

    // Build CallbackMetrics object
    jclass metricsCls = (*env)->FindClass(env, "com/nimcp/NIMCP$CallbackMetrics");
    if (!metricsCls) goto cleanup;
    jobject metricsObj = (*env)->AllocObject(env, metricsCls);
    if (!metricsObj) goto cleanup;

    // Set fields
    (*env)->SetLongField(env, metricsObj,
        (*env)->GetFieldID(env, metricsCls, "step", "J"), (jlong)metrics->step);
    (*env)->SetLongField(env, metricsObj,
        (*env)->GetFieldID(env, metricsCls, "epoch", "J"), (jlong)metrics->epoch);
    (*env)->SetFloatField(env, metricsObj,
        (*env)->GetFieldID(env, metricsCls, "loss", "F"), metrics->loss);
    (*env)->SetFloatField(env, metricsObj,
        (*env)->GetFieldID(env, metricsCls, "lossEma", "F"), metrics->loss_ema);
    (*env)->SetFloatField(env, metricsObj,
        (*env)->GetFieldID(env, metricsCls, "learningRate", "F"), metrics->learning_rate);
    (*env)->SetFloatField(env, metricsObj,
        (*env)->GetFieldID(env, metricsCls, "gradientNorm", "F"), metrics->gradient_norm);
    (*env)->SetLongField(env, metricsObj,
        (*env)->GetFieldID(env, metricsCls, "stepTimeUs", "J"), (jlong)metrics->step_time_us);
    (*env)->SetBooleanField(env, metricsObj,
        (*env)->GetFieldID(env, metricsCls, "isConverging", "Z"), metrics->is_converging);
    (*env)->SetBooleanField(env, metricsObj,
        (*env)->GetFieldID(env, metricsCls, "isDiverging", "Z"), metrics->is_diverging);

    // Build CallbackEvent enum value
    jclass eventCls = (*env)->FindClass(env, "com/nimcp/NIMCP$CallbackEvent");
    if (!eventCls) goto cleanup;
    jmethodID fromInt = (*env)->GetStaticMethodID(env, eventCls, "fromInt",
        "(I)Lcom/nimcp/NIMCP$CallbackEvent;");
    if (!fromInt) goto cleanup;
    jobject eventObj = (*env)->CallStaticObjectMethod(env, eventCls, fromInt, (jint)event);

    // Call the callback
    jclass cbCls = (*env)->GetObjectClass(env, data->callback_ref);
    jmethodID onEvent = (*env)->GetMethodID(env, cbCls, "onEvent",
        "(Lcom/nimcp/NIMCP$CallbackEvent;Lcom/nimcp/NIMCP$CallbackMetrics;)"
        "Lcom/nimcp/NIMCP$CallbackAction;");
    if (!onEvent) goto cleanup;

    jobject actionObj = (*env)->CallObjectMethod(env, data->callback_ref,
                                                 onEvent, eventObj, metricsObj);
    if (!actionObj) goto cleanup;

    jclass actionCls = (*env)->GetObjectClass(env, actionObj);
    jfieldID valField = (*env)->GetFieldID(env, actionCls, "value", "I");
    jint actionVal = (*env)->GetIntField(env, actionObj, valField);

    if (attached) (*data->jvm)->DetachCurrentThread(data->jvm);
    return (nimcp_callback_action_t)actionVal;

cleanup:
    if (attached) (*data->jvm)->DetachCurrentThread(data->jvm);
    return NIMCP_CB_ACTION_CONTINUE;
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeRegisterCallback(
    JNIEnv *env, jclass cls, jlong h, jint event,
    jobject callback, jstring name)
{
    (void)cls;
    // Allocate trampoline data
    jni_callback_data_t *data = (jni_callback_data_t *)nimcp_malloc(sizeof(jni_callback_data_t));
    if (!data) return 0;
    memset(data, 0, sizeof(jni_callback_data_t));
    (*env)->GetJavaVM(env, &data->jvm);
    data->callback_ref = (*env)->NewGlobalRef(env, callback);
    if (!data->callback_ref) {
        nimcp_free(data);
        return 0;
    }

    const char *c_name = name ? (*env)->GetStringUTFChars(env, name, NULL) : NULL;
    uint32_t id = nimcp_brain_register_callback(
        (nimcp_brain_t)(uintptr_t)h, (nimcp_callback_event_t)event,
        jni_callback_trampoline, data, c_name);
    if (c_name) (*env)->ReleaseStringUTFChars(env, name, c_name);

    if (id == 0) {
        (*env)->DeleteGlobalRef(env, data->callback_ref);
        nimcp_free(data);
    }
    return (jint)id;
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeUnregisterCallback(
    JNIEnv *env, jclass cls, jlong h, jint cbId)
{
    (void)env; (void)cls;
    return (jint)nimcp_brain_unregister_callback(
        (nimcp_brain_t)(uintptr_t)h, (uint32_t)cbId);
}

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeGetCallbackStats(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)cls;
    uint64_t fired = 0; float avg_time = 0; uint32_t early = 0;
    nimcp_status_t rc = nimcp_brain_get_callback_stats(
        (nimcp_brain_t)(uintptr_t)h, &fired, &avg_time, &early);
    if (rc != NIMCP_OK) return NULL;

    jfloatArray out = (*env)->NewFloatArray(env, 3);
    jfloat vals[3] = { (float)fired, avg_time, (float)early };
    (*env)->SetFloatArrayRegion(env, out, 0, 3, vals);
    return out;
}

// ============================================================================
// Brain - Resize
// ============================================================================

JNIEXPORT jboolean JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeResize(
    JNIEnv *env, jclass cls, jlong h, jint count)
{
    (void)env; (void)cls;
    return nimcp_brain_resize(
        (nimcp_brain_t)(uintptr_t)h, (uint32_t)count);
}

JNIEXPORT jboolean JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeAutoResize(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)env; (void)cls;
    return nimcp_brain_auto_resize((nimcp_brain_t)(uintptr_t)h);
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeGetNeuronCount(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)env; (void)cls;
    return (jint)nimcp_brain_get_neuron_count(
        (nimcp_brain_t)(uintptr_t)h);
}

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeGetUtilizationMetrics(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)cls;
    float util = 0, sat = 0;
    bool ok = nimcp_brain_get_utilization_metrics(
        (nimcp_brain_t)(uintptr_t)h, &util, &sat);
    if (!ok) return NULL;

    jfloatArray out = (*env)->NewFloatArray(env, 2);
    jfloat vals[2] = { util, sat };
    (*env)->SetFloatArrayRegion(env, out, 0, 2, vals);
    return out;
}

// ============================================================================
// Brain - Named Snapshots
// ============================================================================

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSnapshotSave(
    JNIEnv *env, jclass cls, jlong h, jstring name, jstring desc)
{
    (void)cls;
    const char *c_name = (*env)->GetStringUTFChars(env, name, NULL);
    if (!c_name) return (jint)NIMCP_OK; /* OOM — JVM already threw */
    const char *c_desc = desc ? (*env)->GetStringUTFChars(env, desc, NULL) : NULL;
    /* c_desc NULL is OK when desc is NULL (optional param) */

    nimcp_status_t rc = nimcp_brain_snapshot_save(
        (nimcp_brain_t)(uintptr_t)h, c_name, c_desc);

    (*env)->ReleaseStringUTFChars(env, name, c_name);
    if (c_desc) (*env)->ReleaseStringUTFChars(env, desc, c_desc);
    return (jint)rc;
}

JNIEXPORT jlong JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSnapshotRestore(
    JNIEnv *env, jclass cls, jlong h, jstring name)
{
    (void)cls;
    const char *c_name = (*env)->GetStringUTFChars(env, name, NULL);
    if (!c_name) return 0; /* OOM — JVM already threw */
    nimcp_brain_t brain = nimcp_brain_snapshot_restore(
        (nimcp_brain_t)(uintptr_t)h, c_name);
    (*env)->ReleaseStringUTFChars(env, name, c_name);
    return (jlong)(uintptr_t)brain;
}

JNIEXPORT jobjectArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSnapshotList(
    JNIEnv *env, jclass cls, jlong h, jint maxCount)
{
    (void)cls;
    nimcp_brain_snapshot_info_t *infos = (nimcp_brain_snapshot_info_t *)
        calloc((size_t)maxCount, sizeof(nimcp_brain_snapshot_info_t));
    if (!infos) return NULL;

    uint32_t count = 0;
    nimcp_status_t rc = nimcp_brain_snapshot_list(
        (nimcp_brain_t)(uintptr_t)h, infos, (uint32_t)maxCount, &count);

    if (rc != NIMCP_OK || count == 0) {
        nimcp_free(infos);
        return NULL;
    }

    jclass strCls = (*env)->FindClass(env, "java/lang/String");
    jobjectArray arr = (*env)->NewObjectArray(env, (jsize)count, strCls, NULL);

    for (uint32_t i = 0; i < count; i++) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "%s\t%s\t%lu\t%u\t%d\t%d",
            infos[i].name, infos[i].description,
            (unsigned long)infos[i].timestamp, infos[i].file_size,
            infos[i].is_compressed ? 1 : 0, infos[i].is_encrypted ? 1 : 0);
        (*env)->SetObjectArrayElement(env, arr, (jsize)i,
            (*env)->NewStringUTF(env, buf));
    }

    nimcp_free(infos);
    return arr;
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSnapshotDelete(
    JNIEnv *env, jclass cls, jlong h, jstring name)
{
    (void)cls;
    const char *c_name = (*env)->GetStringUTFChars(env, name, NULL);
    if (!c_name) return (jint)NIMCP_OK; /* OOM — JVM already threw */
    nimcp_status_t rc = nimcp_brain_snapshot_delete(
        (nimcp_brain_t)(uintptr_t)h, c_name);
    (*env)->ReleaseStringUTFChars(env, name, c_name);
    return (jint)rc;
}

// ============================================================================
// Brain - COW
// ============================================================================

JNIEXPORT jlong JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeCloneCow(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)env; (void)cls;
    return (jlong)(uintptr_t)nimcp_brain_clone_cow(
        (nimcp_brain_t)(uintptr_t)h);
}

JNIEXPORT jlong JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSnapshotCow(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)env; (void)cls;
    return (jlong)(uintptr_t)nimcp_brain_snapshot_cow(
        (nimcp_brain_t)(uintptr_t)h);
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeRestoreCow(
    JNIEnv *env, jclass cls, jlong h, jlong snapH)
{
    (void)env; (void)cls;
    return (jint)nimcp_brain_restore_cow(
        (nimcp_brain_t)(uintptr_t)h,
        (nimcp_brain_snapshot_t)(uintptr_t)snapH);
}

JNIEXPORT void JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSnapshotCowDestroy(
    JNIEnv *env, jclass cls, jlong snapH)
{
    (void)env; (void)cls;
    if (snapH) nimcp_brain_snapshot_destroy(
        (nimcp_brain_snapshot_t)(uintptr_t)snapH);
}

// ============================================================================
// Brain - Working Memory
// ============================================================================

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeWorkingMemoryAdd(
    JNIEnv *env, jclass cls, jlong h, jfloatArray data, jfloat salience)
{
    (void)cls;
    jint len = (*env)->GetArrayLength(env, data);
    jfloat *arr = (*env)->GetFloatArrayElements(env, data, NULL);
    if (!arr) return (jint)NIMCP_OK; /* OOM — JVM already threw */
    nimcp_status_t rc = nimcp_brain_working_memory_add(
        (nimcp_brain_t)(uintptr_t)h, arr, (uint32_t)len, salience);
    (*env)->ReleaseFloatArrayElements(env, data, arr, JNI_ABORT);
    return (jint)rc;
}

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeWorkingMemoryGet(
    JNIEnv *env, jclass cls, jlong h, jint index)
{
    (void)cls;
    uint32_t size = 0;
    const float *item = nimcp_brain_working_memory_get(
        (nimcp_brain_t)(uintptr_t)h, (uint32_t)index, &size);
    if (!item || size == 0) return NULL;

    jfloatArray out = (*env)->NewFloatArray(env, (jsize)size);
    (*env)->SetFloatArrayRegion(env, out, 0, (jsize)size, item);
    return out;
}

JNIEXPORT jintArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeWorkingMemoryStats(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)cls;
    uint32_t cur = 0, cap = 0;
    nimcp_status_t rc = nimcp_brain_working_memory_stats(
        (nimcp_brain_t)(uintptr_t)h, &cur, &cap);
    if (rc != NIMCP_OK) return NULL;

    jintArray out = (*env)->NewIntArray(env, 2);
    jint vals[2] = { (jint)cur, (jint)cap };
    (*env)->SetIntArrayRegion(env, out, 0, 2, vals);
    return out;
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeWorkingMemoryRefresh(
    JNIEnv *env, jclass cls, jlong h, jint index)
{
    (void)env; (void)cls;
    return (jint)nimcp_brain_working_memory_refresh(
        (nimcp_brain_t)(uintptr_t)h, (uint32_t)index);
}

// ============================================================================
// Brain - Workspace
// ============================================================================

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeWorkspaceCompete(
    JNIEnv *env, jclass cls, jlong h, jint module,
    jfloatArray content, jfloat strength)
{
    (void)cls;
    jint len = (*env)->GetArrayLength(env, content);
    jfloat *arr = (*env)->GetFloatArrayElements(env, content, NULL);
    if (!arr) return (jint)NIMCP_OK; /* OOM — JVM already threw */
    nimcp_status_t rc = nimcp_brain_workspace_compete(
        (nimcp_brain_t)(uintptr_t)h,
        (nimcp_cognitive_module_t)module,
        arr, (uint32_t)len, strength);
    (*env)->ReleaseFloatArrayElements(env, content, arr, JNI_ABORT);
    return (jint)rc;
}

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeWorkspaceRead(
    JNIEnv *env, jclass cls, jlong h, jint maxDim, jintArray outMeta)
{
    (void)cls;
    float *buf = nimcp_calloc((size_t)maxDim, sizeof(float));
    if (!buf) return NULL;

    uint32_t actualDim = 0;
    nimcp_cognitive_module_t source = NIMCP_MODULE_NONE;
    nimcp_status_t rc = nimcp_brain_workspace_read(
        (nimcp_brain_t)(uintptr_t)h,
        buf, (uint32_t)maxDim, &actualDim, &source);

    if (rc != NIMCP_OK) { nimcp_free(buf); return NULL; }

    jfloatArray out = (*env)->NewFloatArray(env, (jsize)actualDim);
    (*env)->SetFloatArrayRegion(env, out, 0, (jsize)actualDim, buf);
    nimcp_free(buf);

    jint meta[2] = { (jint)actualDim, (jint)source };
    (*env)->SetIntArrayRegion(env, outMeta, 0, 2, meta);
    return out;
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeWorkspaceSubscribe(
    JNIEnv *env, jclass cls, jlong h, jint module)
{
    (void)env; (void)cls;
    return (jint)nimcp_brain_workspace_subscribe(
        (nimcp_brain_t)(uintptr_t)h,
        (nimcp_cognitive_module_t)module);
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeWorkspaceUnsubscribe(
    JNIEnv *env, jclass cls, jlong h, jint module)
{
    (void)env; (void)cls;
    return (jint)nimcp_brain_workspace_unsubscribe(
        (nimcp_brain_t)(uintptr_t)h,
        (nimcp_cognitive_module_t)module);
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeWorkspaceHasBroadcast(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)cls;
    bool has = false;
    nimcp_status_t rc = nimcp_brain_workspace_has_broadcast(
        (nimcp_brain_t)(uintptr_t)h, &has);
    if (rc != NIMCP_OK) return 0;
    return has ? 1 : 0;
}

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeWorkspaceStats(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)cls;
    uint32_t broadcasts = 0, competitions = 0;
    float avg = 0;
    nimcp_status_t rc = nimcp_brain_workspace_stats(
        (nimcp_brain_t)(uintptr_t)h, &broadcasts, &competitions, &avg);
    if (rc != NIMCP_OK) return NULL;

    jfloatArray out = (*env)->NewFloatArray(env, 3);
    jfloat vals[3] = { (float)broadcasts, (float)competitions, avg };
    (*env)->SetFloatArrayRegion(env, out, 0, 3, vals);
    return out;
}

// ============================================================================
// Brain - Oscillations
// ============================================================================

JNIEXPORT jboolean JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeEnableOscillations(
    JNIEnv *env, jclass cls, jlong h, jboolean en)
{
    (void)env; (void)cls;
    return nimcp_enable_complex_oscillations(
        (nimcp_brain_t)(uintptr_t)h, en);
}

JNIEXPORT jboolean JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeIsOscillationsEnabled(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)env; (void)cls;
    return nimcp_is_complex_oscillations_enabled(
        (nimcp_brain_t)(uintptr_t)h);
}

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeGetPhasor(
    JNIEnv *env, jclass cls, jlong h, jint neuronId)
{
    (void)cls;
    nimcp_oscillation_phasor_t p = nimcp_get_oscillation_phasor(
        (nimcp_brain_t)(uintptr_t)h, (uint32_t)neuronId);

    jfloatArray out = (*env)->NewFloatArray(env, 2);
    jfloat vals[2] = { p.amplitude, p.phase };
    (*env)->SetFloatArrayRegion(env, out, 0, 2, vals);
    return out;
}

JNIEXPORT jfloat JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeGetPhaseCoherence(
    JNIEnv *env, jclass cls, jlong h, jintArray neuronIds)
{
    (void)cls;
    jint len = (*env)->GetArrayLength(env, neuronIds);
    jint *ids = (*env)->GetIntArrayElements(env, neuronIds, NULL);
    if (!ids) return 0.0f;

    // Convert jint[] to uint32_t[]
    if (len <= 0 || (size_t)len > SIZE_MAX / sizeof(uint32_t)) {
        (*env)->ReleaseIntArrayElements(env, neuronIds, ids, JNI_ABORT);
        return 0.0f;
    }
    uint32_t *uids = nimcp_malloc((size_t)len * sizeof(uint32_t));
    if (!uids) {
        (*env)->ReleaseIntArrayElements(env, neuronIds, ids, JNI_ABORT);
        return 0.0f;
    }
    for (jint i = 0; i < len; i++) uids[i] = (uint32_t)ids[i];

    float result = nimcp_get_phase_coherence(
        (nimcp_brain_t)(uintptr_t)h, uids, (uint32_t)len);

    nimcp_free(uids);
    (*env)->ReleaseIntArrayElements(env, neuronIds, ids, JNI_ABORT);
    return result;
}

JNIEXPORT jfloat JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeGetPacModulation(
    JNIEnv *env, jclass cls, jlong h, jfloat theta, jfloat gamma)
{
    (void)env; (void)cls;
    return nimcp_get_pac_modulation(
        (nimcp_brain_t)(uintptr_t)h, theta, gamma);
}

// ============================================================================
// Brain - Probe
// ============================================================================

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeProbe(
    JNIEnv *env, jclass cls, jlong h, jobjectArray outStrings)
{
    (void)cls;
    nimcp_brain_probe_t probe = {0};
    nimcp_status_t rc = nimcp_brain_probe(
        (nimcp_brain_t)(uintptr_t)h, &probe);
    if (rc != NIMCP_OK) return NULL;

    // Set task name string
    (*env)->SetObjectArrayElement(env, outStrings, 0,
        (*env)->NewStringUTF(env, probe.task_name));

    // Pack numeric fields into float array
    jfloatArray out = (*env)->NewFloatArray(env, 18);
    jfloat vals[18] = {
        (float)probe.size, (float)probe.task,
        (float)probe.num_neurons, (float)probe.num_synapses,
        (float)probe.num_active_synapses,
        (float)probe.total_inferences, (float)probe.total_learning_steps,
        probe.avg_sparsity, probe.avg_inference_time_us,
        probe.current_learning_rate, probe.accuracy,
        (float)probe.memory_bytes,
        (float)probe.num_inputs, (float)probe.num_outputs,
        probe.is_cow_clone ? 1.0f : 0.0f,
        (float)probe.cow_ref_count,
        (float)probe.cow_shared_bytes, (float)probe.cow_private_bytes
    };
    (*env)->SetFloatArrayRegion(env, out, 0, 18, vals);
    return out;
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeBroadcastProbe(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)env; (void)cls;
    return (jint)nimcp_brain_broadcast_probe(
        (nimcp_brain_t)(uintptr_t)h);
}

// ============================================================================
// Network
// ============================================================================

JNIEXPORT jlong JNICALL Java_com_nimcp_NIMCP_00024Network_nativeCreate(
    JNIEnv *env, jclass cls, jint numInputs, jint numOutputs,
    jint numHidden, jfloat lr)
{
    (void)env; (void)cls;
    return (jlong)(uintptr_t)nimcp_network_create(
        (uint32_t)numInputs, (uint32_t)numOutputs,
        (uint32_t)numHidden, lr);
}

JNIEXPORT void JNICALL Java_com_nimcp_NIMCP_00024Network_nativeDestroy(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)env; (void)cls;
    if (h) nimcp_network_destroy((nimcp_network_t)(uintptr_t)h);
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Network_nativeForward(
    JNIEnv *env, jclass cls, jlong h, jfloatArray inputs,
    jfloatArray outputs)
{
    (void)cls;
    jint ilen = (*env)->GetArrayLength(env, inputs);
    jint olen = (*env)->GetArrayLength(env, outputs);
    jfloat *ins = (*env)->GetFloatArrayElements(env, inputs, NULL);
    if (!ins) return (jint)NIMCP_OK; /* OOM — JVM already threw */
    jfloat *outs = (*env)->GetFloatArrayElements(env, outputs, NULL);
    if (!outs) {
        (*env)->ReleaseFloatArrayElements(env, inputs, ins, JNI_ABORT);
        return (jint)NIMCP_OK; /* OOM — JVM already threw */
    }

    nimcp_status_t rc = nimcp_network_forward(
        (nimcp_network_t)(uintptr_t)h,
        ins, (uint32_t)ilen, outs, (uint32_t)olen);

    (*env)->ReleaseFloatArrayElements(env, inputs, ins, JNI_ABORT);
    (*env)->ReleaseFloatArrayElements(env, outputs, outs, 0); // copy back
    return (jint)rc;
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Network_nativeTrain(
    JNIEnv *env, jclass cls, jlong h, jfloatArray inputs,
    jfloatArray targets)
{
    (void)cls;
    jint ilen = (*env)->GetArrayLength(env, inputs);
    jint tlen = (*env)->GetArrayLength(env, targets);
    jfloat *ins = (*env)->GetFloatArrayElements(env, inputs, NULL);
    if (!ins) return (jint)NIMCP_OK; /* OOM — JVM already threw */
    jfloat *tgts = (*env)->GetFloatArrayElements(env, targets, NULL);
    if (!tgts) {
        (*env)->ReleaseFloatArrayElements(env, inputs, ins, JNI_ABORT);
        return (jint)NIMCP_OK; /* OOM — JVM already threw */
    }

    nimcp_status_t rc = nimcp_network_train(
        (nimcp_network_t)(uintptr_t)h,
        ins, (uint32_t)ilen, tgts, (uint32_t)tlen);

    (*env)->ReleaseFloatArrayElements(env, inputs, ins, JNI_ABORT);
    (*env)->ReleaseFloatArrayElements(env, targets, tgts, JNI_ABORT);
    return (jint)rc;
}

// ============================================================================
// Ethics
// ============================================================================

JNIEXPORT jlong JNICALL Java_com_nimcp_NIMCP_00024Ethics_nativeCreate(
    JNIEnv *env, jclass cls)
{
    (void)env; (void)cls;
    return (jlong)(uintptr_t)nimcp_ethics_create();
}

JNIEXPORT void JNICALL Java_com_nimcp_NIMCP_00024Ethics_nativeDestroy(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)env; (void)cls;
    if (h) nimcp_ethics_destroy((nimcp_ethics_t)(uintptr_t)h);
}

JNIEXPORT jfloat JNICALL Java_com_nimcp_NIMCP_00024Ethics_nativeCheck(
    JNIEnv *env, jclass cls, jlong h, jfloatArray situation)
{
    (void)cls;
    jint len = (*env)->GetArrayLength(env, situation);
    jfloat *arr = (*env)->GetFloatArrayElements(env, situation, NULL);
    if (!arr) return 0.0f; /* OOM — JVM already threw */

    float score = 0;
    nimcp_status_t rc = nimcp_ethics_check(
        (nimcp_ethics_t)(uintptr_t)h, arr, (uint32_t)len, &score);

    (*env)->ReleaseFloatArrayElements(env, situation, arr, JNI_ABORT);
    if (rc != NIMCP_OK) return 0.0f;
    return score;
}

// ============================================================================
// Knowledge Graph
// ============================================================================

JNIEXPORT jlong JNICALL Java_com_nimcp_NIMCP_00024KnowledgeGraph_nativeCreate(
    JNIEnv *env, jclass cls)
{
    (void)env; (void)cls;
    return (jlong)(uintptr_t)nimcp_knowledge_create();
}

JNIEXPORT void JNICALL Java_com_nimcp_NIMCP_00024KnowledgeGraph_nativeDestroy(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)env; (void)cls;
    if (h) nimcp_knowledge_destroy((nimcp_knowledge_t)(uintptr_t)h);
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024KnowledgeGraph_nativeAddFact(
    JNIEnv *env, jclass cls, jlong h, jstring subject,
    jstring predicate, jstring object)
{
    (void)cls;
    const char *c_subj = (*env)->GetStringUTFChars(env, subject, NULL);
    if (!c_subj) return (jint)NIMCP_OK; /* OOM — JVM already threw */
    const char *c_pred = (*env)->GetStringUTFChars(env, predicate, NULL);
    if (!c_pred) {
        (*env)->ReleaseStringUTFChars(env, subject, c_subj);
        return (jint)NIMCP_OK;
    }
    const char *c_obj  = (*env)->GetStringUTFChars(env, object, NULL);
    if (!c_obj) {
        (*env)->ReleaseStringUTFChars(env, subject, c_subj);
        (*env)->ReleaseStringUTFChars(env, predicate, c_pred);
        return (jint)NIMCP_OK;
    }

    nimcp_status_t rc = nimcp_knowledge_add_fact(
        (nimcp_knowledge_t)(uintptr_t)h, c_subj, c_pred, c_obj);

    (*env)->ReleaseStringUTFChars(env, subject, c_subj);
    (*env)->ReleaseStringUTFChars(env, predicate, c_pred);
    (*env)->ReleaseStringUTFChars(env, object, c_obj);
    return (jint)rc;
}

JNIEXPORT jstring JNICALL Java_com_nimcp_NIMCP_00024KnowledgeGraph_nativeQuery(
    JNIEnv *env, jclass cls, jlong h, jstring query, jint maxLen)
{
    (void)cls;
    const char *c_query = (*env)->GetStringUTFChars(env, query, NULL);
    if (!c_query) return NULL; /* OOM — JVM already threw */
    char *buf = nimcp_calloc((size_t)maxLen + 1, 1);
    if (!buf) {
        (*env)->ReleaseStringUTFChars(env, query, c_query);
        return NULL;
    }

    nimcp_status_t rc = nimcp_knowledge_query(
        (nimcp_knowledge_t)(uintptr_t)h, c_query, buf, (uint32_t)maxLen);

    (*env)->ReleaseStringUTFChars(env, query, c_query);

    jstring result = NULL;
    if (rc == NIMCP_OK) {
        result = (*env)->NewStringUTF(env, buf);
    }
    nimcp_free(buf);
    return result;
}

// ============================================================================
// Brain - Sensory/Multimodal (Group 1)
// ============================================================================

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSubmitSensory(
    JNIEnv *env, jclass cls, jlong h, jstring modality,
    jfloatArray data, jint width, jint height, jint channels, jint nSegments)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain || !brain->internal_brain) {
        throw_nimcp_exception(env, 1004, "Brain not initialized");
        return -1;
    }

    const char *c_mod = (*env)->GetStringUTFChars(env, modality, NULL);
    if (!c_mod) return -1;
    jint len = (*env)->GetArrayLength(env, data);
    jfloat *arr = (*env)->GetFloatArrayElements(env, data, NULL);
    if (!arr) {
        (*env)->ReleaseStringUTFChars(env, modality, c_mod);
        return -1;
    }

    brain_t ib = brain->internal_brain;
    int rc = 0;

    if (strcmp(c_mod, "visual") == 0) {
        if (ib->staged_sensory.visual_frame)
            nimcp_free(ib->staged_sensory.visual_frame);
        uint8_t *pixels = (uint8_t *)nimcp_malloc((size_t)len);
        if (!pixels) { rc = -1; goto cleanup_sensory; }
        for (jint i = 0; i < len; i++) {
            float v = arr[i];
            if (v <= 1.0f && v >= 0.0f) v *= 255.0f;
            pixels[i] = (uint8_t)(v > 255.0f ? 255 : (v < 0.0f ? 0 : v));
        }
        ib->staged_sensory.visual_frame = pixels;
        ib->staged_sensory.visual_width = (width > 0) ? (uint32_t)width : 32;
        ib->staged_sensory.visual_height = (height > 0) ? (uint32_t)height : 32;
        ib->staged_sensory.visual_channels = (channels > 0) ? (uint32_t)channels : 3;
    } else if (strcmp(c_mod, "audio") == 0) {
        if (ib->staged_sensory.audio_data)
            nimcp_free(ib->staged_sensory.audio_data);
        float *copy = (float *)nimcp_malloc((size_t)len * sizeof(float));
        if (!copy) { rc = -1; goto cleanup_sensory; }
        memcpy(copy, arr, (size_t)len * sizeof(float));
        ib->staged_sensory.audio_data = copy;
        ib->staged_sensory.audio_size = (uint32_t)len;
    } else if (strcmp(c_mod, "speech") == 0) {
        if (ib->staged_sensory.speech_data)
            nimcp_free(ib->staged_sensory.speech_data);
        float *copy = (float *)nimcp_malloc((size_t)len * sizeof(float));
        if (!copy) { rc = -1; goto cleanup_sensory; }
        memcpy(copy, arr, (size_t)len * sizeof(float));
        ib->staged_sensory.speech_data = copy;
        ib->staged_sensory.speech_size = (uint32_t)len;
    } else if (strcmp(c_mod, "somatosensory") == 0 || strcmp(c_mod, "somato") == 0) {
        if (ib->staged_sensory.somato_data)
            nimcp_free(ib->staged_sensory.somato_data);
        float *copy = (float *)nimcp_malloc((size_t)len * sizeof(float));
        if (!copy) { rc = -1; goto cleanup_sensory; }
        memcpy(copy, arr, (size_t)len * sizeof(float));
        ib->staged_sensory.somato_data = copy;
        ib->staged_sensory.somato_segments = (nSegments > 0) ? (uint32_t)nSegments : (uint32_t)len;
    } else {
        rc = -1;
    }

cleanup_sensory:
    (*env)->ReleaseFloatArrayElements(env, data, arr, JNI_ABORT);
    (*env)->ReleaseStringUTFChars(env, modality, c_mod);
    return rc;
}

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeVisualCortexProcess(
    JNIEnv *env, jclass cls, jlong h, jfloatArray pixels,
    jint width, jint height, jint channels)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain || !brain->internal_brain) return NULL;
    brain_t ib = brain->internal_brain;
    if (!ib->visual_cortex) return (*env)->NewFloatArray(env, 0);

    jint len = (*env)->GetArrayLength(env, pixels);
    jfloat *pix = (*env)->GetFloatArrayElements(env, pixels, NULL);
    if (!pix) return NULL;

    /* Convert float [0,1] to uint8 [0,255] */
    uint8_t *img = (uint8_t *)nimcp_malloc((size_t)len);
    if (!img) {
        (*env)->ReleaseFloatArrayElements(env, pixels, pix, JNI_ABORT);
        return NULL;
    }
    for (jint i = 0; i < len; i++) {
        float v = pix[i];
        if (v <= 1.0f && v >= 0.0f) v *= 255.0f;
        img[i] = (uint8_t)(v > 255.0f ? 255 : (v < 0.0f ? 0 : v));
    }
    (*env)->ReleaseFloatArrayElements(env, pixels, pix, JNI_ABORT);

    uint32_t feat_dim = visual_cortex_get_feature_dim(ib->visual_cortex);
    if (feat_dim == 0) feat_dim = 128;

    float *features = (float *)nimcp_calloc(feat_dim, sizeof(float));
    if (!features) { nimcp_free(img); return NULL; }

    bool ok = visual_cortex_process(ib->visual_cortex, img,
                                     (uint32_t)width, (uint32_t)height,
                                     (uint32_t)channels, features);
    nimcp_free(img);

    if (!ok) { nimcp_free(features); return (*env)->NewFloatArray(env, 0); }

    jfloatArray out = (*env)->NewFloatArray(env, (jsize)feat_dim);
    (*env)->SetFloatArrayRegion(env, out, 0, (jsize)feat_dim, features);
    nimcp_free(features);
    return out;
}

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeAudioCortexProcess(
    JNIEnv *env, jclass cls, jlong h, jfloatArray samples)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain || !brain->internal_brain) return NULL;
    brain_t ib = brain->internal_brain;
    if (!ib->audio_cortex) return (*env)->NewFloatArray(env, 0);

    jint len = (*env)->GetArrayLength(env, samples);
    jfloat *samps = (*env)->GetFloatArrayElements(env, samples, NULL);
    if (!samps) return NULL;

    uint32_t feat_dim = audio_cortex_get_feature_dim(ib->audio_cortex);
    if (feat_dim == 0) feat_dim = 128;

    float *features = (float *)nimcp_calloc(feat_dim, sizeof(float));
    if (!features) {
        (*env)->ReleaseFloatArrayElements(env, samples, samps, JNI_ABORT);
        return NULL;
    }

    bool ok = audio_cortex_process(ib->audio_cortex, samps,
                                    (uint32_t)len, 1, features);
    (*env)->ReleaseFloatArrayElements(env, samples, samps, JNI_ABORT);

    if (!ok) { nimcp_free(features); return (*env)->NewFloatArray(env, 0); }

    jfloatArray out = (*env)->NewFloatArray(env, (jsize)feat_dim);
    (*env)->SetFloatArrayRegion(env, out, 0, (jsize)feat_dim, features);
    nimcp_free(features);
    return out;
}

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSpeechCortexProcess(
    JNIEnv *env, jclass cls, jlong h, jfloatArray samples)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain || !brain->internal_brain) return NULL;
    brain_t ib = brain->internal_brain;
    if (!ib->speech_cortex) return (*env)->NewFloatArray(env, 0);

    jint len = (*env)->GetArrayLength(env, samples);
    jfloat *samps = (*env)->GetFloatArrayElements(env, samples, NULL);
    if (!samps) return NULL;

    /* speech_cortex_t is opaque and feature_dim isn't queryable via a
     * public getter; fall back to the default 128-dim output. */
    uint32_t feat_dim = 128;

    float *features = (float *)nimcp_calloc(feat_dim, sizeof(float));
    if (!features) {
        (*env)->ReleaseFloatArrayElements(env, samples, samps, JNI_ABORT);
        return NULL;
    }

    /* speech_cortex_process signature: (cortex, audio_data, num_samples, features)
     * API drift cleanup — previously had an extra channel arg. */
    bool ok = speech_cortex_process(ib->speech_cortex, samps,
                                     (uint32_t)len, features);
    (*env)->ReleaseFloatArrayElements(env, samples, samps, JNI_ABORT);

    if (!ok) { nimcp_free(features); return (*env)->NewFloatArray(env, 0); }

    jfloatArray out = (*env)->NewFloatArray(env, (jsize)feat_dim);
    (*env)->SetFloatArrayRegion(env, out, 0, (jsize)feat_dim, features);
    nimcp_free(features);
    return out;
}

// ============================================================================
// Brain - Avatar/Identity (Group 2)
// ============================================================================

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeGetAvatarState(
    JNIEnv *env, jclass cls, jlong h, jobjectArray outStrings)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain) return NULL;

    nimcp_avatar_state_t state;
    nimcp_status_t rc = nimcp_brain_get_avatar_state(brain, &state);
    if (rc != NIMCP_OK) return NULL;

    /* Pack key fields: mouth(6), AUs(16), emotion(5), gaze+head(6), voice(3), meta(2) = 38 floats */
    jfloatArray out = (*env)->NewFloatArray(env, 38);
    jfloat vals[38] = {
        /* mouth */
        state.mouth_open, state.lip_round, state.lip_upper,
        state.lip_lower, state.tongue_position, (float)state.current_viseme,
        /* FACS AUs */
        state.au1_inner_brow_raise, state.au2_outer_brow_raise,
        state.au4_brow_lower, state.au5_upper_lid_raise,
        state.au6_cheek_raise, state.au7_lid_tighten,
        state.au9_nose_wrinkle, state.au10_upper_lip_raise,
        state.au12_lip_corner_pull, state.au15_lip_corner_drop,
        state.au17_chin_raise, state.au20_lip_stretch,
        state.au23_lip_tighten, state.au25_lips_part,
        state.au26_jaw_drop, state.au28_lip_suck,
        /* emotion */
        state.valence, state.arousal, state.dominance,
        (float)state.emotion_id, state.emotion_intensity,
        /* gaze + head */
        state.gaze_x, state.gaze_y,
        state.head_pitch, state.head_yaw, state.head_roll, state.blink,
        /* voice */
        state.pitch_hz, state.speaking_rate, state.volume,
        /* meta */
        state.is_speaking ? 1.0f : 0.0f,
        (float)(state.timestamp_us / 1000000ULL) /* seconds, lossy */
    };
    (*env)->SetFloatArrayRegion(env, out, 0, 38, vals);
    return out;
}

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSpeak(
    JNIEnv *env, jclass cls, jlong h, jfloatArray semanticInput,
    jobjectArray outText)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain) return NULL;

    float *features = NULL;
    uint32_t num_features = 0;

    if (semanticInput) {
        jint flen = (*env)->GetArrayLength(env, semanticInput);
        features = (*env)->GetFloatArrayElements(env, semanticInput, NULL);
        if (!features) return NULL;
        num_features = (uint32_t)flen;
    }

    enum { SPEAK_MAX_TEXT = 4096 };
    char text[SPEAK_MAX_TEXT];
    memset(text, 0, sizeof(text));
    float confidence = 0.0f;
    float fluency = 0.0f;

    nimcp_status_t rc = nimcp_brain_speak(
        brain, features, num_features,
        text, SPEAK_MAX_TEXT, &confidence, &fluency);

    if (semanticInput && features)
        (*env)->ReleaseFloatArrayElements(env, semanticInput, features, JNI_ABORT);

    if (rc != NIMCP_OK) return NULL;

    /* Return text via outText[0] */
    (*env)->SetObjectArrayElement(env, outText, 0,
        (*env)->NewStringUTF(env, text));

    /* Return [confidence, fluency] */
    jfloatArray out = (*env)->NewFloatArray(env, 2);
    jfloat vals[2] = { confidence, fluency };
    (*env)->SetFloatArrayRegion(env, out, 0, 2, vals);
    return out;
}

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeGetNetworkMetrics(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain) return NULL;

    float ema_ann = 0, ema_cnn = 0, ema_snn = 0, ema_lnn = 0;
    uint64_t ann_steps = 0, cnn_steps = 0, snn_steps = 0, lnn_steps = 0;

    if (!nimcp_brain_get_network_metrics(brain,
            &ema_ann, &ema_cnn, &ema_snn, &ema_lnn,
            &ann_steps, &cnn_steps, &snn_steps, &lnn_steps))
        return NULL;

    /* Pack: 4 losses + 4 step counts = 8 floats */
    jfloatArray out = (*env)->NewFloatArray(env, 8);
    jfloat vals[8] = {
        ema_ann, ema_cnn, ema_snn, ema_lnn,
        (float)ann_steps, (float)cnn_steps,
        (float)snn_steps, (float)lnn_steps
    };
    (*env)->SetFloatArrayRegion(env, out, 0, 8, vals);
    return out;
}

// ============================================================================
// Brain - Core Inference (Group 3)
// ============================================================================

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeDecideFull(
    JNIEnv *env, jclass cls, jlong h, jfloatArray features,
    jobjectArray outStrings, jfloatArray outMeta)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain) return NULL;

    jint flen = (*env)->GetArrayLength(env, features);
    jfloat *feats = (*env)->GetFloatArrayElements(env, features, NULL);
    if (!feats) return NULL;

    char label[NIMCP_MAX_LABEL_SIZE];
    memset(label, 0, sizeof(label));
    float confidence = 0.0f;
    char explanation[NIMCP_NAME_BUFFER_SIZE];
    memset(explanation, 0, sizeof(explanation));
    enum { MAX_OUT = 4096 };
    float output_vector[MAX_OUT];
    uint32_t output_size = MAX_OUT;
    uint32_t num_active = 0;
    float sparsity = 0.0f;
    uint64_t inference_time_us = 0;

    nimcp_status_t rc = nimcp_brain_decide_full(
        brain, feats, (uint32_t)flen,
        label, &confidence, explanation,
        output_vector, &output_size,
        &num_active, &sparsity, &inference_time_us);

    (*env)->ReleaseFloatArrayElements(env, features, feats, JNI_ABORT);
    if (rc != NIMCP_OK) return NULL;

    /* Return label and explanation via outStrings */
    (*env)->SetObjectArrayElement(env, outStrings, 0,
        (*env)->NewStringUTF(env, label));
    (*env)->SetObjectArrayElement(env, outStrings, 1,
        (*env)->NewStringUTF(env, explanation));

    /* Return meta: [confidence, num_active, sparsity, inference_time_us] */
    jfloat meta[4] = { confidence, (float)num_active, sparsity,
                       (float)(inference_time_us / 1000.0f) };
    (*env)->SetFloatArrayRegion(env, outMeta, 0, 4, meta);

    /* Return output vector */
    uint32_t vec_len = (output_size < MAX_OUT) ? output_size : MAX_OUT;
    jfloatArray out = (*env)->NewFloatArray(env, (jsize)vec_len);
    (*env)->SetFloatArrayRegion(env, out, 0, (jsize)vec_len, output_vector);
    return out;
}

JNIEXPORT jobjectArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeGetTranscript(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain) return NULL;

    enum { MAX_ENTRIES = 32 };
    char summaries[MAX_ENTRIES][256];
    float saliences[MAX_ENTRIES];
    float confidences[MAX_ENTRIES];
    const char *modules[MAX_ENTRIES];

    memset(summaries, 0, sizeof(summaries));
    memset(saliences, 0, sizeof(saliences));
    memset(confidences, 0, sizeof(confidences));
    memset(modules, 0, sizeof(modules));

    uint32_t count = nimcp_brain_get_last_transcript(
        brain, summaries, saliences, confidences, modules, MAX_ENTRIES);

    if (count == 0) return NULL;

    /* Return as String[] where each entry is "module\tsummary\tsalience\tconfidence" */
    jclass strCls = (*env)->FindClass(env, "java/lang/String");
    jobjectArray arr = (*env)->NewObjectArray(env, (jsize)count, strCls, NULL);

    for (uint32_t i = 0; i < count; i++) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s\t%s\t%.6f\t%.6f",
            modules[i] ? modules[i] : "unknown",
            summaries[i], saliences[i], confidences[i]);
        (*env)->SetObjectArrayElement(env, arr, (jsize)i,
            (*env)->NewStringUTF(env, buf));
    }
    return arr;
}

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeGetCognitiveStats(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain) return NULL;

    uint32_t steps[13];
    float losses[13];
    uint32_t count = 0;
    memset(steps, 0, sizeof(steps));
    memset(losses, 0, sizeof(losses));

    nimcp_status_t rc = nimcp_brain_get_cognitive_stats(brain, steps, losses, &count);
    if (rc != NIMCP_OK) return NULL;

    /* Pack as interleaved [steps0, loss0, steps1, loss1, ...] */
    uint32_t n = (count < 13) ? count : 13;
    jfloatArray out = (*env)->NewFloatArray(env, (jsize)(n * 2));
    jfloat *vals = (jfloat *)nimcp_calloc(n * 2, sizeof(jfloat));
    if (!vals) return NULL;
    for (uint32_t i = 0; i < n; i++) {
        vals[i * 2] = (float)steps[i];
        vals[i * 2 + 1] = losses[i];
    }
    (*env)->SetFloatArrayRegion(env, out, 0, (jsize)(n * 2), vals);
    nimcp_free(vals);
    return out;
}

JNIEXPORT jfloat JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeGetAccuracy(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)env; (void)cls;
    return nimcp_brain_get_accuracy((nimcp_brain_t)(uintptr_t)h);
}

JNIEXPORT jfloat JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeGetLastGradientNorm(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)env; (void)cls;
    return nimcp_brain_get_last_gradient_norm((nimcp_brain_t)(uintptr_t)h);
}

// ============================================================================
// Brain - LNN/SNN/CNN (Group 4)
// ============================================================================

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeLnnCreate(
    JNIEnv *env, jclass cls, jlong h,
    jint nSensory, jint nInter, jint nCommand, jint nOutput)
{
    (void)cls;
    nimcp_brain_t brain_h = (nimcp_brain_t)(uintptr_t)h;
    if (!brain_h || !brain_h->internal_brain) {
        throw_nimcp_exception(env, 1004, "Brain not initialized");
        return -1;
    }
    brain_t brain = brain_h->internal_brain;

    if (brain->lnn_network) return 0; /* Already created — idempotent */

    if (!lnn_is_initialized()) lnn_init(1);

    brain->lnn_network = lnn_network_create_ncp(
        (uint32_t)nSensory, (uint32_t)nInter,
        (uint32_t)nCommand, (uint32_t)nOutput);
    if (!brain->lnn_network) {
        throw_nimcp_exception(env, 2000, "Failed to create LNN network");
        return -1;
    }
    lnn_network_init_weights(brain->lnn_network, 42);

    lnn_training_config_t cfg;
    lnn_training_config_default(&cfg);
    cfg.learning_rate = 0.01f;
    cfg.gradient_clip_norm = 100.0f;
    cfg.enable_plasticity_integration = true;
    cfg.lnn_train_mode = LNN_TRAIN_ADJOINT;
    cfg.track_statistics = true;

    if (brain->lnn_training_ctx) {
        lnn_training_destroy(brain->lnn_training_ctx);
        brain->lnn_training_ctx = NULL;
    }
    brain->lnn_training_ctx = lnn_training_create(brain->lnn_network, &cfg);
    return 0;
}

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeLnnForwardStep(
    JNIEnv *env, jclass cls, jlong h, jfloatArray features)
{
    (void)cls;
    nimcp_brain_t brain_h = (nimcp_brain_t)(uintptr_t)h;
    if (!brain_h || !brain_h->internal_brain) return NULL;
    brain_t brain = brain_h->internal_brain;
    if (!brain->lnn_network) return NULL;

    jint flen = (*env)->GetArrayLength(env, features);
    jfloat *feats = (*env)->GetFloatArrayElements(env, features, NULL);
    if (!feats) return NULL;

    uint32_t in_dims[] = {(uint32_t)flen};
    nimcp_tensor_t *input = nimcp_tensor_create(in_dims, 1, NIMCP_DTYPE_F32);
    if (!input) {
        (*env)->ReleaseFloatArrayElements(env, features, feats, JNI_ABORT);
        return NULL;
    }
    memcpy(nimcp_tensor_data(input), feats, (size_t)flen * sizeof(float));
    (*env)->ReleaseFloatArrayElements(env, features, feats, JNI_ABORT);

    /* API drift: use lnn_network_forward_step (takes pre-allocated output
     * tensor, returns int status). Allocate output before the call. */
    lnn_network_t* lnn = (lnn_network_t*)brain->lnn_network;
    uint32_t n_out = lnn ? lnn->n_outputs : 0;
    uint32_t out_shape[1] = { n_out };
    nimcp_tensor_t *output = (n_out > 0)
        ? nimcp_tensor_create(out_shape, 1, NIMCP_DTYPE_F32)
        : NULL;
    int forward_rc = -1;
    if (output) {
        forward_rc = lnn_network_forward_step(lnn, input, output, 0.01f);
    }
    nimcp_tensor_destroy(input);

    if (!output || forward_rc != 0) {
        if (output) nimcp_tensor_destroy(output);
        return NULL;
    }

    /* nimcp_tensor_total_elements was renamed to nimcp_tensor_numel. */
    size_t out_size = nimcp_tensor_numel(output);
    float *out_data = (float *)nimcp_tensor_data(output);

    jfloatArray out = (*env)->NewFloatArray(env, (jsize)out_size);
    (*env)->SetFloatArrayRegion(env, out, 0, (jsize)out_size, out_data);
    nimcp_tensor_destroy(output);
    return out;
}

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeLnnGetStats(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)cls;
    nimcp_brain_t brain_h = (nimcp_brain_t)(uintptr_t)h;
    if (!brain_h || !brain_h->internal_brain) return NULL;
    brain_t brain = brain_h->internal_brain;
    if (!brain->lnn_network) return NULL;

    lnn_network_stats_t stats;
    if (lnn_get_stats(brain->lnn_network, &stats) != 0) return NULL;

    /* Pack: fwd_steps, bwd_steps, ode_evals, avg_tau, state_norm, grad_norm, nan_count, inf_count */
    jfloatArray out = (*env)->NewFloatArray(env, 8);
    jfloat vals[8] = {
        (float)stats.forward_steps, (float)stats.backward_steps,
        (float)stats.ode_evaluations, stats.avg_tau_network,
        stats.state_norm, stats.gradient_norm,
        (float)stats.nan_count, (float)stats.inf_count
    };
    (*env)->SetFloatArrayRegion(env, out, 0, 8, vals);
    return out;
}

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSnnGetStats(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)cls;
    nimcp_brain_t brain_h = (nimcp_brain_t)(uintptr_t)h;
    if (!brain_h || !brain_h->internal_brain) return NULL;
    brain_t brain = brain_h->internal_brain;
    if (!brain->snn_network) return NULL;

    snn_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    if (snn_network_get_stats(brain->snn_network, &stats) != 0) return NULL;

    /* Pack: total_steps, total_spikes, mean_firing_rate, max_firing_rate,
             sparsity, synchrony, spikes_per_sample, silent, hyperactive, health, mem_bytes */
    jfloatArray out = (*env)->NewFloatArray(env, 11);
    jfloat vals[11] = {
        (float)stats.total_steps, (float)stats.total_spikes,
        stats.mean_firing_rate, stats.max_firing_rate,
        stats.sparsity, stats.synchrony,
        stats.spikes_per_sample,
        (float)stats.silent_neurons, (float)stats.hyperactive_neurons,
        (float)stats.health, (float)stats.memory_usage_bytes
    };
    (*env)->SetFloatArrayRegion(env, out, 0, 11, vals);
    return out;
}

JNIEXPORT void JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSnnSetInputScale(
    JNIEnv *env, jclass cls, jfloat scale)
{
    (void)env; (void)cls;
    extern void nimcp_snn_set_input_scale(float);
    nimcp_snn_set_input_scale((float)scale);
}

JNIEXPORT jfloat JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSnnGetInputScale(
    JNIEnv *env, jclass cls)
{
    (void)env; (void)cls;
    extern float nimcp_snn_get_input_scale(void);
    return (jfloat)nimcp_snn_get_input_scale();
}

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeCnnGetStats(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)cls;
    nimcp_brain_t brain_h = (nimcp_brain_t)(uintptr_t)h;
    if (!brain_h || !brain_h->internal_brain) return NULL;
    brain_t brain = brain_h->internal_brain;
    if (!brain->cnn_trainer) return NULL;

    /* Pack: num_layers, num_parameters, num_labels */
    jfloatArray out = (*env)->NewFloatArray(env, 3);
    jfloat vals[3] = {
        (float)cnn_get_layer_count(brain->cnn_trainer),
        (float)cnn_count_parameters(brain->cnn_trainer),
        (float)brain->num_output_labels
    };
    (*env)->SetFloatArrayRegion(env, out, 0, 3, vals);
    return out;
}

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeGetCortexCnnMetrics(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)cls;
    nimcp_brain_t brain_h = (nimcp_brain_t)(uintptr_t)h;
    if (!brain_h || !brain_h->internal_brain) return NULL;
    brain_t brain = brain_h->internal_brain;

    /* 4 cortex types x 4 fields each = 16 floats */
    /* Fields per cortex: last_loss, ema_loss, forward_steps, backward_steps */
    jfloat vals[16];
    memset(vals, 0, sizeof(vals));

    for (int ci = 0; ci < 4; ci++) {
        if (!brain->cortex_cnns[ci]) continue;
        cortex_cnn_metrics_t m = {0};
        if (cortex_cnn_get_metrics(brain->cortex_cnns[ci], &m) != 0) continue;
        vals[ci * 4 + 0] = m.last_loss;
        vals[ci * 4 + 1] = m.ema_loss;
        vals[ci * 4 + 2] = (float)m.forward_steps;
        vals[ci * 4 + 3] = (float)m.backward_steps;
    }

    jfloatArray out = (*env)->NewFloatArray(env, 16);
    (*env)->SetFloatArrayRegion(env, out, 0, 16, vals);
    return out;
}

// ============================================================================
// Brain - Configuration (Group 5)
// ============================================================================

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSetFastTraining(
    JNIEnv *env, jclass cls, jlong h, jboolean enabled)
{
    (void)env; (void)cls;
    nimcp_brain_t brain_h = (nimcp_brain_t)(uintptr_t)h;
    if (!brain_h || !brain_h->internal_brain) return -1;
    brain_h->internal_brain->config.fast_training_mode = (bool)enabled;
    return 0;
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSetTaskType(
    JNIEnv *env, jclass cls, jlong h, jstring taskType)
{
    (void)cls;
    nimcp_brain_t brain_h = (nimcp_brain_t)(uintptr_t)h;
    if (!brain_h || !brain_h->internal_brain) return -1;
    brain_t brain = brain_h->internal_brain;

    const char *c_task = (*env)->GetStringUTFChars(env, taskType, NULL);
    if (!c_task) return -1;

    extern task_strategy_t *strategy_create(brain_task_t task);
    brain_task_t task;
    int rc = 0;

    if (strcmp(c_task, "regression") == 0)            task = BRAIN_TASK_REGRESSION;
    else if (strcmp(c_task, "classification") == 0)    task = BRAIN_TASK_CLASSIFICATION;
    else if (strcmp(c_task, "pattern") == 0)           task = BRAIN_TASK_PATTERN_MATCHING;
    else if (strcmp(c_task, "association") == 0)       task = BRAIN_TASK_ASSOCIATION;
    else { rc = -1; goto done_task; }

    {
        task_strategy_t *new_strat = strategy_create(task);
        if (!new_strat) { rc = -1; goto done_task; }
        if (brain->strategy && !brain->is_cow_clone)
            nimcp_free(brain->strategy);
        brain->strategy = new_strat;
        brain->config.task = task;
    }

done_task:
    (*env)->ReleaseStringUTFChars(env, taskType, c_task);
    return rc;
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeEnableBiologicalPlasticity(
    JNIEnv *env, jclass cls, jlong h, jboolean enabled)
{
    (void)env; (void)cls;
    nimcp_brain_t brain_h = (nimcp_brain_t)(uintptr_t)h;
    if (!brain_h || !brain_h->internal_brain) return -1;
    brain_t brain = brain_h->internal_brain;
    brain->enable_plasticity_bridge = (bool)enabled;
    brain->enable_event_driven_plasticity = (bool)enabled;
    brain->plasticity_coordinator_enabled = (bool)enabled;
    return 0;
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeEnableMixedPrecision(
    JNIEnv *env, jclass cls, jlong h, jboolean enabled)
{
    (void)cls;
    nimcp_status_t rc = nimcp_brain_enable_mixed_precision(
        (nimcp_brain_t)(uintptr_t)h, (bool)enabled);
    if (rc != NIMCP_OK) {
        check_status(env, rc);
        return -1;
    }
    return 0;
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeEnableGradientCheckpointing(
    JNIEnv *env, jclass cls, jlong h, jboolean enabled, jint interval)
{
    (void)cls;
    nimcp_status_t rc = nimcp_brain_enable_gradient_checkpointing(
        (nimcp_brain_t)(uintptr_t)h, (bool)enabled, (uint32_t)interval);
    if (rc != NIMCP_OK) {
        check_status(env, rc);
        return -1;
    }
    return 0;
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeEnableWorldModel(
    JNIEnv *env, jclass cls, jlong h, jboolean enabled)
{
    (void)env; (void)cls;
    nimcp_brain_t brain_h = (nimcp_brain_t)(uintptr_t)h;
    if (!brain_h || !brain_h->internal_brain) return -1;
    brain_t brain = brain_h->internal_brain;
    brain->config.enable_world_model = (bool)enabled;
    if (enabled && !brain->world_model_enabled)
        brain->world_model_lazy_init = true;
    return 0;
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeEnableMultiNetwork(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)cls;
    nimcp_brain_t brain_h = (nimcp_brain_t)(uintptr_t)h;
    if (!brain_h || !brain_h->internal_brain) {
        throw_nimcp_exception(env, 1004, "Brain not initialized");
        return -1;
    }
    extern int brain_enable_multi_network_training(brain_t brain);
    int rc = brain_enable_multi_network_training(brain_h->internal_brain);
    if (rc < 0) {
        throw_nimcp_exception(env, 1004, "Failed to enable multi-network training");
        return -1;
    }
    return 0;
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeEnableHemispheric(
    JNIEnv *env, jclass cls, jlong h, jboolean enabled)
{
    (void)cls;
    nimcp_status_t rc = nimcp_brain_enable_hemispheric(
        (nimcp_brain_t)(uintptr_t)h, (bool)enabled);
    if (rc != NIMCP_OK) {
        check_status(env, rc);
        return -1;
    }
    return 0;
}

// ============================================================================
// Brain - Brain Regions (Group 6)
// ============================================================================

JNIEXPORT jfloat JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeMedullaGetArousal(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)env; (void)cls;
    nimcp_brain_t brain_h = (nimcp_brain_t)(uintptr_t)h;
    if (!brain_h || !brain_h->internal_brain) return 0.0f;
    return brain_ti_get_arousal(brain_h->internal_brain);
}

JNIEXPORT void JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeMedullaBoostArousal(
    JNIEnv *env, jclass cls, jlong h, jfloat amount)
{
    (void)env; (void)cls;
    nimcp_brain_t brain_h = (nimcp_brain_t)(uintptr_t)h;
    if (!brain_h || !brain_h->internal_brain) return;
    brain_ti_boost_arousal(brain_h->internal_brain, amount);
}

JNIEXPORT jfloat JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSleepGetPressure(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)env; (void)cls;
    nimcp_brain_t brain_h = (nimcp_brain_t)(uintptr_t)h;
    if (!brain_h || !brain_h->internal_brain) return 0.0f;
    sleep_system_t ss = brain_get_sleep_system(brain_h->internal_brain);
    if (!ss) return 0.0f;
    return sleep_get_pressure(ss);
}

JNIEXPORT jfloat JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeBgGetDopamine(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)env; (void)cls;
    nimcp_brain_t brain_h = (nimcp_brain_t)(uintptr_t)h;
    if (!brain_h || !brain_h->internal_brain) return 0.0f;
    return brain_ti_get_dopamine(brain_h->internal_brain);
}

JNIEXPORT jstring JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSubstrateGetHealth(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)cls;
    nimcp_brain_t brain_h = (nimcp_brain_t)(uintptr_t)h;
    if (!brain_h || !brain_h->internal_brain)
        return (*env)->NewStringUTF(env, "UNKNOWN");
    brain_t brain = brain_h->internal_brain;
    if (!brain->substrate_gpu_ctx)
        return (*env)->NewStringUTF(env, "UNKNOWN");
    return (*env)->NewStringUTF(env, "OPTIMAL");
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeFocusAttention(
    JNIEnv *env, jclass cls, jlong h, jstring modality)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain) return -1;

    const char *c_mod = (*env)->GetStringUTFChars(env, modality, NULL);
    if (!c_mod) return -1;

    nimcp_status_t rc = nimcp_brain_experience_attend(brain, c_mod, 1.0f);
    (*env)->ReleaseStringUTFChars(env, modality, c_mod);
    return (jint)rc;
}

// ============================================================================
// Edge Brain API (13 new methods)
// ============================================================================

// Helper: create a HashMap and return refs to put method
static jobject jni_create_hashmap(JNIEnv *env, jclass *map_cls, jmethodID *put_mid) {
    *map_cls = (*env)->FindClass(env, "java/util/HashMap");
    if (!*map_cls) return NULL;
    jmethodID init = (*env)->GetMethodID(env, *map_cls, "<init>", "()V");
    if (!init) return NULL;
    *put_mid = (*env)->GetMethodID(env, *map_cls, "put",
        "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    if (!*put_mid) return NULL;
    return (*env)->NewObject(env, *map_cls, init);
}

static void jni_map_put_int(JNIEnv *env, jobject map, jmethodID put, const char *key, int val) {
    jstring k = (*env)->NewStringUTF(env, key);
    jclass int_cls = (*env)->FindClass(env, "java/lang/Integer");
    jmethodID vof = (*env)->GetStaticMethodID(env, int_cls, "valueOf", "(I)Ljava/lang/Integer;");
    jobject v = (*env)->CallStaticObjectMethod(env, int_cls, vof, val);
    (*env)->CallObjectMethod(env, map, put, k, v);
    (*env)->DeleteLocalRef(env, k);
    (*env)->DeleteLocalRef(env, v);
}

static void jni_map_put_long(JNIEnv *env, jobject map, jmethodID put, const char *key, int64_t val) {
    jstring k = (*env)->NewStringUTF(env, key);
    jclass cls = (*env)->FindClass(env, "java/lang/Long");
    jmethodID vof = (*env)->GetStaticMethodID(env, cls, "valueOf", "(J)Ljava/lang/Long;");
    jobject v = (*env)->CallStaticObjectMethod(env, cls, vof, (jlong)val);
    (*env)->CallObjectMethod(env, map, put, k, v);
    (*env)->DeleteLocalRef(env, k);
    (*env)->DeleteLocalRef(env, v);
}

static void jni_map_put_float(JNIEnv *env, jobject map, jmethodID put, const char *key, double val) {
    jstring k = (*env)->NewStringUTF(env, key);
    jclass cls = (*env)->FindClass(env, "java/lang/Double");
    jmethodID vof = (*env)->GetStaticMethodID(env, cls, "valueOf", "(D)Ljava/lang/Double;");
    jobject v = (*env)->CallStaticObjectMethod(env, cls, vof, val);
    (*env)->CallObjectMethod(env, map, put, k, v);
    (*env)->DeleteLocalRef(env, k);
    (*env)->DeleteLocalRef(env, v);
}

static void jni_map_put_string(JNIEnv *env, jobject map, jmethodID put, const char *key, const char *val) {
    jstring k = (*env)->NewStringUTF(env, key);
    jstring v = (*env)->NewStringUTF(env, val ? val : "");
    (*env)->CallObjectMethod(env, map, put, k, v);
    (*env)->DeleteLocalRef(env, k);
    (*env)->DeleteLocalRef(env, v);
}

static void jni_map_put_bool(JNIEnv *env, jobject map, jmethodID put, const char *key, bool val) {
    jstring k = (*env)->NewStringUTF(env, key);
    jclass cls = (*env)->FindClass(env, "java/lang/Boolean");
    jmethodID vof = (*env)->GetStaticMethodID(env, cls, "valueOf", "(Z)Ljava/lang/Boolean;");
    jobject v = (*env)->CallStaticObjectMethod(env, cls, vof, (jboolean)val);
    (*env)->CallObjectMethod(env, map, put, k, v);
    (*env)->DeleteLocalRef(env, k);
    (*env)->DeleteLocalRef(env, v);
}

// 1. edge_resize(target_neurons, mode, knowledge_transfer) -> Map
JNIEXPORT jobject JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeEdgeResize(
    JNIEnv *env, jclass cls, jlong h, jint target, jstring mode, jboolean transfer)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain) return NULL;

    nimcp_resize_config_t config = nimcp_resize_config_default();
    config.target_neuron_count = (uint32_t)target;
    config.enable_knowledge_transfer = (bool)transfer;

    const char *mode_str = (*env)->GetStringUTFChars(env, mode, NULL);
    if (mode_str) {
        if (strcmp(mode_str, "expand") == 0) config.mode = NIMCP_RESIZE_EXPAND;
        else if (strcmp(mode_str, "rebalance") == 0) config.mode = NIMCP_RESIZE_REBALANCE;
        else config.mode = NIMCP_RESIZE_CONTRACT;
        (*env)->ReleaseStringUTFChars(env, mode, mode_str);
    }

    int ret = nimcp_edge_brain_resize(brain, &config);

    jclass map_cls; jmethodID put;
    jobject map = jni_create_hashmap(env, &map_cls, &put);
    if (!map) return NULL;
    jni_map_put_int(env, map, put, "status", ret);
    jni_map_put_int(env, map, put, "target_neurons", (int)target);
    return map;
}

// 2. edge_resize_check(target_neurons) -> Map
JNIEXPORT jobject JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeEdgeResizeCheck(
    JNIEnv *env, jclass cls, jlong h, jint target)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain) return NULL;

    nimcp_resize_config_t config = nimcp_resize_config_default();
    config.target_neuron_count = (uint32_t)target;
    config.mode = NIMCP_RESIZE_CONTRACT;
    nimcp_resize_report_t report = {0};
    nimcp_edge_brain_resize_check(brain, &config, &report);

    jclass map_cls; jmethodID put;
    jobject map = jni_create_hashmap(env, &map_cls, &put);
    if (!map) return NULL;
    jni_map_put_bool(env, map, put, "feasible", report.feasible);
    jni_map_put_int(env, map, put, "neurons_before", (int)report.neurons_before);
    jni_map_put_int(env, map, put, "neurons_after", (int)report.neurons_after);
    jni_map_put_float(env, map, put, "ram_delta_mb", report.estimated_ram_delta_mb);
    jni_map_put_string(env, map, put, "reason", report.reason);
    return map;
}

// 3. edge_distill(target, temperature, steps, inc_snn, inc_lnn, inc_cnn) -> Map
JNIEXPORT jobject JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeEdgeDistill(
    JNIEnv *env, jclass cls, jlong h, jint target, jfloat temperature,
    jint steps, jboolean inc_snn, jboolean inc_lnn, jboolean inc_cnn)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain) return NULL;

    nimcp_distill_config_t config = nimcp_distill_config_default();
    config.target_neurons = (uint32_t)target;
    config.temperature = temperature;
    config.distillation_steps = (uint32_t)steps;
    config.include_snn = (bool)inc_snn;
    config.include_lnn = (bool)inc_lnn;
    config.include_cnn = (bool)inc_cnn;

    nimcp_distill_report_t report = {0};
    nimcp_brain_t student = NULL;
    int ret = nimcp_brain_distill(brain, &student, &config, &report);

    jclass map_cls; jmethodID put;
    jobject map = jni_create_hashmap(env, &map_cls, &put);
    if (!map) return NULL;
    jni_map_put_int(env, map, put, "status", ret);
    jni_map_put_float(env, map, put, "accuracy_retention", report.accuracy_retention);
    jni_map_put_int(env, map, put, "neurons_selected", (int)report.neurons_selected);
    jni_map_put_float(env, map, put, "compression_ratio", report.compression_ratio);
    jni_map_put_float(env, map, put, "teacher_loss", report.teacher_loss);
    jni_map_put_float(env, map, put, "student_loss", report.student_loss);
    jni_map_put_int(env, map, put, "steps_trained", (int)report.steps_trained);
    return map;
}

// 4. edge_optimize_for_device(ram_mb, cpu_cores, camera, imu, motor, network, role) -> Map
JNIEXPORT jobject JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeEdgeOptimizeForDevice(
    JNIEnv *env, jclass cls, jlong h, jint ram_mb, jint cpu_cores,
    jboolean camera, jboolean imu, jboolean motor, jboolean network, jstring role)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain) return NULL;

    nimcp_device_profile_t profile = nimcp_device_profile_default();
    profile.ram_mb = (uint32_t)ram_mb;
    profile.cpu_cores = (uint32_t)cpu_cores;
    profile.has_camera = (bool)camera;
    profile.has_imu = (bool)imu;
    profile.has_motor_control = (bool)motor;
    profile.has_network = (bool)network;

    const char *role_str = (*env)->GetStringUTFChars(env, role, NULL);
    if (role_str) {
        if (strcmp(role_str, "sensor") == 0) profile.role = NIMCP_DEVICE_SENSOR;
        else if (strcmp(role_str, "actuator") == 0) profile.role = NIMCP_DEVICE_ACTUATOR;
        else if (strcmp(role_str, "coordinator") == 0) profile.role = NIMCP_DEVICE_COORDINATOR;
        else profile.role = NIMCP_DEVICE_GENERAL;
        (*env)->ReleaseStringUTFChars(env, role, role_str);
    }

    nimcp_optimization_report_t report = {0};
    nimcp_brain_t child = NULL;
    int ret = nimcp_brain_optimize_for_device(brain, &profile, &child, &report);

    jclass map_cls; jmethodID put;
    jobject map = jni_create_hashmap(env, &map_cls, &put);
    if (!map) return NULL;
    jni_map_put_int(env, map, put, "status", ret);
    jni_map_put_int(env, map, put, "neuron_count", (int)report.neuron_count);
    jni_map_put_int(env, map, put, "subsystems_enabled", (int)report.subsystems_enabled);
    jni_map_put_float(env, map, put, "estimated_ram_mb", report.estimated_ram_mb);
    jni_map_put_float(env, map, put, "estimated_inference_ms", report.estimated_inference_ms);
    jni_map_put_float(env, map, put, "accuracy_retention", report.accuracy_retention);
    return map;
}

// 5. edge_quantize(precision, calibration_samples) -> Map
JNIEXPORT jobject JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeEdgeQuantize(
    JNIEnv *env, jclass cls, jlong h, jstring precision, jint cal_samples)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain) return NULL;

    nimcp_quantize_config_t config = nimcp_quantize_config_default();
    config.calibration_samples = (uint32_t)cal_samples;

    const char *prec = (*env)->GetStringUTFChars(env, precision, NULL);
    if (prec) {
        if (strcmp(prec, "fp16") == 0) config.weight_precision = NIMCP_QUANT_FP16;
        else if (strcmp(prec, "int8_affine") == 0) config.weight_precision = NIMCP_QUANT_INT8_AFFINE;
        else if (strcmp(prec, "int4") == 0) config.weight_precision = NIMCP_QUANT_INT4;
        else if (strcmp(prec, "ternary") == 0) config.weight_precision = NIMCP_QUANT_TERNARY;
        else config.weight_precision = NIMCP_QUANT_INT8_SYMMETRIC;
        (*env)->ReleaseStringUTFChars(env, precision, prec);
    }

    int ret = nimcp_brain_quantize(brain, &config);

    jclass map_cls; jmethodID put;
    jobject map = jni_create_hashmap(env, &map_cls, &put);
    if (!map) return NULL;
    jni_map_put_int(env, map, put, "status", ret);
    return map;
}

// 6. edge_score_importance(num_neurons) -> float[]
JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeEdgeScoreImportance(
    JNIEnv *env, jclass cls, jlong h, jint num_neurons)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain || num_neurons <= 0) return NULL;

    uint32_t n = (uint32_t)num_neurons;
    float *scores = (float*)calloc(n, sizeof(float));
    if (!scores) return NULL;

    nimcp_edge_score_neuron_importance(brain, scores, n);

    jfloatArray arr = (*env)->NewFloatArray(env, (jsize)n);
    if (arr) (*env)->SetFloatArrayRegion(env, arr, 0, (jsize)n, scores);
    free(scores);
    return arr;
}

// ============================================================================
// Swarm / Sensor / Watchdog / ROS2 / MAVLink (module-level handles)
// ============================================================================

static nimcp_swarm_master_t*       g_jni_swarm_master   = NULL;
static nimcp_swarm_edge_runtime_t* g_jni_swarm_edge     = NULL;
static nimcp_sensor_hub_t*         g_jni_sensor_hub     = NULL;
static nimcp_safety_watchdog_t*    g_jni_safety_watchdog = NULL;
static nimcp_ros2_bridge_t*        g_jni_ros2_bridge    = NULL;
static nimcp_mavlink_bridge_t*     g_jni_mavlink_bridge = NULL;

// --- Swarm Master ---

JNIEXPORT jboolean JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSwarmMasterCreate(
    JNIEnv *env, jclass cls, jlong h, jint device_id, jint listen_port,
    jint sync_interval_ms, jint heartbeat_timeout_ms, jint min_devices)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain || g_jni_swarm_master) return JNI_FALSE;
    nimcp_master_config_t cfg = nimcp_swarm_master_config_default();
    cfg.device_id = (uint32_t)device_id;
    cfg.listen_port = (uint16_t)listen_port;
    cfg.sync_interval_ms = (uint32_t)sync_interval_ms;
    cfg.heartbeat_timeout_ms = (uint32_t)heartbeat_timeout_ms;
    cfg.min_devices_for_sync = (uint32_t)min_devices;
    g_jni_swarm_master = nimcp_swarm_master_create(brain, &cfg);
    return g_jni_swarm_master ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSwarmMasterDestroy(
    JNIEnv *env, jclass cls)
{
    (void)env; (void)cls;
    if (g_jni_swarm_master) { nimcp_swarm_master_destroy(g_jni_swarm_master); g_jni_swarm_master = NULL; }
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSwarmMasterStart(
    JNIEnv *env, jclass cls)
{
    (void)env; (void)cls;
    if (!g_jni_swarm_master) return -1;
    return (jint)nimcp_swarm_master_start(g_jni_swarm_master);
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSwarmMasterStop(
    JNIEnv *env, jclass cls)
{
    (void)env; (void)cls;
    if (!g_jni_swarm_master) return -1;
    return (jint)nimcp_swarm_master_stop(g_jni_swarm_master);
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSwarmMasterKick(
    JNIEnv *env, jclass cls, jint device_id)
{
    (void)env; (void)cls;
    if (!g_jni_swarm_master) return -1;
    return (jint)nimcp_swarm_master_kick(g_jni_swarm_master, (uint32_t)device_id);
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSwarmMasterForceSync(
    JNIEnv *env, jclass cls)
{
    (void)env; (void)cls;
    if (!g_jni_swarm_master) return -1;
    return (jint)nimcp_swarm_master_force_sync(g_jni_swarm_master);
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSwarmMasterGetPeerCount(
    JNIEnv *env, jclass cls)
{
    (void)env; (void)cls;
    if (!g_jni_swarm_master) return 0;
    return (jint)nimcp_swarm_master_get_peer_count(g_jni_swarm_master);
}

JNIEXPORT jobject JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSwarmMasterGetPeerInfo(
    JNIEnv *env, jclass cls, jint device_id)
{
    (void)cls;
    if (!g_jni_swarm_master) return NULL;
    nimcp_peer_entry_t entry = {0};
    int ret = nimcp_swarm_master_get_peer_info(g_jni_swarm_master, (uint32_t)device_id, &entry);
    if (ret != 0) return NULL;
    jclass map_cls; jmethodID put;
    jobject map = jni_create_hashmap(env, &map_cls, &put);
    if (!map) return NULL;
    jni_map_put_int(env, map, put, "device_id", (int)entry.device_id);
    jni_map_put_int(env, map, put, "state", (int)entry.state);
    jni_map_put_string(env, map, put, "address", entry.address);
    jni_map_put_int(env, map, put, "port", (int)entry.port);
    jni_map_put_int(env, map, put, "missed_heartbeats", (int)entry.missed_heartbeats);
    jni_map_put_int(env, map, put, "anomaly_count", (int)entry.anomaly_count);
    jni_map_put_long(env, map, put, "total_syncs", (int64_t)entry.total_syncs);
    jni_map_put_bool(env, map, put, "quarantined", entry.quarantined);
    jni_map_put_float(env, map, put, "gradient_norm_ema", entry.gradient_norm_ema);
    return map;
}

// --- Swarm Edge ---

JNIEXPORT jboolean JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSwarmEdgeCreate(
    JNIEnv *env, jclass cls, jlong h, jint device_id, jint hb_interval,
    jint reconnect_delay, jboolean local_learn)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain || g_jni_swarm_edge) return JNI_FALSE;
    nimcp_edge_runtime_config_t cfg = nimcp_swarm_edge_config_default();
    cfg.device_id = (uint32_t)device_id;
    cfg.heartbeat_interval_ms = (uint32_t)hb_interval;
    cfg.reconnect_delay_ms = (uint32_t)reconnect_delay;
    cfg.enable_local_learning = (bool)local_learn;
    g_jni_swarm_edge = nimcp_swarm_edge_create(brain, &cfg);
    return g_jni_swarm_edge ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSwarmEdgeDestroy(
    JNIEnv *env, jclass cls)
{
    (void)env; (void)cls;
    if (g_jni_swarm_edge) { nimcp_swarm_edge_destroy(g_jni_swarm_edge); g_jni_swarm_edge = NULL; }
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSwarmEdgeStart(
    JNIEnv *env, jclass cls)
{
    (void)env; (void)cls;
    if (!g_jni_swarm_edge) return -1;
    return (jint)nimcp_swarm_edge_start(g_jni_swarm_edge);
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSwarmEdgeStop(
    JNIEnv *env, jclass cls)
{
    (void)env; (void)cls;
    if (!g_jni_swarm_edge) return -1;
    return (jint)nimcp_swarm_edge_stop(g_jni_swarm_edge);
}

JNIEXPORT jboolean JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSwarmEdgeIsConnected(
    JNIEnv *env, jclass cls)
{
    (void)env; (void)cls;
    if (!g_jni_swarm_edge) return JNI_FALSE;
    return nimcp_swarm_edge_is_connected(g_jni_swarm_edge) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSwarmEdgeSubmitGradients(
    JNIEnv *env, jclass cls, jfloatArray gradients)
{
    (void)cls;
    if (!g_jni_swarm_edge) return -1;
    jsize n = (*env)->GetArrayLength(env, gradients);
    float *data = (*env)->GetFloatArrayElements(env, gradients, NULL);
    if (!data) return -1;
    int ret = nimcp_swarm_edge_submit_gradients(g_jni_swarm_edge, data, (uint32_t)n);
    (*env)->ReleaseFloatArrayElements(env, gradients, data, JNI_ABORT);
    return (jint)ret;
}

// --- Sensor Hub ---

JNIEXPORT jboolean JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSensorHubCreate(
    JNIEnv *env, jclass cls, jint max_sensors)
{
    (void)env; (void)cls;
    if (g_jni_sensor_hub) return JNI_FALSE;
    g_jni_sensor_hub = nimcp_sensor_hub_create((uint32_t)max_sensors);
    return g_jni_sensor_hub ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSensorHubDestroy(
    JNIEnv *env, jclass cls)
{
    (void)env; (void)cls;
    if (g_jni_sensor_hub) { nimcp_sensor_hub_destroy(g_jni_sensor_hub); g_jni_sensor_hub = NULL; }
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSensorRegister(
    JNIEnv *env, jclass cls, jint sensor_id, jint type, jint format,
    jstring name, jfloat sample_rate, jint max_data)
{
    (void)cls;
    if (!g_jni_sensor_hub) return -1;
    nimcp_sensor_descriptor_t desc = {0};
    desc.sensor_id = (uint32_t)sensor_id;
    desc.type = (nimcp_sensor_type_t)type;
    desc.format = (nimcp_sensor_format_t)format;
    const char *cname = (*env)->GetStringUTFChars(env, name, NULL);
    if (cname) { strncpy(desc.name, cname, sizeof(desc.name) - 1); (*env)->ReleaseStringUTFChars(env, name, cname); }
    desc.sample_rate_hz = (float)sample_rate;
    desc.max_data_count = (uint32_t)max_data;
    return (jint)nimcp_sensor_register(g_jni_sensor_hub, &desc);
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSensorSubmitReading(
    JNIEnv *env, jclass cls, jint sensor_id, jfloatArray data, jfloat confidence)
{
    (void)cls;
    if (!g_jni_sensor_hub) return -1;
    jsize n = (*env)->GetArrayLength(env, data);
    float *cdata = (*env)->GetFloatArrayElements(env, data, NULL);
    if (!cdata) return -1;
    nimcp_sensor_reading_t reading = {0};
    reading.sensor_id = (uint32_t)sensor_id;
    reading.data = cdata;
    reading.data_count = (uint32_t)n;
    reading.confidence = (float)confidence;
    reading.valid = true;
    int ret = nimcp_sensor_submit_reading(g_jni_sensor_hub, &reading);
    (*env)->ReleaseFloatArrayElements(env, data, cdata, JNI_ABORT);
    return (jint)ret;
}

JNIEXPORT jobject JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSensorGetLatest(
    JNIEnv *env, jclass cls, jint sensor_id)
{
    (void)cls;
    if (!g_jni_sensor_hub) return NULL;
    nimcp_sensor_reading_t reading = {0};
    int ret = nimcp_sensor_get_latest(g_jni_sensor_hub, (uint32_t)sensor_id, &reading);
    if (ret != 0) return NULL;
    jclass map_cls; jmethodID put;
    jobject map = jni_create_hashmap(env, &map_cls, &put);
    if (!map) return NULL;
    jni_map_put_int(env, map, put, "sensor_id", (int)reading.sensor_id);
    jni_map_put_int(env, map, put, "type", (int)reading.type);
    jni_map_put_float(env, map, put, "confidence", reading.confidence);
    jni_map_put_bool(env, map, put, "valid", reading.valid);
    return map;
}

JNIEXPORT jobject JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSensorGetAllLatest(
    JNIEnv *env, jclass cls)
{
    (void)cls;
    jclass list_cls = (*env)->FindClass(env, "java/util/ArrayList");
    jmethodID list_init = (*env)->GetMethodID(env, list_cls, "<init>", "()V");
    jmethodID list_add = (*env)->GetMethodID(env, list_cls, "add", "(Ljava/lang/Object;)Z");
    jobject list = (*env)->NewObject(env, list_cls, list_init);
    if (!g_jni_sensor_hub) return list;
    uint32_t count = nimcp_sensor_get_count(g_jni_sensor_hub);
    if (count == 0) return list;
    nimcp_sensor_reading_t *readings = (nimcp_sensor_reading_t*)calloc(count, sizeof(nimcp_sensor_reading_t));
    if (!readings) return list;
    int got = nimcp_sensor_get_all_latest(g_jni_sensor_hub, readings, count);
    for (int i = 0; i < got; i++) {
        jclass mc; jmethodID mp;
        jobject entry = jni_create_hashmap(env, &mc, &mp);
        if (entry) {
            jni_map_put_int(env, entry, mp, "sensor_id", (int)readings[i].sensor_id);
            jni_map_put_int(env, entry, mp, "type", (int)readings[i].type);
            jni_map_put_float(env, entry, mp, "confidence", readings[i].confidence);
            jni_map_put_bool(env, entry, mp, "valid", readings[i].valid);
            (*env)->CallBooleanMethod(env, list, list_add, entry);
            (*env)->DeleteLocalRef(env, entry);
        }
    }
    free(readings);
    return list;
}

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSensorComposeFeatures(
    JNIEnv *env, jclass cls, jint max_features)
{
    (void)cls;
    if (!g_jni_sensor_hub) return (*env)->NewFloatArray(env, 0);
    float *features = (float*)calloc((uint32_t)max_features, sizeof(float));
    if (!features) return (*env)->NewFloatArray(env, 0);
    int count = nimcp_sensor_compose_feature_vector(g_jni_sensor_hub, features, (uint32_t)max_features);
    if (count < 0) count = 0;
    jfloatArray arr = (*env)->NewFloatArray(env, (jsize)count);
    if (arr && count > 0) (*env)->SetFloatArrayRegion(env, arr, 0, (jsize)count, features);
    free(features);
    return arr;
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSensorGetCount(
    JNIEnv *env, jclass cls)
{
    (void)env; (void)cls;
    if (!g_jni_sensor_hub) return 0;
    return (jint)nimcp_sensor_get_count(g_jni_sensor_hub);
}

// --- Safety Watchdog ---

JNIEXPORT jboolean JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeWatchdogCreate(
    JNIEnv *env, jclass cls, jint timeout_ms, jint action, jfloat max_magnitude, jint max_outputs)
{
    (void)env; (void)cls;
    if (g_jni_safety_watchdog) return JNI_FALSE;
    nimcp_watchdog_config_t cfg = nimcp_watchdog_config_default();
    cfg.timeout_ms = (uint32_t)timeout_ms;
    cfg.action = (nimcp_safe_action_t)action;
    cfg.validation.max_output_magnitude = (float)max_magnitude;
    cfg.max_outputs = (uint32_t)max_outputs;
    g_jni_safety_watchdog = nimcp_watchdog_create(&cfg);
    return g_jni_safety_watchdog ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeWatchdogDestroy(
    JNIEnv *env, jclass cls)
{
    (void)env; (void)cls;
    if (g_jni_safety_watchdog) { nimcp_watchdog_destroy(g_jni_safety_watchdog); g_jni_safety_watchdog = NULL; }
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeWatchdogArm(
    JNIEnv *env, jclass cls)
{
    (void)env; (void)cls;
    if (!g_jni_safety_watchdog) return -1;
    return (jint)nimcp_watchdog_arm(g_jni_safety_watchdog);
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeWatchdogDisarm(
    JNIEnv *env, jclass cls)
{
    (void)env; (void)cls;
    if (!g_jni_safety_watchdog) return -1;
    return (jint)nimcp_watchdog_disarm(g_jni_safety_watchdog);
}

JNIEXPORT void JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeWatchdogHeartbeat(
    JNIEnv *env, jclass cls)
{
    (void)env; (void)cls;
    if (g_jni_safety_watchdog) nimcp_watchdog_heartbeat(g_jni_safety_watchdog);
}

JNIEXPORT jboolean JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeWatchdogValidateOutput(
    JNIEnv *env, jclass cls, jfloatArray output)
{
    (void)cls;
    if (!g_jni_safety_watchdog) return JNI_FALSE;
    jsize n = (*env)->GetArrayLength(env, output);
    float *data = (*env)->GetFloatArrayElements(env, output, NULL);
    if (!data) return JNI_FALSE;
    int ret = nimcp_watchdog_validate_output(g_jni_safety_watchdog, data, (uint32_t)n);
    (*env)->ReleaseFloatArrayElements(env, output, data, 0);
    return (ret == 0) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeWatchdogGetSafeOutput(
    JNIEnv *env, jclass cls, jint num_outputs)
{
    (void)cls;
    if (!g_jni_safety_watchdog || num_outputs <= 0) return (*env)->NewFloatArray(env, 0);
    float *out = (float*)calloc((uint32_t)num_outputs, sizeof(float));
    if (!out) return (*env)->NewFloatArray(env, 0);
    nimcp_watchdog_get_safe_output(g_jni_safety_watchdog, out, (uint32_t)num_outputs);
    jfloatArray arr = (*env)->NewFloatArray(env, (jsize)num_outputs);
    if (arr) (*env)->SetFloatArrayRegion(env, arr, 0, (jsize)num_outputs, out);
    free(out);
    return arr;
}

JNIEXPORT void JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeWatchdogEstop(
    JNIEnv *env, jclass cls)
{
    (void)env; (void)cls;
    if (g_jni_safety_watchdog) nimcp_watchdog_estop(g_jni_safety_watchdog);
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeWatchdogReset(
    JNIEnv *env, jclass cls)
{
    (void)env; (void)cls;
    if (!g_jni_safety_watchdog) return -1;
    return (jint)nimcp_watchdog_reset(g_jni_safety_watchdog);
}

JNIEXPORT jstring JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeWatchdogGetState(
    JNIEnv *env, jclass cls)
{
    (void)cls;
    if (!g_jni_safety_watchdog) return (*env)->NewStringUTF(env, "NONE");
    nimcp_watchdog_state_t state = nimcp_watchdog_get_state(g_jni_safety_watchdog);
    return (*env)->NewStringUTF(env, nimcp_watchdog_state_name(state));
}

// --- ROS 2 Bridge ---

JNIEXPORT jboolean JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeRos2BridgeCreate(
    JNIEnv *env, jclass cls, jlong h, jstring node_name, jfloat cmd_rate,
    jfloat inf_rate, jint input_dim, jboolean sub_imu, jboolean sub_odom)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain || g_jni_ros2_bridge) return JNI_FALSE;
    nimcp_ros2_config_t cfg = nimcp_ros2_config_default();
    const char *name = (*env)->GetStringUTFChars(env, node_name, NULL);
    if (name) { cfg.node_name = name; }
    cfg.cmd_rate_hz = (float)cmd_rate;
    cfg.inference_rate_hz = (float)inf_rate;
    cfg.brain_input_dim = (uint32_t)input_dim;
    cfg.subscribe_imu = (bool)sub_imu;
    cfg.subscribe_odom = (bool)sub_odom;
    g_jni_ros2_bridge = nimcp_ros2_bridge_create(brain, &cfg);
    if (name) (*env)->ReleaseStringUTFChars(env, node_name, name);
    return g_jni_ros2_bridge ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeRos2BridgeDestroy(
    JNIEnv *env, jclass cls)
{
    (void)env; (void)cls;
    if (g_jni_ros2_bridge) { nimcp_ros2_bridge_destroy(g_jni_ros2_bridge); g_jni_ros2_bridge = NULL; }
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeRos2BridgeStart(
    JNIEnv *env, jclass cls)
{
    (void)env; (void)cls;
    if (!g_jni_ros2_bridge) return -1;
    return (jint)nimcp_ros2_bridge_start(g_jni_ros2_bridge);
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeRos2BridgeStop(
    JNIEnv *env, jclass cls)
{
    (void)env; (void)cls;
    if (!g_jni_ros2_bridge) return -1;
    return (jint)nimcp_ros2_bridge_stop(g_jni_ros2_bridge);
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeRos2BridgeInjectSensor(
    JNIEnv *env, jclass cls, jstring topic, jfloatArray data)
{
    (void)cls;
    if (!g_jni_ros2_bridge) return -1;
    const char *ctopic = (*env)->GetStringUTFChars(env, topic, NULL);
    if (!ctopic) return -1;
    jsize n = (*env)->GetArrayLength(env, data);
    float *cdata = (*env)->GetFloatArrayElements(env, data, NULL);
    if (!cdata) { (*env)->ReleaseStringUTFChars(env, topic, ctopic); return -1; }
    int ret = nimcp_ros2_bridge_inject_sensor(g_jni_ros2_bridge, ctopic, cdata, (uint32_t)n);
    (*env)->ReleaseFloatArrayElements(env, data, cdata, JNI_ABORT);
    (*env)->ReleaseStringUTFChars(env, topic, ctopic);
    return (jint)ret;
}

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeRos2BridgeGetLastCmd(
    JNIEnv *env, jclass cls, jint max_count)
{
    (void)cls;
    if (!g_jni_ros2_bridge || max_count <= 0) return (*env)->NewFloatArray(env, 0);
    float *data = (float*)calloc((uint32_t)max_count, sizeof(float));
    if (!data) return (*env)->NewFloatArray(env, 0);
    int got = nimcp_ros2_bridge_get_last_cmd(g_jni_ros2_bridge, data, (uint32_t)max_count);
    if (got < 0) got = 0;
    jfloatArray arr = (*env)->NewFloatArray(env, (jsize)got);
    if (arr && got > 0) (*env)->SetFloatArrayRegion(env, arr, 0, (jsize)got, data);
    free(data);
    return arr;
}

// --- MAVLink Bridge ---

JNIEXPORT jboolean JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeMavlinkCreate(
    JNIEnv *env, jclass cls, jstring conn_str, jint conn_type, jint baud,
    jint sys_id, jfloat geofence)
{
    (void)cls;
    if (g_jni_mavlink_bridge) return JNI_FALSE;
    nimcp_mavlink_config_t cfg = nimcp_mavlink_config_default();
    const char *cs = (*env)->GetStringUTFChars(env, conn_str, NULL);
    if (cs) { strncpy(cfg.connection_string, cs, sizeof(cfg.connection_string) - 1); (*env)->ReleaseStringUTFChars(env, conn_str, cs); }
    cfg.conn_type = (nimcp_mavlink_conn_type_t)conn_type;
    cfg.baud_rate = (uint32_t)baud;
    cfg.system_id = (uint8_t)sys_id;
    cfg.geofence_radius_m = (float)geofence;
    g_jni_mavlink_bridge = nimcp_mavlink_bridge_create(&cfg);
    return g_jni_mavlink_bridge ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeMavlinkDestroy(
    JNIEnv *env, jclass cls)
{
    (void)env; (void)cls;
    if (g_jni_mavlink_bridge) { nimcp_mavlink_bridge_destroy(g_jni_mavlink_bridge); g_jni_mavlink_bridge = NULL; }
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeMavlinkConnect(JNIEnv *env, jclass cls) { (void)env; (void)cls; if (!g_jni_mavlink_bridge) return -1; return (jint)nimcp_mavlink_bridge_connect(g_jni_mavlink_bridge); }
JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeMavlinkDisconnect(JNIEnv *env, jclass cls) { (void)env; (void)cls; if (!g_jni_mavlink_bridge) return -1; return (jint)nimcp_mavlink_bridge_disconnect(g_jni_mavlink_bridge); }
JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeMavlinkStart(JNIEnv *env, jclass cls) { (void)env; (void)cls; if (!g_jni_mavlink_bridge) return -1; return (jint)nimcp_mavlink_bridge_start(g_jni_mavlink_bridge); }
JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeMavlinkStop(JNIEnv *env, jclass cls) { (void)env; (void)cls; if (!g_jni_mavlink_bridge) return -1; return (jint)nimcp_mavlink_bridge_stop(g_jni_mavlink_bridge); }

JNIEXPORT jobject JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeMavlinkGetAttitude(
    JNIEnv *env, jclass cls)
{
    (void)cls;
    if (!g_jni_mavlink_bridge) return NULL;
    nimcp_mavlink_attitude_t att = {0};
    if (nimcp_mavlink_get_attitude(g_jni_mavlink_bridge, &att) != 0) return NULL;
    jclass mc; jmethodID mp;
    jobject map = jni_create_hashmap(env, &mc, &mp);
    if (!map) return NULL;
    jni_map_put_float(env, map, mp, "roll", att.roll);
    jni_map_put_float(env, map, mp, "pitch", att.pitch);
    jni_map_put_float(env, map, mp, "yaw", att.yaw);
    jni_map_put_float(env, map, mp, "rollspeed", att.rollspeed);
    jni_map_put_float(env, map, mp, "pitchspeed", att.pitchspeed);
    jni_map_put_float(env, map, mp, "yawspeed", att.yawspeed);
    return map;
}

JNIEXPORT jobject JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeMavlinkGetPosition(
    JNIEnv *env, jclass cls)
{
    (void)cls;
    if (!g_jni_mavlink_bridge) return NULL;
    nimcp_mavlink_position_t pos = {0};
    if (nimcp_mavlink_get_position(g_jni_mavlink_bridge, &pos) != 0) return NULL;
    jclass mc; jmethodID mp;
    jobject map = jni_create_hashmap(env, &mc, &mp);
    if (!map) return NULL;
    jni_map_put_float(env, map, mp, "latitude", pos.latitude);
    jni_map_put_float(env, map, mp, "longitude", pos.longitude);
    jni_map_put_float(env, map, mp, "altitude_msl", pos.altitude_msl);
    jni_map_put_float(env, map, mp, "altitude_rel", pos.altitude_rel);
    jni_map_put_float(env, map, mp, "vx", pos.vx);
    jni_map_put_float(env, map, mp, "vy", pos.vy);
    jni_map_put_float(env, map, mp, "vz", pos.vz);
    jni_map_put_float(env, map, mp, "heading", pos.heading);
    jni_map_put_int(env, map, mp, "fix_type", pos.fix_type);
    jni_map_put_int(env, map, mp, "satellites", pos.satellites);
    return map;
}

JNIEXPORT jobject JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeMavlinkGetBattery(
    JNIEnv *env, jclass cls)
{
    (void)cls;
    if (!g_jni_mavlink_bridge) return NULL;
    nimcp_mavlink_battery_t bat = {0};
    if (nimcp_mavlink_get_battery(g_jni_mavlink_bridge, &bat) != 0) return NULL;
    jclass mc; jmethodID mp;
    jobject map = jni_create_hashmap(env, &mc, &mp);
    if (!map) return NULL;
    jni_map_put_float(env, map, mp, "voltage", bat.voltage);
    jni_map_put_float(env, map, mp, "current", bat.current);
    jni_map_put_float(env, map, mp, "remaining_pct", bat.remaining_pct);
    jni_map_put_int(env, map, mp, "consumed_mah", bat.consumed_mah);
    return map;
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeMavlinkSetVelocity(
    JNIEnv *env, jclass cls, jfloat vx, jfloat vy, jfloat vz, jfloat yaw_rate)
{
    (void)env; (void)cls;
    if (!g_jni_mavlink_bridge) return -1;
    return (jint)nimcp_mavlink_set_velocity(g_jni_mavlink_bridge, vx, vy, vz, yaw_rate);
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeMavlinkArm(
    JNIEnv *env, jclass cls, jboolean arm)
{
    (void)env; (void)cls;
    if (!g_jni_mavlink_bridge) return -1;
    return (jint)nimcp_mavlink_arm(g_jni_mavlink_bridge, (bool)arm);
}

JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeMavlinkTakeoff(JNIEnv *env, jclass cls, jfloat alt) { (void)env; (void)cls; if (!g_jni_mavlink_bridge) return -1; return (jint)nimcp_mavlink_takeoff(g_jni_mavlink_bridge, alt); }
JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeMavlinkLand(JNIEnv *env, jclass cls) { (void)env; (void)cls; if (!g_jni_mavlink_bridge) return -1; return (jint)nimcp_mavlink_land(g_jni_mavlink_bridge); }
JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeMavlinkGoto(JNIEnv *env, jclass cls, jdouble lat, jdouble lon, jfloat alt) { (void)env; (void)cls; if (!g_jni_mavlink_bridge) return -1; return (jint)nimcp_mavlink_goto(g_jni_mavlink_bridge, lat, lon, alt); }
JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeMavlinkRtl(JNIEnv *env, jclass cls) { (void)env; (void)cls; if (!g_jni_mavlink_bridge) return -1; return (jint)nimcp_mavlink_rtl(g_jni_mavlink_bridge); }

JNIEXPORT jfloatArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeMavlinkComposeFeatures(
    JNIEnv *env, jclass cls)
{
    (void)cls;
    if (!g_jni_mavlink_bridge) return (*env)->NewFloatArray(env, 0);
    float features[NIMCP_MAVLINK_FEATURE_COUNT] = {0};
    int count = nimcp_mavlink_compose_features(g_jni_mavlink_bridge, features, NIMCP_MAVLINK_FEATURE_COUNT);
    if (count < 0) count = 0;
    jfloatArray arr = (*env)->NewFloatArray(env, (jsize)count);
    if (arr && count > 0) (*env)->SetFloatArrayRegion(env, arr, 0, (jsize)count, features);
    return arr;
}

// 7. memory_store_stats() -> Map
JNIEXPORT jobject JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeMemoryStoreStats(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain || !brain->internal_brain || !brain->internal_brain->memory_store)
        return NULL;

    nimcp_memory_store_stats_t stats = {0};
    nimcp_memory_store_get_stats(
        (nimcp_memory_store_t*)brain->internal_brain->memory_store, &stats);

    jclass map_cls; jmethodID put;
    jobject map = jni_create_hashmap(env, &map_cls, &put);
    if (!map) return NULL;
    jni_map_put_long(env, map, put, "total_engrams", (int64_t)stats.total_engrams);
    jni_map_put_long(env, map, put, "total_concepts", (int64_t)stats.total_concepts);
    jni_map_put_long(env, map, put, "total_relations", (int64_t)stats.total_relations);
    jni_map_put_long(env, map, put, "total_autobio", (int64_t)stats.total_autobio);
    jni_map_put_long(env, map, put, "total_writes", (int64_t)stats.total_writes);
    jni_map_put_long(env, map, put, "total_reads", (int64_t)stats.total_reads);
    jni_map_put_long(env, map, put, "cache_hits", (int64_t)stats.cache_hits);
    jni_map_put_long(env, map, put, "cache_misses", (int64_t)stats.cache_misses);
    jni_map_put_long(env, map, put, "db_size_bytes", (int64_t)stats.db_size_bytes);
    return map;
}

// 8. memory_search_text(query, max_results) -> long[]
JNIEXPORT jlongArray JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeMemorySearchText(
    JNIEnv *env, jclass cls, jlong h, jstring query, jint max_results)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain || !brain->internal_brain || !brain->internal_brain->memory_store) {
        return (*env)->NewLongArray(env, 0);
    }

    const char *q = (*env)->GetStringUTFChars(env, query, NULL);
    if (!q) return (*env)->NewLongArray(env, 0);

    nimcp_memory_search_result_t *res = nimcp_memory_store_engram_search_text(
        (nimcp_memory_store_t*)brain->internal_brain->memory_store,
        q, (uint32_t)max_results);
    (*env)->ReleaseStringUTFChars(env, query, q);

    if (!res || res->count == 0) {
        if (res) nimcp_memory_search_result_destroy(res);
        return (*env)->NewLongArray(env, 0);
    }

    jlongArray arr = (*env)->NewLongArray(env, (jsize)res->count);
    if (arr) {
        jlong *buf = (jlong*)malloc(res->count * sizeof(jlong));
        if (buf) {
            for (uint32_t i = 0; i < res->count; i++) buf[i] = (jlong)res->ids[i];
            (*env)->SetLongArrayRegion(env, arr, 0, (jsize)res->count, buf);
            free(buf);
        }
    }
    nimcp_memory_search_result_destroy(res);
    return arr;
}

// 9. memory_search_similar(embedding, top_k) -> float[][] (pairs of [id, distance])
JNIEXPORT jobject JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeMemorySearchSimilar(
    JNIEnv *env, jclass cls, jlong h, jfloatArray embedding, jint top_k)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;

    /* Create empty ArrayList for return */
    jclass list_cls = (*env)->FindClass(env, "java/util/ArrayList");
    jmethodID list_init = (*env)->GetMethodID(env, list_cls, "<init>", "()V");
    jmethodID list_add = (*env)->GetMethodID(env, list_cls, "add", "(Ljava/lang/Object;)Z");
    jobject list = (*env)->NewObject(env, list_cls, list_init);

    if (!brain || !brain->internal_brain || !brain->internal_brain->memory_store)
        return list;

    jsize dim = (*env)->GetArrayLength(env, embedding);
    float *emb = (float*)malloc(dim * sizeof(float));
    if (!emb) return list;
    (*env)->GetFloatArrayRegion(env, embedding, 0, dim, emb);

    nimcp_memory_search_result_t *res = nimcp_memory_store_engram_search_similar(
        (nimcp_memory_store_t*)brain->internal_brain->memory_store,
        emb, (uint32_t)dim, (uint32_t)top_k, 0.0f);
    free(emb);

    if (res) {
        for (uint32_t i = 0; i < res->count; i++) {
            /* Each entry is a HashMap with id + distance */
            jclass mc; jmethodID mp;
            jobject entry = jni_create_hashmap(env, &mc, &mp);
            if (entry) {
                jni_map_put_long(env, entry, mp, "id", (int64_t)res->ids[i]);
                jni_map_put_float(env, entry, mp, "distance", res->distances[i]);
                (*env)->CallBooleanMethod(env, list, list_add, entry);
                (*env)->DeleteLocalRef(env, entry);
            }
        }
        nimcp_memory_search_result_destroy(res);
    }
    return list;
}

// 10. memory_is_healthy() -> boolean
JNIEXPORT jboolean JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeMemoryIsHealthy(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)env; (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain || !brain->internal_brain || !brain->internal_brain->memory_store)
        return JNI_TRUE; /* No store = nothing unhealthy */
    return nimcp_memory_store_is_healthy(
        (nimcp_memory_store_t*)brain->internal_brain->memory_store) ? JNI_TRUE : JNI_FALSE;
}

// 11. ood_stats() -> Map
JNIEXPORT jobject JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeOodStats(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain || !brain->internal_brain || !brain->internal_brain->ood_detector)
        return NULL;

    nimcp_ood_stats_t stats = {0};
    nimcp_ood_get_stats(
        (const nimcp_ood_detector_t*)brain->internal_brain->ood_detector, &stats);

    jclass map_cls; jmethodID put;
    jobject map = jni_create_hashmap(env, &map_cls, &put);
    if (!map) return NULL;
    jni_map_put_long(env, map, put, "total_checks", (int64_t)stats.total_checks);
    jni_map_put_long(env, map, put, "ood_detected", (int64_t)stats.ood_detected);
    jni_map_put_long(env, map, put, "in_distribution", (int64_t)stats.in_distribution);
    jni_map_put_float(env, map, put, "avg_ood_score", stats.avg_ood_score);
    jni_map_put_float(env, map, put, "ood_rate", stats.ood_rate);
    return map;
}

// 12. audit_log(description, severity, details) -> int
JNIEXPORT jint JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeAuditLog(
    JNIEnv *env, jclass cls, jlong h, jstring description, jint severity, jstring details)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain || !brain->internal_brain || !brain->internal_brain->memory_store)
        return -1;

    const char *desc = (*env)->GetStringUTFChars(env, description, NULL);
    const char *det = details ? (*env)->GetStringUTFChars(env, details, NULL) : "";

    nimcp_memory_audit_event_t event = {0};
    event.timestamp_us = nimcp_time_get_us();
    event.event_type = (uint32_t)severity;
    if (desc) strncpy(event.description, desc, sizeof(event.description) - 1);
    if (det) strncpy(event.details, det, sizeof(event.details) - 1);

    int rc = nimcp_memory_store_audit_log(
        (nimcp_memory_store_t*)brain->internal_brain->memory_store, &event);

    if (desc) (*env)->ReleaseStringUTFChars(env, description, desc);
    if (details && det) (*env)->ReleaseStringUTFChars(env, details, det);
    return (jint)rc;
}

// 13. audit_search(min_severity, max_results) -> List<Map>
JNIEXPORT jobject JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeAuditSearch(
    JNIEnv *env, jclass cls, jlong h, jint min_severity, jint max_results)
{
    (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;

    jclass list_cls = (*env)->FindClass(env, "java/util/ArrayList");
    jmethodID list_init = (*env)->GetMethodID(env, list_cls, "<init>", "()V");
    jmethodID list_add = (*env)->GetMethodID(env, list_cls, "add", "(Ljava/lang/Object;)Z");
    jobject list = (*env)->NewObject(env, list_cls, list_init);

    if (!brain || !brain->internal_brain || !brain->internal_brain->memory_store)
        return list;

    nimcp_memory_search_result_t *res = nimcp_memory_store_audit_search(
        (nimcp_memory_store_t*)brain->internal_brain->memory_store,
        (uint32_t)min_severity, 0, UINT64_MAX, (uint32_t)max_results);

    if (res) {
        for (uint32_t i = 0; i < res->count; i++) {
            jclass mc; jmethodID mp;
            jobject entry = jni_create_hashmap(env, &mc, &mp);
            if (entry) {
                jni_map_put_long(env, entry, mp, "id", (int64_t)res->ids[i]);
                jni_map_put_float(env, entry, mp, "severity", res->distances[i]);
                (*env)->CallBooleanMethod(env, list, list_add, entry);
                (*env)->DeleteLocalRef(env, entry);
            }
        }
        nimcp_memory_search_result_destroy(res);
    }
    return list;
}

// ============================================================================
// Per-network training toggles (runtime-dynamic, no rebuild required)
// ============================================================================
//
// Expose brain->config.train_ann/cnn/snn/lnn ablation flags plus the
// snn_only_recovery convenience preset and the ensemble_warmup_scale
// gradual ramp used by the daemon plateau detector. The brain reads
// these on every brain_learn_vector call so flips take effect
// immediately with no rebuild or restart required.

#define NIMCP_JNI_TRAIN_TOGGLE(Name, lower) \
JNIEXPORT void JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSetTrain##Name( \
    JNIEnv *env, jclass cls, jlong h, jboolean enabled) { \
    (void)env; (void)cls; \
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h; \
    if (!brain) return; \
    nimcp_brain_set_train_##lower(brain, (bool)enabled); \
} \
JNIEXPORT jboolean JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeGetTrain##Name( \
    JNIEnv *env, jclass cls, jlong h) { \
    (void)env; (void)cls; \
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h; \
    if (!brain) return JNI_FALSE; \
    return nimcp_brain_get_train_##lower(brain) ? JNI_TRUE : JNI_FALSE; \
}

NIMCP_JNI_TRAIN_TOGGLE(Ann, ann)
NIMCP_JNI_TRAIN_TOGGLE(Cnn, cnn)
NIMCP_JNI_TRAIN_TOGGLE(Snn, snn)
NIMCP_JNI_TRAIN_TOGGLE(Lnn, lnn)

#undef NIMCP_JNI_TRAIN_TOGGLE

JNIEXPORT void JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSetSnnOnlyRecovery(
    JNIEnv *env, jclass cls, jlong h, jboolean enabled)
{
    (void)env; (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain) return;
    nimcp_brain_set_snn_only_recovery(brain, (bool)enabled);
}

JNIEXPORT jboolean JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeGetSnnOnlyRecovery(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)env; (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain) return JNI_FALSE;
    return nimcp_brain_get_snn_only_recovery(brain) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeSetEnsembleWarmupScale(
    JNIEnv *env, jclass cls, jlong h, jfloat scale)
{
    (void)env; (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain) return;
    nimcp_brain_set_ensemble_warmup_scale(brain, (float)scale);
}

JNIEXPORT jfloat JNICALL Java_com_nimcp_NIMCP_00024Brain_nativeGetEnsembleWarmupScale(
    JNIEnv *env, jclass cls, jlong h)
{
    (void)env; (void)cls;
    nimcp_brain_t brain = (nimcp_brain_t)(uintptr_t)h;
    if (!brain) return 1.0f;
    return (jfloat)nimcp_brain_get_ensemble_warmup_scale(brain);
}
