/**
 * @file NIMCP.java
 * @brief Complete Java bindings for NIMCP via JNI
 * @version 2.6.3
 *
 * Wraps the entire nimcp.h public C API with idiomatic Java classes.
 * Uses AutoCloseable for deterministic resource cleanup,
 * typed exceptions, and enum mappings.
 *
 * Usage:
 *   NIMCP.init();
 *   try (NIMCP.Brain brain = new NIMCP.Brain("test", NIMCP.BrainSize.TINY,
 *            NIMCP.TaskType.CLASSIFICATION, 4, 2)) {
 *       brain.learn(new float[]{1,0,0.5f,0.3f}, "cat", 0.9f);
 *       NIMCP.Prediction p = brain.predict(new float[]{1,0,0.5f,0.3f});
 *   }
 *   NIMCP.shutdown();
 */
package com.nimcp;

public class NIMCP {

    static {
        System.loadLibrary("nimcp_jni");
    }

    // ========================================================================
    // Exception Hierarchy
    // ========================================================================

    public static class NIMCPException extends Exception {
        private final int code;
        public NIMCPException(String message) { this(0, message); }
        public NIMCPException(int code, String message) {
            super(message);
            this.code = code;
        }
        public int getCode() { return code; }
    }

    public static class NullArgException extends NIMCPException {
        public NullArgException(String msg) { super(1003, msg); }
    }

    public static class InvalidException extends NIMCPException {
        public InvalidException(String msg) { super(1004, msg); }
    }

    public static class MemoryException extends NIMCPException {
        public MemoryException(String msg) { super(2000, msg); }
    }

    public static class IOExceptionNIMCP extends NIMCPException {
        public IOExceptionNIMCP(String msg) { super(4000, msg); }
    }

    // ========================================================================
    // Enums
    // ========================================================================

    public enum BrainSize {
        TINY(0), SMALL(1), MEDIUM(2), LARGE(3);
        public final int value;
        BrainSize(int v) { this.value = v; }
        public static BrainSize fromInt(int v) {
            for (BrainSize s : values()) if (s.value == v) return s;
            return TINY;
        }
    }

    public enum TaskType {
        CLASSIFICATION(0), REGRESSION(1), PATTERN_MATCHING(2),
        SEQUENCE(3), ASSOCIATION(4);
        public final int value;
        TaskType(int v) { this.value = v; }
        public static TaskType fromInt(int v) {
            for (TaskType t : values()) if (t.value == v) return t;
            return CLASSIFICATION;
        }
    }

    public enum NetworkType {
        ADAPTIVE(0), SNN(1), LNN(2), CNN(3), HYBRID(4);
        public final int value;
        NetworkType(int v) { this.value = v; }
    }

    public enum SNNTrainMethod {
        STDP(0), R_STDP(1), EPROP(2), SURROGATE(3), HOMEOSTATIC(4);
        public final int value;
        SNNTrainMethod(int v) { this.value = v; }
    }

    public enum LNNTrainMethod {
        ADJOINT(0), BPTT(1), RTRL(2), EPROP(3);
        public final int value;
        LNNTrainMethod(int v) { this.value = v; }
    }

    public enum LossType {
        MSE(0), CROSS_ENTROPY(1), BINARY_CE(2), HUBER(3),
        MAE(4), FOCAL(5), KL_DIV(6);
        public final int value;
        LossType(int v) { this.value = v; }
    }

    public enum OptimizerType {
        SGD(0), MOMENTUM(1), ADAM(2), ADAMW(3), RMSPROP(4), ADAGRAD(5);
        public final int value;
        OptimizerType(int v) { this.value = v; }
    }

    public enum SchedulerType {
        CONSTANT(0), STEP(1), EXPONENTIAL(2), COSINE(3),
        WARMUP_COSINE(4), REDUCE_ON_PLATEAU(5), CYCLIC(6);
        public final int value;
        SchedulerType(int v) { this.value = v; }
    }

    public enum CallbackEvent {
        STEP_COMPLETE(0), EPOCH_COMPLETE(1), LOSS_COMPUTED(2),
        WEIGHTS_UPDATED(3), LR_CHANGED(4), CONVERGENCE(5),
        DIVERGENCE(6), CHECKPOINT(7);
        public final int value;
        CallbackEvent(int v) { this.value = v; }
        public static CallbackEvent fromInt(int v) {
            for (CallbackEvent e : values()) if (e.value == v) return e;
            return STEP_COMPLETE;
        }
    }

    public enum CallbackAction {
        CONTINUE(0), STOP(1), SKIP(2), ROLLBACK(3),
        REDUCE_LR(4), INCREASE_LR(5);
        public final int value;
        CallbackAction(int v) { this.value = v; }
    }

    public enum CognitiveModule {
        NONE(0), PERCEPTION(1), WORKING_MEMORY(2), EXECUTIVE(3),
        THEORY_OF_MIND(4), ETHICS(5), ATTENTION(6), EMOTION(7),
        SALIENCE(8), MOTOR(9), LANGUAGE(10), METACOGNITION(11),
        CURIOSITY(12), INTROSPECTION(13), PREDICTIVE(14),
        CONSOLIDATION(15), EPISODIC_MEMORY(16), SEMANTIC_MEMORY(17),
        WELLBEING(18), MENTAL_HEALTH(19), GOAL_MOTIVATION(20),
        COGNITIVE_CONTROL(21), CUSTOM_START(100);
        public final int value;
        CognitiveModule(int v) { this.value = v; }
        public static CognitiveModule fromInt(int v) {
            for (CognitiveModule m : values()) if (m.value == v) return m;
            return NONE;
        }
    }

    // ========================================================================
    // Data Classes
    // ========================================================================

    public static class Prediction {
        public final String label;
        public final float confidence;
        public Prediction(String label, float confidence) {
            this.label = label;
            this.confidence = confidence;
        }
    }

    public static class TrainingConfig {
        public LossType lossType = LossType.CROSS_ENTROPY;
        public OptimizerType optimizerType = OptimizerType.ADAM;
        public SchedulerType schedulerType = SchedulerType.COSINE;
        public float learningRate = 0.001f;
        public float weightDecay = 0.0f;
        public float momentum = 0.9f;
        public float beta1 = 0.9f;
        public float beta2 = 0.999f;
        public float epsilon = 1e-8f;
        public int schedulerStepSize = 100;
        public float schedulerGamma = 0.1f;
        public int warmupSteps = 0;
        public boolean enableGradientClipping = false;
        public float gradientClipValue = 1.0f;
        public boolean enableBiologicalModulation = true;
        public float biologicalBlend = 0.5f;
        public NetworkType networkType = NetworkType.ADAPTIVE;
        public SNNTrainMethod snnMethod = SNNTrainMethod.STDP;
        public float snnEligibilityTau = 20.0f;
        public float snnRewardTau = 100.0f;
        public float snnSurrogateBeta = 5.0f;
        public LNNTrainMethod lnnMethod = LNNTrainMethod.ADJOINT;
        public int lnnBpttTruncation = 100;
        public boolean lnnUseAdjointCheckpointing = true;
    }

    public static class TrainingResult {
        public float loss;
        public float learningRate;
        public int step;
        public boolean earlyStopped;
        public float gradientNorm;
    }

    public static class CallbackConfig {
        public boolean enableAutoCheckpoint = false;
        public int checkpointInterval = 100;
        public boolean enableEarlyStopping = false;
        public int patience = 10;
        public float minDelta = 0.0001f;
        public float divergenceThreshold = 10.0f;
        public int logInterval = 0;
    }

    public static class CallbackMetrics {
        public long step;
        public long epoch;
        public float loss;
        public float lossEma;
        public float learningRate;
        public float gradientNorm;
        public long stepTimeUs;
        public boolean isConverging;
        public boolean isDiverging;
    }

    public static class SnapshotInfo {
        public String name;
        public String description;
        public long timestamp;
        public int fileSize;
        public boolean isCompressed;
        public boolean isEncrypted;
    }

    public static class BrainProbe {
        public String taskName;
        public BrainSize size;
        public TaskType task;
        public int numNeurons;
        public int numSynapses;
        public int numActiveSynapses;
        public long totalInferences;
        public long totalLearningSteps;
        public float avgSparsity;
        public float avgInferenceTimeUs;
        public float currentLearningRate;
        public float accuracy;
        public long memoryBytes;
        public int numInputs;
        public int numOutputs;
        public boolean isCowClone;
        public int cowRefCount;
        public long cowSharedBytes;
        public long cowPrivateBytes;
    }

    public static class Phasor {
        public final float amplitude;
        public final float phase;
        public Phasor(float amplitude, float phase) {
            this.amplitude = amplitude;
            this.phase = phase;
        }
    }

    public static class TrainingStats {
        public long totalSteps;
        public float totalLoss;
        public float currentLr;
    }

    public static class UtilizationMetrics {
        public float utilization;
        public float saturation;
    }

    public static class WorkspaceReadResult {
        public float[] content;
        public int actualDim;
        public CognitiveModule sourceModule;
    }

    public static class WorkspaceStats {
        public int totalBroadcasts;
        public int totalCompetitions;
        public float avgStrength;
    }

    public static class CallbackStats {
        public long totalFired;
        public float avgTimeUs;
        public int earlyStops;
    }

    public static class WorkingMemoryStats {
        public int currentSize;
        public int capacity;
    }

    // ========================================================================
    // Callback Interface
    // ========================================================================

    @FunctionalInterface
    public interface TrainingCallback {
        CallbackAction onEvent(CallbackEvent event, CallbackMetrics metrics);
    }

    // ========================================================================
    // Library Lifecycle (static)
    // ========================================================================

    private static native int nativeInit();
    private static native void nativeShutdown();
    private static native String nativeVersion();
    private static native int nativeVersionInt();

    public static void init() throws NIMCPException {
        int rc = nativeInit();
        if (rc != 0) throw new NIMCPException(rc, "Failed to initialize NIMCP");
    }

    public static void shutdown() { nativeShutdown(); }
    public static String version() { return nativeVersion(); }
    public static int versionInt() { return nativeVersionInt(); }

    // ========================================================================
    // Brain Class
    // ========================================================================

    public static class Brain implements AutoCloseable {
        private long handle;

        // --- Native methods ---
        private static native long nativeCreate(String name, int size, int task,
                                                int numInputs, int numOutputs);
        private static native void nativeDestroy(long h);
        private static native int nativeLearn(long h, float[] features,
                                              String label, float confidence);
        private static native String nativePredict(long h, float[] features,
                                                   float[] outConf);
        private static native int nativeInfer(long h, float[] features,
                                              float[] outputs);
        private static native int nativeSave(long h, String path);
        private static native long nativeLoad(String path);
        private static native long nativeCreateFromConfig(String path);

        // Training
        private static native int nativeConfigureTraining(long h, int loss, int opt,
            int sched, float lr, float wd, float mom, float b1, float b2, float eps,
            int schedStep, float schedGamma, int warmup, boolean gradClip,
            float gradClipVal, boolean bioMod, float bioBlend, int netType,
            int snnMethod, float snnEligTau, float snnRewTau, float snnSurrBeta,
            int lnnMethod, int lnnBptt, boolean lnnAdjCheck);
        private static native float[] nativeTrainStep(long h, float[] features,
                                                      float[] targets);
        private static native float[] nativeTrainBatch(long h, float[] features,
            float[] targets, int batchSize, int numFeatures, int numTargets);
        private static native float[] nativeGetTrainingStats(long h);
        private static native float nativeStepScheduler(long h, float valMetric);

        // Callbacks
        private static native int nativeEnableCallbacks(long h, boolean autoCP,
            int cpInterval, boolean earlyStop, int patience, float minDelta,
            float divThresh, int logInterval);
        private static native int nativeDisableCallbacks(long h);
        private static native int nativeRegisterCallback(long h, int event,
            TrainingCallback callback, String name);
        private static native int nativeUnregisterCallback(long h, int cbId);
        private static native float[] nativeGetCallbackStats(long h);

        // Resize
        private static native boolean nativeResize(long h, int count);
        private static native boolean nativeAutoResize(long h);
        private static native int nativeGetNeuronCount(long h);
        private static native float[] nativeGetUtilizationMetrics(long h);

        // Per-network training toggles (runtime-dynamic, no rebuild required)
        private static native void nativeSetTrainAnn(long h, boolean enabled);
        private static native boolean nativeGetTrainAnn(long h);
        private static native void nativeSetTrainCnn(long h, boolean enabled);
        private static native boolean nativeGetTrainCnn(long h);
        private static native void nativeSetTrainSnn(long h, boolean enabled);
        private static native boolean nativeGetTrainSnn(long h);
        private static native void nativeSetTrainLnn(long h, boolean enabled);
        private static native boolean nativeGetTrainLnn(long h);
        private static native void nativeSetSnnOnlyRecovery(long h, boolean enabled);
        private static native boolean nativeGetSnnOnlyRecovery(long h);
        private static native void nativeSetEnsembleWarmupScale(long h, float scale);
        private static native float nativeGetEnsembleWarmupScale(long h);

        // Named snapshots
        private static native int nativeSnapshotSave(long h, String name, String desc);
        private static native long nativeSnapshotRestore(long h, String name);
        private static native String[] nativeSnapshotList(long h, int maxCount);
        private static native int nativeSnapshotDelete(long h, String name);

        // COW
        private static native long nativeCloneCow(long h);
        private static native long nativeSnapshotCow(long h);
        private static native int nativeRestoreCow(long h, long snapH);
        private static native void nativeSnapshotCowDestroy(long snapH);

        // Working memory
        private static native int nativeWorkingMemoryAdd(long h, float[] data,
                                                         float salience);
        private static native float[] nativeWorkingMemoryGet(long h, int index);
        private static native int[] nativeWorkingMemoryStats(long h);
        private static native int nativeWorkingMemoryRefresh(long h, int index);

        // Workspace
        private static native int nativeWorkspaceCompete(long h, int module,
            float[] content, float strength);
        private static native float[] nativeWorkspaceRead(long h, int maxDim,
            int[] outMeta);
        private static native int nativeWorkspaceSubscribe(long h, int module);
        private static native int nativeWorkspaceUnsubscribe(long h, int module);
        private static native int nativeWorkspaceHasBroadcast(long h);
        private static native float[] nativeWorkspaceStats(long h);

        // Oscillations
        private static native boolean nativeEnableOscillations(long h, boolean en);
        private static native boolean nativeIsOscillationsEnabled(long h);
        private static native float[] nativeGetPhasor(long h, int neuronId);
        private static native float nativeGetPhaseCoherence(long h, int[] neuronIds);
        private static native float nativeGetPacModulation(long h, float theta,
                                                           float gamma);

        // Probe
        private static native float[] nativeProbe(long h, String[] outStrings);
        private static native int nativeBroadcastProbe(long h);

        // --- Constructors ---
        public Brain(String name, BrainSize size, TaskType task,
                     int numInputs, int numOutputs) throws NIMCPException {
            this.handle = nativeCreate(name, size.value, task.value,
                                       numInputs, numOutputs);
            if (this.handle == 0)
                throw new NIMCPException("Failed to create brain");
        }

        private Brain(long handle) { this.handle = handle; }

        public static Brain load(String filepath) throws NIMCPException {
            long h = nativeLoad(filepath);
            if (h == 0) throw new IOExceptionNIMCP("Failed to load brain: " + filepath);
            return new Brain(h);
        }

        public static Brain createFromConfig(String filepath) throws NIMCPException {
            long h = nativeCreateFromConfig(filepath);
            if (h == 0) throw new NIMCPException("Failed to create brain from config");
            return new Brain(h);
        }

        @Override
        public void close() {
            if (handle != 0) {
                nativeDestroy(handle);
                handle = 0;
            }
        }

        private void checkOpen() throws NIMCPException {
            if (handle == 0) throw new NIMCPException("Brain is closed");
        }

        // --- Core operations ---

        public void learn(float[] features, String label,
                          float confidence) throws NIMCPException {
            checkOpen();
            int rc = nativeLearn(handle, features, label, confidence);
            if (rc != 0) throw new NIMCPException(rc, "learn failed");
        }

        public Prediction predict(float[] features) throws NIMCPException {
            checkOpen();
            float[] conf = new float[1];
            String label = nativePredict(handle, features, conf);
            if (label == null) throw new NIMCPException("predict failed");
            return new Prediction(label, conf[0]);
        }

        public void infer(float[] features, float[] outputs) throws NIMCPException {
            checkOpen();
            int rc = nativeInfer(handle, features, outputs);
            if (rc != 0) throw new NIMCPException(rc, "infer failed");
        }

        public void save(String filepath) throws NIMCPException {
            checkOpen();
            int rc = nativeSave(handle, filepath);
            if (rc != 0) throw new IOExceptionNIMCP("save failed: " + filepath);
        }

        // --- Training Pipeline ---

        public void configureTraining(TrainingConfig cfg) throws NIMCPException {
            checkOpen();
            int rc = nativeConfigureTraining(handle,
                cfg.lossType.value, cfg.optimizerType.value,
                cfg.schedulerType.value, cfg.learningRate, cfg.weightDecay,
                cfg.momentum, cfg.beta1, cfg.beta2, cfg.epsilon,
                cfg.schedulerStepSize, cfg.schedulerGamma, cfg.warmupSteps,
                cfg.enableGradientClipping, cfg.gradientClipValue,
                cfg.enableBiologicalModulation, cfg.biologicalBlend,
                cfg.networkType.value, cfg.snnMethod.value,
                cfg.snnEligibilityTau, cfg.snnRewardTau, cfg.snnSurrogateBeta,
                cfg.lnnMethod.value, cfg.lnnBpttTruncation,
                cfg.lnnUseAdjointCheckpointing);
            if (rc != 0) throw new NIMCPException(rc, "configureTraining failed");
        }

        public TrainingResult trainStep(float[] features,
                                        float[] targets) throws NIMCPException {
            checkOpen();
            float[] r = nativeTrainStep(handle, features, targets);
            if (r == null) throw new NIMCPException("trainStep failed");
            TrainingResult res = new TrainingResult();
            res.loss = r[0]; res.learningRate = r[1]; res.step = (int)r[2];
            res.earlyStopped = r[3] != 0.0f; res.gradientNorm = r[4];
            return res;
        }

        public TrainingResult trainBatch(float[] features, float[] targets,
                int batchSize, int numFeatures,
                int numTargets) throws NIMCPException {
            checkOpen();
            float[] r = nativeTrainBatch(handle, features, targets,
                                         batchSize, numFeatures, numTargets);
            if (r == null) throw new NIMCPException("trainBatch failed");
            TrainingResult res = new TrainingResult();
            res.loss = r[0]; res.learningRate = r[1]; res.step = (int)r[2];
            res.earlyStopped = r[3] != 0.0f; res.gradientNorm = r[4];
            return res;
        }

        public TrainingStats getTrainingStats() throws NIMCPException {
            checkOpen();
            float[] r = nativeGetTrainingStats(handle);
            if (r == null) throw new NIMCPException("getTrainingStats failed");
            TrainingStats s = new TrainingStats();
            s.totalSteps = (long)r[0]; s.totalLoss = r[1]; s.currentLr = r[2];
            return s;
        }

        public float stepScheduler(float validationMetric) throws NIMCPException {
            checkOpen();
            return nativeStepScheduler(handle, validationMetric);
        }

        // --- Callbacks ---

        public void enableCallbacks(CallbackConfig cfg) throws NIMCPException {
            checkOpen();
            int rc = nativeEnableCallbacks(handle, cfg.enableAutoCheckpoint,
                cfg.checkpointInterval, cfg.enableEarlyStopping, cfg.patience,
                cfg.minDelta, cfg.divergenceThreshold, cfg.logInterval);
            if (rc != 0) throw new NIMCPException(rc, "enableCallbacks failed");
        }

        public void disableCallbacks() throws NIMCPException {
            checkOpen();
            int rc = nativeDisableCallbacks(handle);
            if (rc != 0) throw new NIMCPException(rc, "disableCallbacks failed");
        }

        public int registerCallback(CallbackEvent event,
                TrainingCallback callback, String name) throws NIMCPException {
            checkOpen();
            int id = nativeRegisterCallback(handle, event.value, callback, name);
            if (id == 0) throw new NIMCPException("registerCallback failed");
            return id;
        }

        public void unregisterCallback(int callbackId) throws NIMCPException {
            checkOpen();
            int rc = nativeUnregisterCallback(handle, callbackId);
            if (rc != 0) throw new NIMCPException(rc, "unregisterCallback failed");
        }

        public CallbackStats getCallbackStats() throws NIMCPException {
            checkOpen();
            float[] r = nativeGetCallbackStats(handle);
            if (r == null) throw new NIMCPException("getCallbackStats failed");
            CallbackStats s = new CallbackStats();
            s.totalFired = (long)r[0]; s.avgTimeUs = r[1];
            s.earlyStops = (int)r[2];
            return s;
        }

        // --- Resize ---

        public boolean resize(int neuronCount) throws NIMCPException {
            checkOpen();
            return nativeResize(handle, neuronCount);
        }

        public boolean autoResize() throws NIMCPException {
            checkOpen();
            return nativeAutoResize(handle);
        }

        public int getNeuronCount() throws NIMCPException {
            checkOpen();
            return nativeGetNeuronCount(handle);
        }

        // --- Per-network training toggles (runtime-dynamic, no rebuild required) ---
        //
        // Flip any subset of the four sub-networks' training on or off
        // at runtime. The brain reads these flags on every learn call,
        // so changes take effect immediately. Useful for ablation
        // studies, SNN-only recovery, and round-robin training.

        public void setTrainAnn(boolean enabled) throws NIMCPException {
            checkOpen(); nativeSetTrainAnn(handle, enabled);
        }
        public boolean getTrainAnn() throws NIMCPException {
            checkOpen(); return nativeGetTrainAnn(handle);
        }

        public void setTrainCnn(boolean enabled) throws NIMCPException {
            checkOpen(); nativeSetTrainCnn(handle, enabled);
        }
        public boolean getTrainCnn() throws NIMCPException {
            checkOpen(); return nativeGetTrainCnn(handle);
        }

        public void setTrainSnn(boolean enabled) throws NIMCPException {
            checkOpen(); nativeSetTrainSnn(handle, enabled);
        }
        public boolean getTrainSnn() throws NIMCPException {
            checkOpen(); return nativeGetTrainSnn(handle);
        }

        public void setTrainLnn(boolean enabled) throws NIMCPException {
            checkOpen(); nativeSetTrainLnn(handle, enabled);
        }
        public boolean getTrainLnn() throws NIMCPException {
            checkOpen(); return nativeGetTrainLnn(handle);
        }

        /**
         * Convenience preset: freeze ANN/CNN/LNN while keeping SNN
         * training active. Used to let the SNN re-converge against a
         * stable ensemble after large BPTT behavior changes.
         */
        public void setSnnOnlyRecovery(boolean enabled) throws NIMCPException {
            checkOpen(); nativeSetSnnOnlyRecovery(handle, enabled);
        }
        public boolean getSnnOnlyRecovery() throws NIMCPException {
            checkOpen(); return nativeGetSnnOnlyRecovery(handle);
        }

        /**
         * Probabilistic gate on non-SNN training updates [0.0, 1.0].
         * 1.0 = full-rate (default), 0.0 = fully frozen, intermediate =
         * Monte-Carlo skip. Used by the daemon plateau detector to ramp
         * ANN/CNN/LNN back in gradually after SNN-only recovery.
         * Clamped to [0.0, 1.0] on the native side.
         */
        public void setEnsembleWarmupScale(float scale) throws NIMCPException {
            checkOpen(); nativeSetEnsembleWarmupScale(handle, scale);
        }
        public float getEnsembleWarmupScale() throws NIMCPException {
            checkOpen(); return nativeGetEnsembleWarmupScale(handle);
        }

        public UtilizationMetrics getUtilizationMetrics() throws NIMCPException {
            checkOpen();
            float[] r = nativeGetUtilizationMetrics(handle);
            if (r == null) throw new NIMCPException("getUtilizationMetrics failed");
            UtilizationMetrics m = new UtilizationMetrics();
            m.utilization = r[0]; m.saturation = r[1];
            return m;
        }

        // --- Named Snapshots ---

        public void snapshotSave(String name,
                                 String description) throws NIMCPException {
            checkOpen();
            int rc = nativeSnapshotSave(handle, name, description);
            if (rc != 0) throw new NIMCPException(rc, "snapshotSave failed");
        }

        public Brain snapshotRestore(String name) throws NIMCPException {
            checkOpen();
            long h = nativeSnapshotRestore(handle, name);
            if (h == 0) throw new NIMCPException("snapshotRestore failed");
            return new Brain(h);
        }

        public SnapshotInfo[] snapshotList(int maxCount) throws NIMCPException {
            checkOpen();
            String[] raw = nativeSnapshotList(handle, maxCount);
            if (raw == null) return new SnapshotInfo[0];
            // Each entry: "name\tdescription\ttimestamp\tfileSize\tcompressed\tencrypted"
            SnapshotInfo[] infos = new SnapshotInfo[raw.length];
            for (int i = 0; i < raw.length; i++) {
                String[] parts = raw[i].split("\t", -1);
                SnapshotInfo info = new SnapshotInfo();
                info.name = parts.length > 0 ? parts[0] : "";
                info.description = parts.length > 1 ? parts[1] : "";
                info.timestamp = parts.length > 2 ? Long.parseLong(parts[2]) : 0;
                info.fileSize = parts.length > 3 ? Integer.parseInt(parts[3]) : 0;
                info.isCompressed = parts.length > 4 && "1".equals(parts[4]);
                info.isEncrypted = parts.length > 5 && "1".equals(parts[5]);
                infos[i] = info;
            }
            return infos;
        }

        public void snapshotDelete(String name) throws NIMCPException {
            checkOpen();
            int rc = nativeSnapshotDelete(handle, name);
            if (rc != 0) throw new NIMCPException(rc, "snapshotDelete failed");
        }

        // --- COW (Copy-on-Write) ---

        public Brain cloneCow() throws NIMCPException {
            checkOpen();
            long h = nativeCloneCow(handle);
            if (h == 0) throw new NIMCPException("cloneCow failed");
            return new Brain(h);
        }

        public BrainSnapshot snapshotCow() throws NIMCPException {
            checkOpen();
            long h = nativeSnapshotCow(handle);
            if (h == 0) throw new NIMCPException("snapshotCow failed");
            return new BrainSnapshot(h);
        }

        public void restoreCow(BrainSnapshot snapshot) throws NIMCPException {
            checkOpen();
            int rc = nativeRestoreCow(handle, snapshot.handle);
            if (rc != 0) throw new NIMCPException(rc, "restoreCow failed");
        }

        // --- Working Memory ---

        public void workingMemoryAdd(float[] data,
                                     float salience) throws NIMCPException {
            checkOpen();
            int rc = nativeWorkingMemoryAdd(handle, data, salience);
            if (rc != 0) throw new NIMCPException(rc, "workingMemoryAdd failed");
        }

        public float[] workingMemoryGet(int index) throws NIMCPException {
            checkOpen();
            return nativeWorkingMemoryGet(handle, index);
        }

        public WorkingMemoryStats workingMemoryStats() throws NIMCPException {
            checkOpen();
            int[] r = nativeWorkingMemoryStats(handle);
            if (r == null) throw new NIMCPException("workingMemoryStats failed");
            WorkingMemoryStats s = new WorkingMemoryStats();
            s.currentSize = r[0]; s.capacity = r[1];
            return s;
        }

        public void workingMemoryRefresh(int index) throws NIMCPException {
            checkOpen();
            int rc = nativeWorkingMemoryRefresh(handle, index);
            if (rc != 0) throw new NIMCPException(rc, "workingMemoryRefresh failed");
        }

        // --- Global Workspace ---

        public void workspaceCompete(CognitiveModule module, float[] content,
                                     float strength) throws NIMCPException {
            checkOpen();
            int rc = nativeWorkspaceCompete(handle, module.value,
                                            content, strength);
            if (rc != 0) throw new NIMCPException(rc, "workspaceCompete failed");
        }

        public WorkspaceReadResult workspaceRead(
                int maxDim) throws NIMCPException {
            checkOpen();
            int[] meta = new int[2]; // [actualDim, sourceModule]
            float[] content = nativeWorkspaceRead(handle, maxDim, meta);
            if (content == null)
                throw new NIMCPException("workspaceRead failed");
            WorkspaceReadResult r = new WorkspaceReadResult();
            r.content = content;
            r.actualDim = meta[0];
            r.sourceModule = CognitiveModule.fromInt(meta[1]);
            return r;
        }

        public void workspaceSubscribe(
                CognitiveModule module) throws NIMCPException {
            checkOpen();
            int rc = nativeWorkspaceSubscribe(handle, module.value);
            if (rc != 0) throw new NIMCPException(rc, "workspaceSubscribe failed");
        }

        public void workspaceUnsubscribe(
                CognitiveModule module) throws NIMCPException {
            checkOpen();
            int rc = nativeWorkspaceUnsubscribe(handle, module.value);
            if (rc != 0) throw new NIMCPException(rc, "workspaceUnsubscribe failed");
        }

        public boolean workspaceHasBroadcast() throws NIMCPException {
            checkOpen();
            return nativeWorkspaceHasBroadcast(handle) != 0;
        }

        public WorkspaceStats workspaceStats() throws NIMCPException {
            checkOpen();
            float[] r = nativeWorkspaceStats(handle);
            if (r == null) throw new NIMCPException("workspaceStats failed");
            WorkspaceStats s = new WorkspaceStats();
            s.totalBroadcasts = (int)r[0]; s.totalCompetitions = (int)r[1];
            s.avgStrength = r[2];
            return s;
        }

        // --- Oscillations ---

        public boolean enableOscillations(boolean enable) throws NIMCPException {
            checkOpen();
            return nativeEnableOscillations(handle, enable);
        }

        public boolean isOscillationsEnabled() throws NIMCPException {
            checkOpen();
            return nativeIsOscillationsEnabled(handle);
        }

        public Phasor getPhasor(int neuronId) throws NIMCPException {
            checkOpen();
            float[] r = nativeGetPhasor(handle, neuronId);
            if (r == null) return new Phasor(0, 0);
            return new Phasor(r[0], r[1]);
        }

        public float getPhaseCoherence(int[] neuronIds) throws NIMCPException {
            checkOpen();
            return nativeGetPhaseCoherence(handle, neuronIds);
        }

        public float getPacModulation(float thetaFreq,
                                      float gammaFreq) throws NIMCPException {
            checkOpen();
            return nativeGetPacModulation(handle, thetaFreq, gammaFreq);
        }

        // --- Probe ---

        public BrainProbe probe() throws NIMCPException {
            checkOpen();
            String[] outStrings = new String[1]; // [taskName]
            float[] r = nativeProbe(handle, outStrings);
            if (r == null) throw new NIMCPException("probe failed");
            BrainProbe p = new BrainProbe();
            p.taskName = outStrings[0];
            p.size = BrainSize.fromInt((int)r[0]);
            p.task = TaskType.fromInt((int)r[1]);
            p.numNeurons = (int)r[2];
            p.numSynapses = (int)r[3];
            p.numActiveSynapses = (int)r[4];
            p.totalInferences = (long)r[5];
            p.totalLearningSteps = (long)r[6];
            p.avgSparsity = r[7];
            p.avgInferenceTimeUs = r[8];
            p.currentLearningRate = r[9];
            p.accuracy = r[10];
            p.memoryBytes = (long)r[11];
            p.numInputs = (int)r[12];
            p.numOutputs = (int)r[13];
            p.isCowClone = r[14] != 0.0f;
            p.cowRefCount = (int)r[15];
            p.cowSharedBytes = (long)r[16];
            p.cowPrivateBytes = (long)r[17];
            return p;
        }

        public void broadcastProbe() throws NIMCPException {
            checkOpen();
            int rc = nativeBroadcastProbe(handle);
            if (rc != 0) throw new NIMCPException(rc, "broadcastProbe failed");
        }
    }

    // ========================================================================
    // BrainSnapshot (COW)
    // ========================================================================

    public static class BrainSnapshot implements AutoCloseable {
        long handle; // package-private for Brain access

        BrainSnapshot(long handle) { this.handle = handle; }

        @Override
        public void close() {
            if (handle != 0) {
                Brain.nativeSnapshotCowDestroy(handle);
                handle = 0;
            }
        }
    }

    // ========================================================================
    // Network Class
    // ========================================================================

    public static class Network implements AutoCloseable {
        private long handle;

        private static native long nativeCreate(int numInputs, int numOutputs,
                                                int numHidden, float lr);
        private static native void nativeDestroy(long h);
        private static native int nativeForward(long h, float[] inputs,
                                                float[] outputs);
        private static native int nativeTrain(long h, float[] inputs,
                                              float[] targets);

        public Network(int numInputs, int numOutputs, int numHidden,
                       float learningRate) throws NIMCPException {
            this.handle = nativeCreate(numInputs, numOutputs,
                                       numHidden, learningRate);
            if (this.handle == 0)
                throw new NIMCPException("Failed to create network");
        }

        public void forward(float[] inputs,
                            float[] outputs) throws NIMCPException {
            if (handle == 0) throw new NIMCPException("Network is closed");
            int rc = nativeForward(handle, inputs, outputs);
            if (rc != 0) throw new NIMCPException(rc, "forward failed");
        }

        public void train(float[] inputs,
                          float[] targets) throws NIMCPException {
            if (handle == 0) throw new NIMCPException("Network is closed");
            int rc = nativeTrain(handle, inputs, targets);
            if (rc != 0) throw new NIMCPException(rc, "train failed");
        }

        @Override
        public void close() {
            if (handle != 0) {
                nativeDestroy(handle);
                handle = 0;
            }
        }
    }

    // ========================================================================
    // Ethics Class
    // ========================================================================

    public static class Ethics implements AutoCloseable {
        private long handle;

        private static native long nativeCreate();
        private static native void nativeDestroy(long h);
        private static native float nativeCheck(long h, float[] situation);

        public Ethics() throws NIMCPException {
            this.handle = nativeCreate();
            if (this.handle == 0)
                throw new NIMCPException("Failed to create ethics module");
        }

        public float check(float[] situation) throws NIMCPException {
            if (handle == 0) throw new NIMCPException("Ethics is closed");
            return nativeCheck(handle, situation);
        }

        @Override
        public void close() {
            if (handle != 0) {
                nativeDestroy(handle);
                handle = 0;
            }
        }
    }

    // ========================================================================
    // KnowledgeGraph Class
    // ========================================================================

    public static class KnowledgeGraph implements AutoCloseable {
        private long handle;

        private static native long nativeCreate();
        private static native void nativeDestroy(long h);
        private static native int nativeAddFact(long h, String subject,
                                                String predicate, String object);
        private static native String nativeQuery(long h, String query, int maxLen);

        public KnowledgeGraph() throws NIMCPException {
            this.handle = nativeCreate();
            if (this.handle == 0)
                throw new NIMCPException("Failed to create knowledge graph");
        }

        public void addFact(String subject, String predicate,
                            String object) throws NIMCPException {
            if (handle == 0) throw new NIMCPException("KnowledgeGraph is closed");
            int rc = nativeAddFact(handle, subject, predicate, object);
            if (rc != 0) throw new NIMCPException(rc, "addFact failed");
        }

        public String query(String query) throws NIMCPException {
            return query(query, 1024);
        }

        public String query(String query, int maxLen) throws NIMCPException {
            if (handle == 0) throw new NIMCPException("KnowledgeGraph is closed");
            String result = nativeQuery(handle, query, maxLen);
            if (result == null) throw new NIMCPException("query failed");
            return result;
        }

        @Override
        public void close() {
            if (handle != 0) {
                nativeDestroy(handle);
                handle = 0;
            }
        }
    }
}
