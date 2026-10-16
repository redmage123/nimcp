/**
 * @file NIMCP.cs
 * @brief Complete C# bindings for NIMCP via P/Invoke
 * @version 2.6.3
 *
 * Wraps the entire nimcp.h public C API with idiomatic C# classes.
 * Uses IDisposable for deterministic resource cleanup,
 * typed exceptions, enums, and delegates for callbacks.
 *
 * Usage:
 *   NimcpLibrary.Init();
 *   using (var brain = new Brain("test", BrainSize.Tiny,
 *              BrainTask.Classification, 4, 2))
 *   {
 *       brain.Learn(new float[]{1,0,0.5f,0.3f}, "cat", 0.9f);
 *       var (label, conf) = brain.Predict(new float[]{1,0,0.5f,0.3f});
 *   }
 *   NimcpLibrary.Shutdown();
 */

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

namespace NIMCP
{
    // ========================================================================
    // Enums
    // ========================================================================

    public enum BrainSize { Tiny = 0, Small = 1, Medium = 2, Large = 3 }

    public enum BrainTask
    {
        Classification = 0, Regression = 1, PatternMatching = 2,
        Sequence = 3, Association = 4
    }

    public enum NetworkType
    {
        Adaptive = 0, SNN = 1, LNN = 2, CNN = 3, Hybrid = 4
    }

    public enum SNNTrainMethod
    {
        STDP = 0, RSTDP = 1, EProp = 2, Surrogate = 3, Homeostatic = 4
    }

    public enum LNNTrainMethod
    {
        Adjoint = 0, BPTT = 1, RTRL = 2, EProp = 3
    }

    public enum LossType
    {
        MSE = 0, CrossEntropy = 1, BinaryCE = 2, Huber = 3,
        MAE = 4, Focal = 5, KLDiv = 6
    }

    public enum OptimizerType
    {
        SGD = 0, Momentum = 1, Adam = 2, AdamW = 3, RMSprop = 4, Adagrad = 5
    }

    public enum SchedulerType
    {
        Constant = 0, Step = 1, Exponential = 2, Cosine = 3,
        WarmupCosine = 4, ReduceOnPlateau = 5, Cyclic = 6
    }

    public enum CallbackEvent
    {
        StepComplete = 0, EpochComplete = 1, LossComputed = 2,
        WeightsUpdated = 3, LRChanged = 4, Convergence = 5,
        Divergence = 6, Checkpoint = 7
    }

    public enum CallbackAction
    {
        Continue = 0, Stop = 1, Skip = 2, Rollback = 3,
        ReduceLR = 4, IncreaseLR = 5
    }

    public enum CognitiveModule
    {
        None = 0, Perception = 1, WorkingMemory = 2, Executive = 3,
        TheoryOfMind = 4, Ethics = 5, Attention = 6, Emotion = 7,
        Salience = 8, Motor = 9, Language = 10, Metacognition = 11,
        Curiosity = 12, Introspection = 13, Predictive = 14,
        Consolidation = 15, EpisodicMemory = 16, SemanticMemory = 17,
        Wellbeing = 18, MentalHealth = 19, GoalMotivation = 20,
        CognitiveControl = 21, CustomStart = 100
    }

    public enum StatusCode
    {
        Ok = 0, Error = 1000, ErrorNullArg = 1003,
        ErrorInvalid = 1004, ErrorMemory = 2000, ErrorIO = 4000
    }

    // ========================================================================
    // Exception Hierarchy
    // ========================================================================

    public class NIMCPException : Exception
    {
        public StatusCode Code { get; }
        public NIMCPException(string message) : base(message) { Code = StatusCode.Error; }
        public NIMCPException(StatusCode code, string message) : base(message) { Code = code; }
    }

    public class NullArgException : NIMCPException
    {
        public NullArgException(string msg) : base(StatusCode.ErrorNullArg, msg) {}
    }

    public class InvalidException : NIMCPException
    {
        public InvalidException(string msg) : base(StatusCode.ErrorInvalid, msg) {}
    }

    public class MemoryException : NIMCPException
    {
        public MemoryException(string msg) : base(StatusCode.ErrorMemory, msg) {}
    }

    public class IOException : NIMCPException
    {
        public IOException(string msg) : base(StatusCode.ErrorIO, msg) {}
    }

    // ========================================================================
    // Data Structures
    // ========================================================================

    public class Prediction
    {
        public string Label { get; }
        public float Confidence { get; }
        public Prediction(string label, float confidence)
        { Label = label; Confidence = confidence; }
    }

    public class TrainingConfig
    {
        public LossType LossType = LossType.CrossEntropy;
        public OptimizerType OptimizerType = OptimizerType.Adam;
        public SchedulerType SchedulerType = SchedulerType.Cosine;
        public float LearningRate = 0.001f;
        public float WeightDecay = 0.0f;
        public float Momentum = 0.9f;
        public float Beta1 = 0.9f;
        public float Beta2 = 0.999f;
        public float Epsilon = 1e-8f;
        public uint SchedulerStepSize = 100;
        public float SchedulerGamma = 0.1f;
        public uint WarmupSteps = 0;
        public bool EnableGradientClipping = false;
        public float GradientClipValue = 1.0f;
        public bool EnableBiologicalModulation = true;
        public float BiologicalBlend = 0.5f;
        public NetworkType NetworkType = NetworkType.Adaptive;
        public SNNTrainMethod SNNMethod = SNNTrainMethod.STDP;
        public float SNNEligibilityTau = 20.0f;
        public float SNNRewardTau = 100.0f;
        public float SNNSurrogateBeta = 5.0f;
        public LNNTrainMethod LNNMethod = LNNTrainMethod.Adjoint;
        public uint LNNBpttTruncation = 100;
        public bool LNNUseAdjointCheckpointing = true;
    }

    public class TrainingResult
    {
        public float Loss;
        public float LearningRate;
        public uint Step;
        public bool EarlyStopped;
        public float GradientNorm;
    }

    public class CallbackConfig
    {
        public bool EnableAutoCheckpoint = false;
        public uint CheckpointInterval = 100;
        public bool EnableEarlyStopping = false;
        public uint Patience = 10;
        public float MinDelta = 0.0001f;
        public float DivergenceThreshold = 10.0f;
        public uint LogInterval = 0;
    }

    public class CallbackMetrics
    {
        public ulong Step;
        public ulong Epoch;
        public float Loss;
        public float LossEma;
        public float LearningRate;
        public float GradientNorm;
        public ulong StepTimeUs;
        public bool IsConverging;
        public bool IsDiverging;
    }

    public class SnapshotInfo
    {
        public string Name;
        public string Description;
        public ulong Timestamp;
        public uint FileSize;
        public bool IsCompressed;
        public bool IsEncrypted;
    }

    public class BrainProbe
    {
        public string TaskName;
        public BrainSize Size;
        public BrainTask Task;
        public uint NumNeurons;
        public uint NumSynapses;
        public uint NumActiveSynapses;
        public ulong TotalInferences;
        public ulong TotalLearningSteps;
        public float AvgSparsity;
        public float AvgInferenceTimeUs;
        public float CurrentLearningRate;
        public float Accuracy;
        public ulong MemoryBytes;
        public uint NumInputs;
        public uint NumOutputs;
        public bool IsCowClone;
        public uint CowRefCount;
        public ulong CowSharedBytes;
        public ulong CowPrivateBytes;
    }

    public struct Phasor
    {
        public float Amplitude;
        public float Phase;
        public Phasor(float amplitude, float phase)
        { Amplitude = amplitude; Phase = phase; }
    }

    public class TrainingStats
    {
        public ulong TotalSteps;
        public float TotalLoss;
        public float CurrentLR;
    }

    public class UtilizationMetrics
    {
        public float Utilization;
        public float Saturation;
    }

    public class WorkspaceReadResult
    {
        public float[] Content;
        public uint ActualDim;
        public CognitiveModule SourceModule;
    }

    public class WorkspaceStats
    {
        public uint TotalBroadcasts;
        public uint TotalCompetitions;
        public float AvgStrength;
    }

    public class CallbackStats
    {
        public ulong TotalFired;
        public float AvgTimeUs;
        public uint EarlyStops;
    }

    public class WorkingMemoryStats
    {
        public uint CurrentSize;
        public uint Capacity;
    }

    // ========================================================================
    // Callback Delegate
    // ========================================================================

    public delegate CallbackAction TrainingCallbackDelegate(
        CallbackEvent evt, CallbackMetrics metrics);

    // ========================================================================
    // Native P/Invoke Declarations
    // ========================================================================

    internal static class Native
    {
        const string Lib = "nimcp";

        // --- Library lifecycle ---
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_init();

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nimcp_shutdown();

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr nimcp_version();

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_version_int();

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr nimcp_get_error();

        // --- Brain core ---
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr nimcp_brain_create(
            [MarshalAs(UnmanagedType.LPStr)] string name,
            int size, int task, uint numInputs, uint numOutputs);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nimcp_brain_destroy(IntPtr brain);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_learn_example(
            IntPtr brain, float[] features, uint numFeatures,
            [MarshalAs(UnmanagedType.LPStr)] string label, float confidence);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_predict(
            IntPtr brain, float[] features, uint numFeatures,
            StringBuilder outLabel, ref float outConfidence);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_infer(
            IntPtr brain, float[] features, uint numFeatures,
            float[] outputs, uint numOutputs);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_save(IntPtr brain,
            [MarshalAs(UnmanagedType.LPStr)] string filepath);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr nimcp_brain_load(
            [MarshalAs(UnmanagedType.LPStr)] string filepath);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr nimcp_brain_create_from_config(
            [MarshalAs(UnmanagedType.LPStr)] string filepath);

        // --- Brain training ---
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_configure_training(
            IntPtr brain, ref NativeTrainingConfig config);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_train_step(
            IntPtr brain, float[] features, uint numFeatures,
            float[] targets, uint numTargets, ref NativeTrainingResult result);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_train_batch(
            IntPtr brain, float[] features, float[] targets,
            uint batchSize, uint numFeatures, uint numTargets,
            ref NativeTrainingResult result);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_get_training_stats(
            IntPtr brain, ref ulong totalSteps, ref float totalLoss,
            ref float currentLR);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern float nimcp_brain_step_scheduler(
            IntPtr brain, float validationMetric);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern NativeTrainingConfig nimcp_training_config_default();

        // --- Brain callbacks ---
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_enable_callbacks(
            IntPtr brain, ref NativeCallbackConfig config);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_disable_callbacks(IntPtr brain);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate int NativeCallbackFn(int evt, IntPtr metrics, IntPtr userData);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern uint nimcp_brain_register_callback(
            IntPtr brain, int evt, NativeCallbackFn callback,
            IntPtr userData,
            [MarshalAs(UnmanagedType.LPStr)] string name);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_unregister_callback(
            IntPtr brain, uint callbackId);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_get_callback_stats(
            IntPtr brain, ref ulong totalFired, ref float avgTimeUs,
            ref uint earlyStops);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern NativeCallbackConfig nimcp_callback_config_default();

        // --- Brain resize ---
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool nimcp_brain_resize(IntPtr brain, uint count);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool nimcp_brain_auto_resize(IntPtr brain);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern uint nimcp_brain_get_neuron_count(IntPtr brain);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool nimcp_brain_get_utilization_metrics(
            IntPtr brain, ref float utilization, ref float saturation);

        // --- Brain named snapshots ---
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_snapshot_save(
            IntPtr brain,
            [MarshalAs(UnmanagedType.LPStr)] string name,
            [MarshalAs(UnmanagedType.LPStr)] string description);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr nimcp_brain_snapshot_restore(
            IntPtr brain,
            [MarshalAs(UnmanagedType.LPStr)] string name);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_snapshot_list(
            IntPtr brain, IntPtr infos, uint maxCount, ref uint outCount);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_snapshot_delete(
            IntPtr brain,
            [MarshalAs(UnmanagedType.LPStr)] string name);

        // --- Brain COW ---
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr nimcp_brain_clone_cow(IntPtr brain);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr nimcp_brain_snapshot_cow(IntPtr brain);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_restore_cow(
            IntPtr brain, IntPtr snapshot);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nimcp_brain_snapshot_destroy(IntPtr snapshot);

        // --- Brain working memory ---
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_working_memory_add(
            IntPtr brain, float[] data, uint size, float salience);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr nimcp_brain_working_memory_get(
            IntPtr brain, uint index, ref uint sizeOut);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_working_memory_stats(
            IntPtr brain, ref uint currentSize, ref uint capacity);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_working_memory_refresh(
            IntPtr brain, uint index);

        // --- Brain workspace ---
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_workspace_compete(
            IntPtr brain, int module, float[] content,
            uint contentDim, float strength);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_workspace_read(
            IntPtr brain, float[] content, uint maxDim,
            ref uint actualDim, ref int sourceModule);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_workspace_subscribe(
            IntPtr brain, int module);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_workspace_unsubscribe(
            IntPtr brain, int module);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_workspace_has_broadcast(
            IntPtr brain, [MarshalAs(UnmanagedType.I1)] ref bool hasBroadcast);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_workspace_stats(
            IntPtr brain, ref uint totalBroadcasts,
            ref uint totalCompetitions, ref float avgStrength);

        // --- Brain oscillations ---
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool nimcp_enable_complex_oscillations(
            IntPtr brain, [MarshalAs(UnmanagedType.I1)] bool enable);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool nimcp_is_complex_oscillations_enabled(
            IntPtr brain);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern NativePhasor nimcp_get_oscillation_phasor(
            IntPtr brain, uint neuronId);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern float nimcp_get_phase_coherence(
            IntPtr brain, uint[] neuronIds, uint count);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern float nimcp_get_pac_modulation(
            IntPtr brain, float thetaFreq, float gammaFreq);

        // --- Brain probe ---
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_probe(
            IntPtr brain, ref NativeBrainProbe probe);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_broadcast_probe(IntPtr brain);

        // --- Network ---
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr nimcp_network_create(
            uint numInputs, uint numOutputs, uint numHidden, float lr);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nimcp_network_destroy(IntPtr network);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_network_forward(
            IntPtr network, float[] inputs, uint numInputs,
            float[] outputs, uint numOutputs);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_network_train(
            IntPtr network, float[] inputs, uint numInputs,
            float[] targets, uint numTargets);

        // --- Ethics ---
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr nimcp_ethics_create();

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nimcp_ethics_destroy(IntPtr ethics);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_ethics_check(
            IntPtr ethics, float[] situation, uint numFeatures,
            ref float outScore);

        // --- Knowledge ---
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr nimcp_knowledge_create();

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nimcp_knowledge_destroy(IntPtr knowledge);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_knowledge_add_fact(
            IntPtr knowledge,
            [MarshalAs(UnmanagedType.LPStr)] string subject,
            [MarshalAs(UnmanagedType.LPStr)] string predicate,
            [MarshalAs(UnmanagedType.LPStr)] string obj);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_knowledge_query(
            IntPtr knowledge,
            [MarshalAs(UnmanagedType.LPStr)] string query,
            StringBuilder outResult, uint maxResultLen);

        // --- Sensory / Multimodal ---
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_submit_sensory(
            IntPtr brain,
            [MarshalAs(UnmanagedType.LPStr)] string modality,
            float[] data, uint numElements,
            uint width, uint height, uint channels,
            uint nSegments);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_visual_cortex_process(
            IntPtr brain,
            float[] pixels, uint numPixels,
            uint width, uint height, uint channels,
            float[] outFeatures, uint maxFeatures,
            ref uint outFeatureCount);

        // --- Avatar / Metrics ---
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_get_avatar_state(
            IntPtr brain, ref NativeAvatarState state);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool nimcp_brain_get_network_metrics(
            IntPtr brain,
            ref float emaAnn, ref float emaCnn,
            ref float emaSnn, ref float emaLnn,
            ref ulong annSteps, ref ulong cnnSteps,
            ref ulong snnSteps, ref ulong lnnSteps);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_get_cortex_cnn_metrics(
            IntPtr brain,
            int[] outTypes, float[] outLosses,
            ulong[] outFwdSteps, ulong[] outBwdSteps,
            float[] outEmbedNorms, ref uint outCount);

        // --- Core Inference ---
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_decide_full(
            IntPtr brain,
            float[] features, uint numFeatures,
            StringBuilder outLabel, ref float outConfidence,
            StringBuilder outExplanation,
            float[] outOutputVector, ref uint outOutputSize,
            ref uint outNumActiveNeurons, ref float outSparsity,
            ref ulong outInferenceTimeUs);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern uint nimcp_brain_get_last_transcript(
            IntPtr brain,
            IntPtr outEntries,
            float[] outSaliences,
            float[] outConfidences,
            IntPtr outModules,
            uint maxEntries);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_get_cognitive_stats(
            IntPtr brain,
            uint[] outSteps, float[] outLosses, ref uint outCount);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern float nimcp_brain_get_accuracy(IntPtr brain);

        // --- LNN / SNN / CNN ---
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_lnn_create(
            IntPtr brain,
            uint nSensory, uint nInter, uint nCommand, uint nOutput);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_lnn_get_stats(
            IntPtr brain,
            ref ulong outForwardSteps, ref ulong outBackwardSteps,
            ref ulong outOdeEvals, ref float outAvgTau,
            ref float outStateNorm, ref float outGradientNorm,
            ref uint outNanCount, ref uint outInfCount);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_snn_get_stats(
            IntPtr brain,
            ref ulong outTotalSteps, ref ulong outTotalSpikes,
            ref float outMeanFiringRate, ref float outSparsity,
            ref float outSynchrony, ref uint outSilentNeurons,
            ref uint outHyperactiveNeurons, ref int outHealth,
            ref ulong outMemoryBytes);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nimcp_snn_set_input_scale(float scale);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern float nimcp_snn_get_input_scale();

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_cnn_get_stats(
            IntPtr brain,
            ref uint outNumLayers, ref ulong outNumParameters,
            ref uint outNumLabels,
            [MarshalAs(UnmanagedType.I1)] ref bool outActive);

        // --- Configuration ---
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_set_fast_training(
            IntPtr brain, [MarshalAs(UnmanagedType.I1)] bool enabled);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_set_task_type(
            IntPtr brain,
            [MarshalAs(UnmanagedType.LPStr)] string taskType);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_enable_biological_plasticity(
            IntPtr brain, [MarshalAs(UnmanagedType.I1)] bool enabled);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_enable_multi_network(IntPtr brain);

        // --- Edge Brain ---
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_edge_brain_resize(IntPtr brain, ref NativeResizeConfig config);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_edge_brain_resize_check(
            IntPtr brain, ref NativeResizeConfig config, ref NativeResizeReport report);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_edge_score_neuron_importance(
            IntPtr brain, float[] scores, uint numNeurons);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_distill(
            IntPtr teacher, ref IntPtr student,
            ref NativeDistillConfig config, ref NativeDistillReport report);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_optimize_for_device(
            IntPtr master, ref NativeDeviceProfile device,
            ref IntPtr child, ref NativeOptimizationReport report);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_quantize(IntPtr brain, ref NativeQuantizeConfig config);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern NativeResizeConfig nimcp_resize_config_default();

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern NativeDistillConfig nimcp_distill_config_default();

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern NativeQuantizeConfig nimcp_quantize_config_default();

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern NativeDeviceProfile nimcp_device_profile_default();

        // --- Swarm Runtime ---
        [DllImport(Lib)] public static extern IntPtr nimcp_swarm_master_create(IntPtr brain, IntPtr config);
        [DllImport(Lib)] public static extern void nimcp_swarm_master_destroy(IntPtr master);
        [DllImport(Lib)] public static extern int nimcp_swarm_master_start(IntPtr master);
        [DllImport(Lib)] public static extern int nimcp_swarm_master_stop(IntPtr master);
        [DllImport(Lib)] public static extern int nimcp_swarm_master_kick(IntPtr master, uint deviceId);
        [DllImport(Lib)] public static extern int nimcp_swarm_master_force_sync(IntPtr master);
        [DllImport(Lib)] public static extern uint nimcp_swarm_master_get_peer_count(IntPtr master);
        [DllImport(Lib)] public static extern int nimcp_swarm_master_get_peer_info(IntPtr master, uint deviceId, IntPtr entryOut);
        [DllImport(Lib)] public static extern IntPtr nimcp_swarm_master_config_default();
        [DllImport(Lib)] public static extern IntPtr nimcp_swarm_edge_create(IntPtr brain, IntPtr config);
        [DllImport(Lib)] public static extern void nimcp_swarm_edge_destroy(IntPtr rt);
        [DllImport(Lib)] public static extern int nimcp_swarm_edge_start(IntPtr rt);
        [DllImport(Lib)] public static extern int nimcp_swarm_edge_stop(IntPtr rt);
        [DllImport(Lib)] [return: MarshalAs(UnmanagedType.I1)] public static extern bool nimcp_swarm_edge_is_connected(IntPtr rt);
        [DllImport(Lib)] public static extern int nimcp_swarm_edge_submit_gradients(IntPtr rt, float[] gradients, uint numParams);
        [DllImport(Lib)] public static extern IntPtr nimcp_swarm_edge_config_default();

        // --- Sensor Hub ---
        [DllImport(Lib)] public static extern IntPtr nimcp_sensor_hub_create(uint maxSensors);
        [DllImport(Lib)] public static extern void nimcp_sensor_hub_destroy(IntPtr hub);
        [DllImport(Lib)] public static extern int nimcp_sensor_register(IntPtr hub, IntPtr descriptor);
        [DllImport(Lib)] public static extern int nimcp_sensor_submit_reading(IntPtr hub, IntPtr reading);
        [DllImport(Lib)] public static extern int nimcp_sensor_get_latest(IntPtr hub, uint sensorId, IntPtr readingOut);
        [DllImport(Lib)] public static extern int nimcp_sensor_get_all_latest(IntPtr hub, IntPtr readingsOut, uint maxCount);
        [DllImport(Lib)] public static extern int nimcp_sensor_compose_feature_vector(IntPtr hub, float[] features, uint maxFeatures);
        [DllImport(Lib)] public static extern uint nimcp_sensor_get_count(IntPtr hub);

        // --- Safety Watchdog ---
        [DllImport(Lib)] public static extern IntPtr nimcp_watchdog_create(IntPtr config);
        [DllImport(Lib)] public static extern void nimcp_watchdog_destroy(IntPtr watchdog);
        [DllImport(Lib)] public static extern int nimcp_watchdog_arm(IntPtr watchdog);
        [DllImport(Lib)] public static extern int nimcp_watchdog_disarm(IntPtr watchdog);
        [DllImport(Lib)] public static extern void nimcp_watchdog_heartbeat(IntPtr watchdog);
        [DllImport(Lib)] public static extern int nimcp_watchdog_validate_output(IntPtr watchdog, float[] output, uint numOutputs);
        [DllImport(Lib)] public static extern int nimcp_watchdog_get_safe_output(IntPtr watchdog, float[] output, uint numOutputs);
        [DllImport(Lib)] public static extern void nimcp_watchdog_estop(IntPtr watchdog);
        [DllImport(Lib)] public static extern int nimcp_watchdog_reset(IntPtr watchdog);
        [DllImport(Lib)] public static extern int nimcp_watchdog_get_state(IntPtr watchdog);
        [DllImport(Lib)] public static extern IntPtr nimcp_watchdog_state_name(int state);
        [DllImport(Lib)] public static extern IntPtr nimcp_watchdog_config_default();

        // --- ROS 2 Bridge ---
        [DllImport(Lib)] public static extern IntPtr nimcp_ros2_bridge_create(IntPtr brain, IntPtr config);
        [DllImport(Lib)] public static extern void nimcp_ros2_bridge_destroy(IntPtr bridge);
        [DllImport(Lib)] public static extern int nimcp_ros2_bridge_start(IntPtr bridge);
        [DllImport(Lib)] public static extern int nimcp_ros2_bridge_stop(IntPtr bridge);
        [DllImport(Lib)] public static extern int nimcp_ros2_bridge_inject_sensor(IntPtr bridge, [MarshalAs(UnmanagedType.LPStr)] string topic, float[] data, uint count);
        [DllImport(Lib)] public static extern int nimcp_ros2_bridge_get_last_cmd(IntPtr bridge, float[] data, uint maxCount);
        [DllImport(Lib)] public static extern IntPtr nimcp_ros2_config_default();

        // --- MAVLink Bridge ---
        [DllImport(Lib)] public static extern IntPtr nimcp_mavlink_bridge_create(IntPtr config);
        [DllImport(Lib)] public static extern void nimcp_mavlink_bridge_destroy(IntPtr bridge);
        [DllImport(Lib)] public static extern int nimcp_mavlink_bridge_connect(IntPtr bridge);
        [DllImport(Lib)] public static extern int nimcp_mavlink_bridge_disconnect(IntPtr bridge);
        [DllImport(Lib)] public static extern int nimcp_mavlink_bridge_start(IntPtr bridge);
        [DllImport(Lib)] public static extern int nimcp_mavlink_bridge_stop(IntPtr bridge);
        [DllImport(Lib)] public static extern int nimcp_mavlink_get_attitude(IntPtr bridge, IntPtr att);
        [DllImport(Lib)] public static extern int nimcp_mavlink_get_position(IntPtr bridge, IntPtr pos);
        [DllImport(Lib)] public static extern int nimcp_mavlink_get_battery(IntPtr bridge, IntPtr bat);
        [DllImport(Lib)] public static extern int nimcp_mavlink_set_velocity(IntPtr bridge, float vx, float vy, float vz, float yawRate);
        [DllImport(Lib)] public static extern int nimcp_mavlink_arm(IntPtr bridge, [MarshalAs(UnmanagedType.I1)] bool arm);
        [DllImport(Lib)] public static extern int nimcp_mavlink_takeoff(IntPtr bridge, float altitude);
        [DllImport(Lib)] public static extern int nimcp_mavlink_land(IntPtr bridge);
        [DllImport(Lib)] public static extern int nimcp_mavlink_goto(IntPtr bridge, double lat, double lon, float alt);
        [DllImport(Lib)] public static extern int nimcp_mavlink_rtl(IntPtr bridge);
        [DllImport(Lib)] public static extern int nimcp_mavlink_compose_features(IntPtr bridge, float[] features, uint maxFeatures);
        [DllImport(Lib)] public static extern IntPtr nimcp_mavlink_config_default();

        // --- Memory Store / OOD / Audit (via brain handle wrappers) ---
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_memory_store_stats(
            IntPtr brain, ref NativeMemoryStoreStats stats);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_memory_search_text(
            IntPtr brain,
            [MarshalAs(UnmanagedType.LPStr)] string query,
            uint maxResults, ulong[] outIds, ref uint outCount);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_memory_search_similar(
            IntPtr brain, float[] embedding, uint dim,
            uint topK, ulong[] outIds, float[] outDistances, ref uint outCount);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool nimcp_brain_memory_is_healthy(IntPtr brain);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_ood_stats(
            IntPtr brain, ref NativeOodStats stats);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_audit_log(
            IntPtr brain,
            [MarshalAs(UnmanagedType.LPStr)] string description,
            uint severity,
            [MarshalAs(UnmanagedType.LPStr)] string details);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_audit_search(
            IntPtr brain, uint minSeverity, uint maxResults,
            ulong[] outIds, float[] outSeverities, ref uint outCount);

        // --- Brain State ---
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern float nimcp_brain_medulla_get_arousal(IntPtr brain);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern float nimcp_brain_sleep_get_pressure(IntPtr brain);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern float nimcp_brain_bg_get_dopamine(IntPtr brain);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_substrate_get_health(
            IntPtr brain, StringBuilder outStatus, uint maxLen);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nimcp_brain_focus_attention(
            IntPtr brain,
            [MarshalAs(UnmanagedType.LPStr)] string modality);

        // --- Per-network training toggles (runtime-dynamic, no rebuild required) ---

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern void nimcp_brain_set_train_ann(IntPtr brain,
            [MarshalAs(UnmanagedType.I1)] bool enabled);
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool nimcp_brain_get_train_ann(IntPtr brain);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nimcp_brain_set_train_cnn(IntPtr brain,
            [MarshalAs(UnmanagedType.I1)] bool enabled);
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool nimcp_brain_get_train_cnn(IntPtr brain);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nimcp_brain_set_train_snn(IntPtr brain,
            [MarshalAs(UnmanagedType.I1)] bool enabled);
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool nimcp_brain_get_train_snn(IntPtr brain);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nimcp_brain_set_train_lnn(IntPtr brain,
            [MarshalAs(UnmanagedType.I1)] bool enabled);
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool nimcp_brain_get_train_lnn(IntPtr brain);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nimcp_brain_set_snn_only_recovery(IntPtr brain,
            [MarshalAs(UnmanagedType.I1)] bool enabled);
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool nimcp_brain_get_snn_only_recovery(IntPtr brain);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nimcp_brain_set_ensemble_warmup_scale(IntPtr brain, float scale);
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern float nimcp_brain_get_ensemble_warmup_scale(IntPtr brain);

        // --- Native structs ---

        [StructLayout(LayoutKind.Sequential)]
        public struct NativeAvatarState
        {
            /* Viseme / mouth shape */
            public float mouth_open;
            public float lip_round;
            public float lip_upper;
            public float lip_lower;
            public float tongue_position;
            public byte current_viseme;
            private byte _pad0;   /* C struct padding after uint8_t */
            private byte _pad1;
            private byte _pad2;

            /* FACS Action Units */
            public float au1_inner_brow_raise;
            public float au2_outer_brow_raise;
            public float au4_brow_lower;
            public float au5_upper_lid_raise;
            public float au6_cheek_raise;
            public float au7_lid_tighten;
            public float au9_nose_wrinkle;
            public float au10_upper_lip_raise;
            public float au12_lip_corner_pull;
            public float au15_lip_corner_drop;
            public float au17_chin_raise;
            public float au20_lip_stretch;
            public float au23_lip_tighten;
            public float au25_lips_part;
            public float au26_jaw_drop;
            public float au28_lip_suck;

            /* Emotional state */
            public float valence;
            public float arousal;
            public float dominance;
            public uint emotion_id;
            public float emotion_intensity;

            /* Gaze and head pose */
            public float gaze_x;
            public float gaze_y;
            public float head_pitch;
            public float head_yaw;
            public float head_roll;
            public float blink;

            /* Voice parameters */
            public float pitch_hz;
            public float speaking_rate;
            public float volume;

            /* Metadata */
            public ulong timestamp_us;
            [MarshalAs(UnmanagedType.I1)] public bool is_speaking;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct NativeTrainingConfig
        {
            public int loss_type;
            public int optimizer_type;
            public int scheduler_type;
            public float learning_rate;
            public float weight_decay;
            public float momentum;
            public float beta1;
            public float beta2;
            public float epsilon;
            public uint scheduler_step_size;
            public float scheduler_gamma;
            public uint warmup_steps;
            [MarshalAs(UnmanagedType.I1)] public bool enable_gradient_clipping;
            public float gradient_clip_value;
            [MarshalAs(UnmanagedType.I1)] public bool enable_biological_modulation;
            public float biological_blend;
            public int network_type;
            public int snn_method;
            public float snn_eligibility_tau;
            public float snn_reward_tau;
            public float snn_surrogate_beta;
            public int lnn_method;
            public uint lnn_bptt_truncation;
            [MarshalAs(UnmanagedType.I1)] public bool lnn_use_adjoint_checkpointing;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct NativeTrainingResult
        {
            public float loss;
            public float learning_rate;
            public uint step;
            [MarshalAs(UnmanagedType.I1)] public bool early_stopped;
            public float gradient_norm;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct NativeCallbackConfig
        {
            [MarshalAs(UnmanagedType.I1)] public bool enable_auto_checkpoint;
            public uint checkpoint_interval;
            [MarshalAs(UnmanagedType.I1)] public bool enable_early_stopping;
            public uint patience;
            public float min_delta;
            public float divergence_threshold;
            public uint log_interval;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct NativeCallbackMetrics
        {
            public ulong step;
            public ulong epoch;
            public float loss;
            public float loss_ema;
            public float learning_rate;
            public float gradient_norm;
            public ulong step_time_us;
            [MarshalAs(UnmanagedType.I1)] public bool is_converging;
            [MarshalAs(UnmanagedType.I1)] public bool is_diverging;
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        public struct NativeSnapshotInfo
        {
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
            public string name;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 512)]
            public string description;
            public ulong timestamp;
            public uint file_size;
            [MarshalAs(UnmanagedType.I1)] public bool is_compressed;
            [MarshalAs(UnmanagedType.I1)] public bool is_encrypted;
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        public struct NativeBrainProbe
        {
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)]
            public string task_name;
            public int size;
            public int task;
            public uint num_neurons;
            public uint num_synapses;
            public uint num_active_synapses;
            public ulong total_inferences;
            public ulong total_learning_steps;
            public float avg_sparsity;
            public float avg_inference_time_us;
            public float current_learning_rate;
            public float accuracy;
            public ulong memory_bytes;
            public uint num_inputs;
            public uint num_outputs;
            [MarshalAs(UnmanagedType.I1)] public bool is_cow_clone;
            public uint cow_ref_count;
            public ulong cow_shared_bytes;
            public ulong cow_private_bytes;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct NativePhasor
        {
            public float amplitude;
            public float phase;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct NativeResizeConfig
        {
            public uint target_neuron_count;
            public int mode;  /* 0=contract, 1=expand, 2=rebalance */
            [MarshalAs(UnmanagedType.I1)] public bool enable_knowledge_transfer;
            public float importance_threshold;
            public uint maturation_steps;
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        public struct NativeResizeReport
        {
            [MarshalAs(UnmanagedType.I1)] public bool feasible;
            public uint neurons_before;
            public uint neurons_after;
            public float estimated_ram_delta_mb;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
            public string reason;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct NativeDistillConfig
        {
            public uint target_neurons;
            public float temperature;
            public uint distillation_steps;
            [MarshalAs(UnmanagedType.I1)] public bool include_snn;
            [MarshalAs(UnmanagedType.I1)] public bool include_lnn;
            [MarshalAs(UnmanagedType.I1)] public bool include_cnn;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct NativeDistillReport
        {
            public float accuracy_retention;
            public uint neurons_selected;
            public float compression_ratio;
            public float teacher_loss;
            public float student_loss;
            public uint steps_trained;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct NativeQuantizeConfig
        {
            public int weight_precision;  /* enum nimcp_quantization_t */
            public uint calibration_samples;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct NativeDeviceProfile
        {
            public uint ram_mb;
            public uint cpu_cores;
            [MarshalAs(UnmanagedType.I1)] public bool has_camera;
            [MarshalAs(UnmanagedType.I1)] public bool has_imu;
            [MarshalAs(UnmanagedType.I1)] public bool has_motor_control;
            [MarshalAs(UnmanagedType.I1)] public bool has_network;
            public int role;  /* enum nimcp_device_role_t */
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct NativeOptimizationReport
        {
            public uint neuron_count;
            public uint subsystems_enabled;
            public float estimated_ram_mb;
            public float estimated_inference_ms;
            public float accuracy_retention;
            public uint num_warnings;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct NativeMemoryStoreStats
        {
            public ulong total_engrams;
            public ulong total_concepts;
            public ulong total_relations;
            public ulong total_autobio;
            public ulong total_writes;
            public ulong total_reads;
            public ulong cache_hits;
            public ulong cache_misses;
            public ulong write_buffer_flushes;
            public ulong bloom_filter_hits;
            public float avg_write_latency_ms;
            public float avg_read_latency_ms;
            public ulong db_size_bytes;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct NativeOodStats
        {
            public ulong total_checks;
            public ulong ood_detected;
            public ulong in_distribution;
            public float avg_ood_score;
            public float max_ood_score;
            public float ood_rate;
        }
    }

    // ========================================================================
    // Helper
    // ========================================================================

    internal static class Helper
    {
        public static string GetError()
        {
            IntPtr ptr = Native.nimcp_get_error();
            if (ptr == IntPtr.Zero) return "NIMCP error";
            string msg = Marshal.PtrToStringAnsi(ptr);
            return string.IsNullOrEmpty(msg) ? "NIMCP error" : msg;
        }

        public static void CheckStatus(int status)
        {
            if (status == 0) return;
            string msg = GetError();
            switch (status)
            {
                case 1003: throw new NullArgException(msg);
                case 1004: throw new InvalidException(msg);
                case 2000: throw new MemoryException(msg);
                case 4000: throw new IOException(msg);
                default:   throw new NIMCPException((StatusCode)status, msg);
            }
        }

        public static Native.NativeTrainingConfig ToNative(TrainingConfig cfg)
        {
            var n = Native.nimcp_training_config_default();
            n.loss_type = (int)cfg.LossType;
            n.optimizer_type = (int)cfg.OptimizerType;
            n.scheduler_type = (int)cfg.SchedulerType;
            n.learning_rate = cfg.LearningRate;
            n.weight_decay = cfg.WeightDecay;
            n.momentum = cfg.Momentum;
            n.beta1 = cfg.Beta1;
            n.beta2 = cfg.Beta2;
            n.epsilon = cfg.Epsilon;
            n.scheduler_step_size = cfg.SchedulerStepSize;
            n.scheduler_gamma = cfg.SchedulerGamma;
            n.warmup_steps = cfg.WarmupSteps;
            n.enable_gradient_clipping = cfg.EnableGradientClipping;
            n.gradient_clip_value = cfg.GradientClipValue;
            n.enable_biological_modulation = cfg.EnableBiologicalModulation;
            n.biological_blend = cfg.BiologicalBlend;
            n.network_type = (int)cfg.NetworkType;
            n.snn_method = (int)cfg.SNNMethod;
            n.snn_eligibility_tau = cfg.SNNEligibilityTau;
            n.snn_reward_tau = cfg.SNNRewardTau;
            n.snn_surrogate_beta = cfg.SNNSurrogateBeta;
            n.lnn_method = (int)cfg.LNNMethod;
            n.lnn_bptt_truncation = cfg.LNNBpttTruncation;
            n.lnn_use_adjoint_checkpointing = cfg.LNNUseAdjointCheckpointing;
            return n;
        }

        public static Native.NativeCallbackConfig ToNative(CallbackConfig cfg)
        {
            var n = Native.nimcp_callback_config_default();
            n.enable_auto_checkpoint = cfg.EnableAutoCheckpoint;
            n.checkpoint_interval = cfg.CheckpointInterval;
            n.enable_early_stopping = cfg.EnableEarlyStopping;
            n.patience = cfg.Patience;
            n.min_delta = cfg.MinDelta;
            n.divergence_threshold = cfg.DivergenceThreshold;
            n.log_interval = cfg.LogInterval;
            return n;
        }
    }

    // ========================================================================
    // Library Lifecycle
    // ========================================================================

    public static class NimcpLibrary
    {
        public static void Init()
        {
            Helper.CheckStatus(Native.nimcp_init());
        }

        public static void Shutdown() => Native.nimcp_shutdown();

        public static string Version()
        {
            IntPtr ptr = Native.nimcp_version();
            return ptr != IntPtr.Zero ? Marshal.PtrToStringAnsi(ptr) : "unknown";
        }

        public static int VersionInt() => Native.nimcp_version_int();
    }

    // ========================================================================
    // Brain Class
    // ========================================================================

    public class Brain : IDisposable
    {
        private IntPtr handle;
        private bool disposed;
        // prevent GC of callback delegates while registered
        private System.Collections.Generic.List<Native.NativeCallbackFn> callbackRefs
            = new System.Collections.Generic.List<Native.NativeCallbackFn>();

        public Brain(string name, BrainSize size, BrainTask task,
                     uint numInputs, uint numOutputs)
        {
            handle = Native.nimcp_brain_create(
                name, (int)size, (int)task, numInputs, numOutputs);
            if (handle == IntPtr.Zero)
                throw new NIMCPException("Failed to create brain");
        }

        private Brain(IntPtr handle) { this.handle = handle; }

        public static Brain Load(string filepath)
        {
            IntPtr h = Native.nimcp_brain_load(filepath);
            if (h == IntPtr.Zero)
                throw new IOException("Failed to load brain: " + filepath);
            return new Brain(h);
        }

        public static Brain CreateFromConfig(string filepath)
        {
            IntPtr h = Native.nimcp_brain_create_from_config(filepath);
            if (h == IntPtr.Zero)
                throw new NIMCPException("Failed to create brain from config");
            return new Brain(h);
        }

        private void CheckOpen()
        {
            if (disposed) throw new ObjectDisposedException(nameof(Brain));
        }

        // --- Core ---

        public void Learn(float[] features, string label, float confidence = 1.0f)
        {
            CheckOpen();
            Helper.CheckStatus(Native.nimcp_brain_learn_example(
                handle, features, (uint)features.Length, label, confidence));
        }

        public Prediction Predict(float[] features)
        {
            CheckOpen();
            var labelBuf = new StringBuilder(64);
            float conf = 0;
            Helper.CheckStatus(Native.nimcp_brain_predict(
                handle, features, (uint)features.Length, labelBuf, ref conf));
            return new Prediction(labelBuf.ToString(), conf);
        }

        public void Infer(float[] features, float[] outputs)
        {
            CheckOpen();
            Helper.CheckStatus(Native.nimcp_brain_infer(
                handle, features, (uint)features.Length,
                outputs, (uint)outputs.Length));
        }

        public void Save(string filepath)
        {
            CheckOpen();
            Helper.CheckStatus(Native.nimcp_brain_save(handle, filepath));
        }

        // --- Training Pipeline ---

        public void ConfigureTraining(TrainingConfig cfg)
        {
            CheckOpen();
            var nc = Helper.ToNative(cfg);
            Helper.CheckStatus(
                Native.nimcp_brain_configure_training(handle, ref nc));
        }

        public TrainingResult TrainStep(float[] features, float[] targets)
        {
            CheckOpen();
            var nr = new Native.NativeTrainingResult();
            Helper.CheckStatus(Native.nimcp_brain_train_step(
                handle, features, (uint)features.Length,
                targets, (uint)targets.Length, ref nr));
            return new TrainingResult
            {
                Loss = nr.loss, LearningRate = nr.learning_rate,
                Step = nr.step, EarlyStopped = nr.early_stopped,
                GradientNorm = nr.gradient_norm
            };
        }

        public TrainingResult TrainBatch(float[] features, float[] targets,
            uint batchSize, uint numFeatures, uint numTargets)
        {
            CheckOpen();
            var nr = new Native.NativeTrainingResult();
            Helper.CheckStatus(Native.nimcp_brain_train_batch(
                handle, features, targets,
                batchSize, numFeatures, numTargets, ref nr));
            return new TrainingResult
            {
                Loss = nr.loss, LearningRate = nr.learning_rate,
                Step = nr.step, EarlyStopped = nr.early_stopped,
                GradientNorm = nr.gradient_norm
            };
        }

        public TrainingStats GetTrainingStats()
        {
            CheckOpen();
            ulong steps = 0; float loss = 0, lr = 0;
            Helper.CheckStatus(Native.nimcp_brain_get_training_stats(
                handle, ref steps, ref loss, ref lr));
            return new TrainingStats
            { TotalSteps = steps, TotalLoss = loss, CurrentLR = lr };
        }

        public float StepScheduler(float validationMetric)
        {
            CheckOpen();
            return Native.nimcp_brain_step_scheduler(handle, validationMetric);
        }

        // --- Callbacks ---

        public void EnableCallbacks(CallbackConfig cfg)
        {
            CheckOpen();
            var nc = Helper.ToNative(cfg);
            Helper.CheckStatus(
                Native.nimcp_brain_enable_callbacks(handle, ref nc));
        }

        public void DisableCallbacks()
        {
            CheckOpen();
            Helper.CheckStatus(Native.nimcp_brain_disable_callbacks(handle));
        }

        public uint RegisterCallback(CallbackEvent evt,
            TrainingCallbackDelegate callback, string name = null)
        {
            CheckOpen();
            Native.NativeCallbackFn native = (int e, IntPtr metricsPtr, IntPtr ud) =>
            {
                var metrics = new CallbackMetrics();
                if (metricsPtr != IntPtr.Zero)
                {
                    var nm = Marshal.PtrToStructure<Native.NativeCallbackMetrics>(metricsPtr);
                    metrics.Step = nm.step; metrics.Epoch = nm.epoch;
                    metrics.Loss = nm.loss; metrics.LossEma = nm.loss_ema;
                    metrics.LearningRate = nm.learning_rate;
                    metrics.GradientNorm = nm.gradient_norm;
                    metrics.StepTimeUs = nm.step_time_us;
                    metrics.IsConverging = nm.is_converging;
                    metrics.IsDiverging = nm.is_diverging;
                }
                var action = callback((CallbackEvent)e, metrics);
                return (int)action;
            };
            callbackRefs.Add(native); // prevent GC

            uint id = Native.nimcp_brain_register_callback(
                handle, (int)evt, native, IntPtr.Zero, name);
            if (id == 0) throw new NIMCPException("Failed to register callback");
            return id;
        }

        public void UnregisterCallback(uint callbackId)
        {
            CheckOpen();
            Helper.CheckStatus(
                Native.nimcp_brain_unregister_callback(handle, callbackId));
        }

        public CallbackStats GetCallbackStats()
        {
            CheckOpen();
            ulong fired = 0; float avg = 0; uint early = 0;
            Helper.CheckStatus(Native.nimcp_brain_get_callback_stats(
                handle, ref fired, ref avg, ref early));
            return new CallbackStats
            { TotalFired = fired, AvgTimeUs = avg, EarlyStops = early };
        }

        // --- Resize ---

        public bool Resize(uint neuronCount)
        {
            CheckOpen();
            return Native.nimcp_brain_resize(handle, neuronCount);
        }

        public bool AutoResize()
        {
            CheckOpen();
            return Native.nimcp_brain_auto_resize(handle);
        }

        public uint GetNeuronCount()
        {
            CheckOpen();
            return Native.nimcp_brain_get_neuron_count(handle);
        }

        // --- Per-network training toggles (runtime-dynamic, no rebuild required) ---

        public void SetTrainAnn(bool enabled)
        { CheckOpen(); Native.nimcp_brain_set_train_ann(handle, enabled); }
        public bool GetTrainAnn()
        { CheckOpen(); return Native.nimcp_brain_get_train_ann(handle); }

        public void SetTrainCnn(bool enabled)
        { CheckOpen(); Native.nimcp_brain_set_train_cnn(handle, enabled); }
        public bool GetTrainCnn()
        { CheckOpen(); return Native.nimcp_brain_get_train_cnn(handle); }

        public void SetTrainSnn(bool enabled)
        { CheckOpen(); Native.nimcp_brain_set_train_snn(handle, enabled); }
        public bool GetTrainSnn()
        { CheckOpen(); return Native.nimcp_brain_get_train_snn(handle); }

        public void SetTrainLnn(bool enabled)
        { CheckOpen(); Native.nimcp_brain_set_train_lnn(handle, enabled); }
        public bool GetTrainLnn()
        { CheckOpen(); return Native.nimcp_brain_get_train_lnn(handle); }

        /// <summary>
        /// SNN-only recovery preset: freeze ANN/CNN/LNN while keeping SNN
        /// training active. Used to let the SNN re-converge against a stable
        /// ensemble after large BPTT behavior changes.
        /// </summary>
        public void SetSnnOnlyRecovery(bool enabled)
        { CheckOpen(); Native.nimcp_brain_set_snn_only_recovery(handle, enabled); }
        public bool GetSnnOnlyRecovery()
        { CheckOpen(); return Native.nimcp_brain_get_snn_only_recovery(handle); }

        /// <summary>
        /// Probabilistic gate on non-SNN training updates [0.0, 1.0].
        /// 1.0 = full-rate (default), 0.0 = fully frozen, intermediate =
        /// Monte-Carlo skip. Used by the daemon plateau detector to ramp
        /// ANN/CNN/LNN back in gradually after SNN-only recovery. Clamped
        /// to [0.0, 1.0] on the native side.
        /// </summary>
        public void SetEnsembleWarmupScale(float scale)
        { CheckOpen(); Native.nimcp_brain_set_ensemble_warmup_scale(handle, scale); }
        public float GetEnsembleWarmupScale()
        { CheckOpen(); return Native.nimcp_brain_get_ensemble_warmup_scale(handle); }

        public UtilizationMetrics GetUtilizationMetrics()
        {
            CheckOpen();
            float util = 0, sat = 0;
            Native.nimcp_brain_get_utilization_metrics(
                handle, ref util, ref sat);
            return new UtilizationMetrics
            { Utilization = util, Saturation = sat };
        }

        // --- Named Snapshots ---

        public void SnapshotSave(string name, string description = null)
        {
            CheckOpen();
            Helper.CheckStatus(
                Native.nimcp_brain_snapshot_save(handle, name, description));
        }

        public Brain SnapshotRestore(string name)
        {
            CheckOpen();
            IntPtr h = Native.nimcp_brain_snapshot_restore(handle, name);
            if (h == IntPtr.Zero)
                throw new NIMCPException("Failed to restore snapshot");
            return new Brain(h);
        }

        public void SnapshotDelete(string name)
        {
            CheckOpen();
            Helper.CheckStatus(
                Native.nimcp_brain_snapshot_delete(handle, name));
        }

        // --- COW ---

        public Brain CloneCow()
        {
            CheckOpen();
            IntPtr h = Native.nimcp_brain_clone_cow(handle);
            if (h == IntPtr.Zero)
                throw new NIMCPException("Failed to clone brain");
            return new Brain(h);
        }

        public BrainSnapshot SnapshotCow()
        {
            CheckOpen();
            IntPtr h = Native.nimcp_brain_snapshot_cow(handle);
            if (h == IntPtr.Zero)
                throw new NIMCPException("Failed to create snapshot");
            return new BrainSnapshot(h);
        }

        public void RestoreCow(BrainSnapshot snapshot)
        {
            CheckOpen();
            Helper.CheckStatus(
                Native.nimcp_brain_restore_cow(handle, snapshot.Handle));
        }

        // --- Working Memory ---

        public void WorkingMemoryAdd(float[] data, float salience)
        {
            CheckOpen();
            Helper.CheckStatus(Native.nimcp_brain_working_memory_add(
                handle, data, (uint)data.Length, salience));
        }

        public float[] WorkingMemoryGet(uint index)
        {
            CheckOpen();
            uint size = 0;
            IntPtr ptr = Native.nimcp_brain_working_memory_get(
                handle, index, ref size);
            if (ptr == IntPtr.Zero || size == 0) return null;
            float[] result = new float[size];
            Marshal.Copy(ptr, result, 0, (int)size);
            return result;
        }

        public WorkingMemoryStats GetWorkingMemoryStats()
        {
            CheckOpen();
            uint cur = 0, cap = 0;
            Helper.CheckStatus(Native.nimcp_brain_working_memory_stats(
                handle, ref cur, ref cap));
            return new WorkingMemoryStats
            { CurrentSize = cur, Capacity = cap };
        }

        public void WorkingMemoryRefresh(uint index)
        {
            CheckOpen();
            Helper.CheckStatus(
                Native.nimcp_brain_working_memory_refresh(handle, index));
        }

        // --- Workspace ---

        public void WorkspaceCompete(CognitiveModule module,
            float[] content, float strength)
        {
            CheckOpen();
            Helper.CheckStatus(Native.nimcp_brain_workspace_compete(
                handle, (int)module, content,
                (uint)content.Length, strength));
        }

        public WorkspaceReadResult WorkspaceRead(uint maxDim)
        {
            CheckOpen();
            float[] buf = new float[maxDim];
            uint actualDim = 0; int source = 0;
            Helper.CheckStatus(Native.nimcp_brain_workspace_read(
                handle, buf, maxDim, ref actualDim, ref source));
            float[] content = new float[actualDim];
            Array.Copy(buf, content, actualDim);
            return new WorkspaceReadResult
            {
                Content = content, ActualDim = actualDim,
                SourceModule = (CognitiveModule)source
            };
        }

        public void WorkspaceSubscribe(CognitiveModule module)
        {
            CheckOpen();
            Helper.CheckStatus(Native.nimcp_brain_workspace_subscribe(
                handle, (int)module));
        }

        public void WorkspaceUnsubscribe(CognitiveModule module)
        {
            CheckOpen();
            Helper.CheckStatus(Native.nimcp_brain_workspace_unsubscribe(
                handle, (int)module));
        }

        public bool WorkspaceHasBroadcast()
        {
            CheckOpen();
            bool has = false;
            Helper.CheckStatus(Native.nimcp_brain_workspace_has_broadcast(
                handle, ref has));
            return has;
        }

        public WorkspaceStats GetWorkspaceStats()
        {
            CheckOpen();
            uint bc = 0, comp = 0; float avg = 0;
            Helper.CheckStatus(Native.nimcp_brain_workspace_stats(
                handle, ref bc, ref comp, ref avg));
            return new WorkspaceStats
            {
                TotalBroadcasts = bc, TotalCompetitions = comp,
                AvgStrength = avg
            };
        }

        // --- Oscillations ---

        public bool EnableOscillations(bool enable)
        {
            CheckOpen();
            return Native.nimcp_enable_complex_oscillations(handle, enable);
        }

        public bool IsOscillationsEnabled()
        {
            CheckOpen();
            return Native.nimcp_is_complex_oscillations_enabled(handle);
        }

        public Phasor GetPhasor(uint neuronId)
        {
            CheckOpen();
            var np = Native.nimcp_get_oscillation_phasor(handle, neuronId);
            return new Phasor(np.amplitude, np.phase);
        }

        public float GetPhaseCoherence(uint[] neuronIds)
        {
            CheckOpen();
            return Native.nimcp_get_phase_coherence(
                handle, neuronIds, (uint)neuronIds.Length);
        }

        public float GetPacModulation(float thetaFreq, float gammaFreq)
        {
            CheckOpen();
            return Native.nimcp_get_pac_modulation(
                handle, thetaFreq, gammaFreq);
        }

        // --- Group 1: Sensory / Multimodal ---

        /// <summary>
        /// Stage sensory data for the next DecideFull() call.
        /// Supported modalities: "visual", "audio", "speech", "somatosensory"/"somato".
        /// </summary>
        public void SubmitSensory(string modality, float[] data,
            uint width = 0, uint height = 0, uint channels = 0, uint nSegments = 0)
        {
            CheckOpen();
            Helper.CheckStatus(Native.nimcp_brain_submit_sensory(
                handle, modality, data, (uint)data.Length,
                width, height, channels, nSegments));
        }

        /// <summary>
        /// Process image through visual cortex and return extracted features.
        /// Pixels should be in [0-1] or [0-255] range.
        /// </summary>
        public float[] VisualCortexProcess(float[] pixels, uint width,
            uint height, uint channels)
        {
            CheckOpen();
            uint maxFeatures = 512;
            float[] features = new float[maxFeatures];
            uint featureCount = 0;
            Helper.CheckStatus(Native.nimcp_brain_visual_cortex_process(
                handle, pixels, (uint)pixels.Length,
                width, height, channels,
                features, maxFeatures, ref featureCount));
            float[] result = new float[featureCount];
            Array.Copy(features, result, featureCount);
            return result;
        }

        // --- Group 2: Avatar / Metrics ---

        /// <summary>
        /// Get avatar visual state including FACS action units, visemes, gaze,
        /// emotion, and voice parameters.
        /// </summary>
        public Dictionary<string, object> GetAvatarState()
        {
            CheckOpen();
            var state = new Native.NativeAvatarState();
            Helper.CheckStatus(Native.nimcp_brain_get_avatar_state(
                handle, ref state));

            return new Dictionary<string, object>
            {
                { "mouth_open", state.mouth_open },
                { "lip_round", state.lip_round },
                { "lip_upper", state.lip_upper },
                { "lip_lower", state.lip_lower },
                { "tongue_position", state.tongue_position },
                { "current_viseme", (int)state.current_viseme },
                { "au1_inner_brow_raise", state.au1_inner_brow_raise },
                { "au2_outer_brow_raise", state.au2_outer_brow_raise },
                { "au4_brow_lower", state.au4_brow_lower },
                { "au5_upper_lid_raise", state.au5_upper_lid_raise },
                { "au6_cheek_raise", state.au6_cheek_raise },
                { "au7_lid_tighten", state.au7_lid_tighten },
                { "au9_nose_wrinkle", state.au9_nose_wrinkle },
                { "au10_upper_lip_raise", state.au10_upper_lip_raise },
                { "au12_lip_corner_pull", state.au12_lip_corner_pull },
                { "au15_lip_corner_drop", state.au15_lip_corner_drop },
                { "au17_chin_raise", state.au17_chin_raise },
                { "au20_lip_stretch", state.au20_lip_stretch },
                { "au23_lip_tighten", state.au23_lip_tighten },
                { "au25_lips_part", state.au25_lips_part },
                { "au26_jaw_drop", state.au26_jaw_drop },
                { "au28_lip_suck", state.au28_lip_suck },
                { "valence", state.valence },
                { "arousal", state.arousal },
                { "dominance", state.dominance },
                { "emotion_id", state.emotion_id },
                { "emotion_intensity", state.emotion_intensity },
                { "gaze_x", state.gaze_x },
                { "gaze_y", state.gaze_y },
                { "head_pitch", state.head_pitch },
                { "head_yaw", state.head_yaw },
                { "head_roll", state.head_roll },
                { "blink", state.blink },
                { "pitch_hz", state.pitch_hz },
                { "speaking_rate", state.speaking_rate },
                { "volume", state.volume },
                { "timestamp_us", state.timestamp_us },
                { "is_speaking", state.is_speaking }
            };
        }

        /// <summary>
        /// Get per-network training metrics (ANN/CNN/SNN/LNN loss + step counts).
        /// </summary>
        public Dictionary<string, object> GetNetworkMetrics()
        {
            CheckOpen();
            float emaAnn = 0, emaCnn = 0, emaSnn = 0, emaLnn = 0;
            ulong annSteps = 0, cnnSteps = 0, snnSteps = 0, lnnSteps = 0;
            bool ok = Native.nimcp_brain_get_network_metrics(
                handle,
                ref emaAnn, ref emaCnn, ref emaSnn, ref emaLnn,
                ref annSteps, ref cnnSteps, ref snnSteps, ref lnnSteps);

            if (!ok) return new Dictionary<string, object>();

            return new Dictionary<string, object>
            {
                { "ann_loss", emaAnn },
                { "cnn_loss", emaCnn },
                { "snn_loss", emaSnn },
                { "lnn_loss", emaLnn },
                { "ann_steps", annSteps },
                { "cnn_steps", cnnSteps },
                { "snn_steps", snnSteps },
                { "lnn_steps", lnnSteps }
            };
        }

        /// <summary>
        /// Get per-cortex CNN processor metrics.
        /// Returns dict with keys "visual"/"audio"/"speech"/"somato",
        /// each mapping to a metrics dictionary.
        /// </summary>
        public Dictionary<string, object> GetCortexCnnMetrics()
        {
            CheckOpen();
            int[] types = new int[4];
            float[] losses = new float[4];
            ulong[] fwdSteps = new ulong[4];
            ulong[] bwdSteps = new ulong[4];
            float[] embedNorms = new float[4];
            uint count = 0;

            Helper.CheckStatus(Native.nimcp_brain_get_cortex_cnn_metrics(
                handle, types, losses, fwdSteps, bwdSteps,
                embedNorms, ref count));

            string[] typeKeys = { "visual", "audio", "speech", "somato" };
            var result = new Dictionary<string, object>();

            for (uint i = 0; i < count && i < 4; i++)
            {
                var entry = new Dictionary<string, object>
                {
                    { "ema_loss", losses[i] },
                    { "forward_steps", fwdSteps[i] },
                    { "backward_steps", bwdSteps[i] },
                    { "embedding_norm", embedNorms[i] }
                };
                string key = (types[i] >= 0 && types[i] < typeKeys.Length)
                    ? typeKeys[types[i]] : "cortex_" + types[i];
                result[key] = entry;
            }

            return result;
        }

        // --- Group 3: Core Inference ---

        /// <summary>
        /// Run the full cognitive pipeline and return a rich result dictionary
        /// with label, confidence, explanation, output vector, active neuron
        /// count, sparsity, and inference time.
        /// </summary>
        public Dictionary<string, object> DecideFull(float[] features)
        {
            CheckOpen();
            var labelBuf = new StringBuilder(256);
            float confidence = 0;
            var explanationBuf = new StringBuilder(1024);
            float[] outputVector = new float[4096];
            uint outputSize = 4096;
            uint numActiveNeurons = 0;
            float sparsity = 0;
            ulong inferenceTimeUs = 0;

            Helper.CheckStatus(Native.nimcp_brain_decide_full(
                handle, features, (uint)features.Length,
                labelBuf, ref confidence, explanationBuf,
                outputVector, ref outputSize,
                ref numActiveNeurons, ref sparsity, ref inferenceTimeUs));

            uint vecLen = Math.Min(outputSize, 4096);
            float[] vec = new float[vecLen];
            Array.Copy(outputVector, vec, vecLen);

            return new Dictionary<string, object>
            {
                { "label", labelBuf.ToString() },
                { "confidence", confidence },
                { "explanation", explanationBuf.ToString() },
                { "output_vector", vec },
                { "num_active_neurons", numActiveNeurons },
                { "sparsity", sparsity },
                { "inference_time_us", inferenceTimeUs }
            };
        }

        /// <summary>
        /// Get the cognitive transcript from the last DecideFull() call.
        /// Each entry contains module name, summary, salience, and confidence.
        /// </summary>
        public List<Dictionary<string, object>> GetTranscript()
        {
            CheckOpen();
            const int maxEntries = 32;
            const int summaryLen = 256;

            // Allocate unmanaged memory for the summary strings array
            IntPtr entriesPtr = Marshal.AllocHGlobal(maxEntries * summaryLen);
            float[] saliences = new float[maxEntries];
            float[] confidences = new float[maxEntries];
            IntPtr modulesPtr = Marshal.AllocHGlobal(maxEntries * IntPtr.Size);

            try
            {
                // Zero out
                for (int i = 0; i < maxEntries * summaryLen; i++)
                    Marshal.WriteByte(entriesPtr, i, 0);
                for (int i = 0; i < maxEntries * IntPtr.Size; i++)
                    Marshal.WriteByte(modulesPtr, i, 0);

                uint count = Native.nimcp_brain_get_last_transcript(
                    handle, entriesPtr, saliences, confidences,
                    modulesPtr, (uint)maxEntries);

                var result = new List<Dictionary<string, object>>();
                for (uint i = 0; i < count; i++)
                {
                    IntPtr summaryAddr = IntPtr.Add(entriesPtr, (int)(i * summaryLen));
                    string summary = Marshal.PtrToStringAnsi(summaryAddr) ?? "";

                    IntPtr moduleAddr = Marshal.ReadIntPtr(modulesPtr, (int)(i * IntPtr.Size));
                    string module = moduleAddr != IntPtr.Zero
                        ? Marshal.PtrToStringAnsi(moduleAddr) ?? "unknown"
                        : "unknown";

                    result.Add(new Dictionary<string, object>
                    {
                        { "module", module },
                        { "summary", summary },
                        { "salience", saliences[i] },
                        { "confidence", confidences[i] }
                    });
                }
                return result;
            }
            finally
            {
                Marshal.FreeHGlobal(entriesPtr);
                Marshal.FreeHGlobal(modulesPtr);
            }
        }

        /// <summary>
        /// Get per-module cognitive training statistics.
        /// Returns dictionary with module names mapped to {steps, last_loss}.
        /// </summary>
        public Dictionary<string, object> GetCognitiveStats()
        {
            CheckOpen();
            uint[] steps = new uint[13];
            float[] losses = new float[13];
            uint count = 0;

            int status = Native.nimcp_brain_get_cognitive_stats(
                handle, steps, losses, ref count);

            string[] moduleNames =
            {
                "grounded_language", "knowledge", "vae", "fep_parietal",
                "physics_nn", "pred_hierarchy", "jepa", "creative",
                "self_heal", "intuition", "fep_orchestrator"
            };

            var result = new Dictionary<string, object>();
            if (status != 0) return result;

            for (uint i = 0; i < count && i < 11; i++)
            {
                result[moduleNames[i]] = new Dictionary<string, object>
                {
                    { "steps", steps[i] },
                    { "last_loss", losses[i] }
                };
            }

            return result;
        }

        /// <summary>
        /// Get running label-match accuracy (EMA).
        /// </summary>
        public float GetAccuracy()
        {
            CheckOpen();
            return Native.nimcp_brain_get_accuracy(handle);
        }

        // --- Group 4: LNN / SNN / CNN ---

        /// <summary>
        /// Create NCP-architecture LNN temporal processor.
        /// Idempotent: returns without error if already created.
        /// </summary>
        public void LnnCreate(uint nSensory = 128, uint nInter = 64,
            uint nCommand = 32, uint nOutput = 64)
        {
            CheckOpen();
            Helper.CheckStatus(Native.nimcp_brain_lnn_create(
                handle, nSensory, nInter, nCommand, nOutput));
        }

        /// <summary>
        /// Get LNN network statistics.
        /// Returns null if LNN is not initialized.
        /// </summary>
        public Dictionary<string, object> LnnGetStats()
        {
            CheckOpen();
            ulong fwdSteps = 0, bwdSteps = 0, odeEvals = 0;
            float avgTau = 0, stateNorm = 0, gradNorm = 0;
            uint nanCount = 0, infCount = 0;

            int status = Native.nimcp_brain_lnn_get_stats(
                handle,
                ref fwdSteps, ref bwdSteps, ref odeEvals,
                ref avgTau, ref stateNorm, ref gradNorm,
                ref nanCount, ref infCount);

            if (status != 0) return null;

            return new Dictionary<string, object>
            {
                { "forward_steps", fwdSteps },
                { "backward_steps", bwdSteps },
                { "total_ode_evals", odeEvals },
                { "avg_tau", avgTau },
                { "state_norm", stateNorm },
                { "gradient_norm", gradNorm },
                { "nan_count", nanCount },
                { "inf_count", infCount }
            };
        }

        /// <summary>
        /// Get SNN network statistics.
        /// Returns null if SNN is not initialized.
        /// </summary>
        public Dictionary<string, object> SnnGetStats()
        {
            CheckOpen();
            ulong totalSteps = 0, totalSpikes = 0;
            float meanFiringRate = 0, sparsity = 0, synchrony = 0;
            uint silentNeurons = 0, hyperactiveNeurons = 0;
            int health = 0;
            ulong memoryBytes = 0;

            int status = Native.nimcp_brain_snn_get_stats(
                handle,
                ref totalSteps, ref totalSpikes,
                ref meanFiringRate, ref sparsity,
                ref synchrony, ref silentNeurons,
                ref hyperactiveNeurons, ref health,
                ref memoryBytes);

            if (status != 0) return null;

            return new Dictionary<string, object>
            {
                { "total_steps", totalSteps },
                { "total_spikes", totalSpikes },
                { "mean_firing_rate", meanFiringRate },
                { "sparsity", sparsity },
                { "synchrony", synchrony },
                { "silent_neurons", silentNeurons },
                { "hyperactive_neurons", hyperactiveNeurons },
                { "health", health },
                { "memory_usage_bytes", memoryBytes }
            };
        }

        /// <summary>Set SNN input amplification scale.</summary>
        public static void SnnSetInputScale(float scale) => Native.nimcp_snn_set_input_scale(scale);

        /// <summary>Get current SNN input scale factor.</summary>
        public static float SnnGetInputScale() => Native.nimcp_snn_get_input_scale();

        /// <summary>
        /// Get CNN trainer statistics.
        /// Returns null if CNN trainer is not initialized.
        /// </summary>
        public Dictionary<string, object> CnnGetStats()
        {
            CheckOpen();
            uint numLayers = 0, numLabels = 0;
            ulong numParameters = 0;
            bool active = false;

            int status = Native.nimcp_brain_cnn_get_stats(
                handle,
                ref numLayers, ref numParameters,
                ref numLabels, ref active);

            if (status != 0) return null;

            return new Dictionary<string, object>
            {
                { "num_layers", numLayers },
                { "num_parameters", numParameters },
                { "num_labels", numLabels },
                { "active", active }
            };
        }

        // --- Group 5: Configuration ---

        /// <summary>
        /// Toggle fast training mode.
        /// </summary>
        public void SetFastTraining(bool enabled)
        {
            CheckOpen();
            Helper.CheckStatus(Native.nimcp_brain_set_fast_training(
                handle, enabled));
        }

        /// <summary>
        /// Set task strategy: "regression", "classification", "pattern", or "association".
        /// </summary>
        public void SetTaskType(string type)
        {
            CheckOpen();
            Helper.CheckStatus(Native.nimcp_brain_set_task_type(handle, type));
        }

        /// <summary>
        /// Enable/disable biological plasticity (TPB + EDP + coordinator).
        /// </summary>
        public void EnableBiologicalPlasticity(bool enabled)
        {
            CheckOpen();
            Helper.CheckStatus(
                Native.nimcp_brain_enable_biological_plasticity(handle, enabled));
        }

        /// <summary>
        /// Enable multi-network ensemble training (LNN + CNN + Adaptive).
        /// </summary>
        public void EnableMultiNetwork()
        {
            CheckOpen();
            Helper.CheckStatus(Native.nimcp_brain_enable_multi_network(handle));
        }

        // --- Group 6: Brain State ---

        /// <summary>
        /// Get medulla arousal level [0,1].
        /// </summary>
        public float MedullaGetArousal()
        {
            CheckOpen();
            return Native.nimcp_brain_medulla_get_arousal(handle);
        }

        /// <summary>
        /// Get sleep pressure [0,1].
        /// </summary>
        public float SleepGetPressure()
        {
            CheckOpen();
            return Native.nimcp_brain_sleep_get_pressure(handle);
        }

        /// <summary>
        /// Get basal ganglia dopamine level.
        /// </summary>
        public float BgGetDopamine()
        {
            CheckOpen();
            return Native.nimcp_brain_bg_get_dopamine(handle);
        }

        /// <summary>
        /// Get substrate health status ("OPTIMAL"/"STRESSED"/"COMPROMISED"/"CRITICAL"/"UNKNOWN").
        /// </summary>
        public string SubstrateGetHealth()
        {
            CheckOpen();
            var buf = new StringBuilder(64);
            Helper.CheckStatus(Native.nimcp_brain_substrate_get_health(
                handle, buf, 64));
            return buf.ToString();
        }

        /// <summary>
        /// Focus attention on a sensory modality (e.g., "visual", "audio").
        /// The actual gating is managed automatically by thalamic bridges
        /// during DecideFull().
        /// </summary>
        public void FocusAttention(string modality)
        {
            CheckOpen();
            Helper.CheckStatus(Native.nimcp_brain_focus_attention(
                handle, modality));
        }

        // --- Edge Brain ---

        /// <summary>
        /// Resize brain neuron count at runtime.
        /// Mode: "contract", "expand", or "rebalance".
        /// </summary>
        public Dictionary<string, object> EdgeResize(uint targetNeurons,
            string mode = "contract", bool knowledgeTransfer = true)
        {
            CheckOpen();
            var config = Native.nimcp_resize_config_default();
            config.target_neuron_count = targetNeurons;
            config.enable_knowledge_transfer = knowledgeTransfer;
            if (mode == "expand") config.mode = 1;
            else if (mode == "rebalance") config.mode = 2;
            else config.mode = 0;
            int ret = Native.nimcp_edge_brain_resize(handle, ref config);
            return new Dictionary<string, object>
            {
                { "status", ret },
                { "target_neurons", targetNeurons },
                { "mode", mode }
            };
        }

        /// <summary>
        /// Dry-run resize check: returns feasibility report.
        /// </summary>
        public Dictionary<string, object> EdgeResizeCheck(uint targetNeurons)
        {
            CheckOpen();
            var config = Native.nimcp_resize_config_default();
            config.target_neuron_count = targetNeurons;
            config.mode = 0;
            var report = new Native.NativeResizeReport();
            Native.nimcp_edge_brain_resize_check(handle, ref config, ref report);
            return new Dictionary<string, object>
            {
                { "feasible", report.feasible },
                { "neurons_before", report.neurons_before },
                { "neurons_after", report.neurons_after },
                { "ram_delta_mb", report.estimated_ram_delta_mb },
                { "reason", report.reason ?? "" }
            };
        }

        /// <summary>
        /// Distill brain into a smaller student brain.
        /// </summary>
        public Dictionary<string, object> EdgeDistill(uint targetNeurons,
            float temperature = 2.0f, uint steps = 5000,
            bool includeSnn = false, bool includeLnn = false, bool includeCnn = true)
        {
            CheckOpen();
            var config = Native.nimcp_distill_config_default();
            config.target_neurons = targetNeurons;
            config.temperature = temperature;
            config.distillation_steps = steps;
            config.include_snn = includeSnn;
            config.include_lnn = includeLnn;
            config.include_cnn = includeCnn;
            var report = new Native.NativeDistillReport();
            IntPtr student = IntPtr.Zero;
            int ret = Native.nimcp_brain_distill(handle, ref student, ref config, ref report);
            return new Dictionary<string, object>
            {
                { "status", ret },
                { "accuracy_retention", report.accuracy_retention },
                { "neurons_selected", report.neurons_selected },
                { "compression_ratio", report.compression_ratio },
                { "teacher_loss", report.teacher_loss },
                { "student_loss", report.student_loss },
                { "steps_trained", report.steps_trained }
            };
        }

        /// <summary>
        /// Auto-optimize brain for a device profile.
        /// </summary>
        public Dictionary<string, object> EdgeOptimizeForDevice(uint ramMb,
            uint cpuCores = 2, bool hasCamera = false, bool hasImu = false,
            bool hasMotorControl = false, bool hasNetwork = true,
            string role = "general")
        {
            CheckOpen();
            var profile = Native.nimcp_device_profile_default();
            profile.ram_mb = ramMb;
            profile.cpu_cores = cpuCores;
            profile.has_camera = hasCamera;
            profile.has_imu = hasImu;
            profile.has_motor_control = hasMotorControl;
            profile.has_network = hasNetwork;
            if (role == "sensor") profile.role = 1;
            else if (role == "actuator") profile.role = 2;
            else if (role == "coordinator") profile.role = 3;
            else profile.role = 0;
            var report = new Native.NativeOptimizationReport();
            IntPtr child = IntPtr.Zero;
            int ret = Native.nimcp_brain_optimize_for_device(
                handle, ref profile, ref child, ref report);
            return new Dictionary<string, object>
            {
                { "status", ret },
                { "neuron_count", report.neuron_count },
                { "subsystems_enabled", report.subsystems_enabled },
                { "estimated_ram_mb", report.estimated_ram_mb },
                { "estimated_inference_ms", report.estimated_inference_ms },
                { "accuracy_retention", report.accuracy_retention }
            };
        }

        /// <summary>
        /// Quantize brain weights in-place.
        /// </summary>
        public Dictionary<string, object> EdgeQuantize(
            string precision = "int8_symmetric", uint calibrationSamples = 100)
        {
            CheckOpen();
            var config = Native.nimcp_quantize_config_default();
            config.calibration_samples = calibrationSamples;
            if (precision == "fp16") config.weight_precision = 1;
            else if (precision == "int8_affine") config.weight_precision = 2;
            else if (precision == "int4") config.weight_precision = 3;
            else if (precision == "ternary") config.weight_precision = 4;
            else config.weight_precision = 0;
            int ret = Native.nimcp_brain_quantize(handle, ref config);
            return new Dictionary<string, object>
            {
                { "status", ret },
                { "precision", precision }
            };
        }

        /// <summary>
        /// Score neuron importance (activity, connectivity, weight magnitude).
        /// </summary>
        public float[] EdgeScoreImportance(uint numNeurons = 1000)
        {
            CheckOpen();
            float[] scores = new float[numNeurons];
            Native.nimcp_edge_score_neuron_importance(handle, scores, numNeurons);
            return scores;
        }

        // --- Swarm / Sensor / Watchdog / ROS2 / MAVLink ---
        // Note: These use opaque config structs via IntPtr. For full config support,
        // use the C API defaults and pass IntPtr.Zero for default config.

        private IntPtr swarmMaster, swarmEdge, sensorHub, safetyWatchdog, ros2Bridge, mavlinkBridge;

        public bool SwarmMasterCreate(uint deviceId = 1, uint listenPort = 9200) { CheckOpen(); var cfg = Native.nimcp_swarm_master_config_default(); swarmMaster = Native.nimcp_swarm_master_create(handle, cfg); return swarmMaster != IntPtr.Zero; }
        public void SwarmMasterDestroy() { if (swarmMaster != IntPtr.Zero) { Native.nimcp_swarm_master_destroy(swarmMaster); swarmMaster = IntPtr.Zero; } }
        public int SwarmMasterStart() { return swarmMaster == IntPtr.Zero ? -1 : Native.nimcp_swarm_master_start(swarmMaster); }
        public int SwarmMasterStop() { return swarmMaster == IntPtr.Zero ? -1 : Native.nimcp_swarm_master_stop(swarmMaster); }
        public int SwarmMasterKick(uint deviceId) { return swarmMaster == IntPtr.Zero ? -1 : Native.nimcp_swarm_master_kick(swarmMaster, deviceId); }
        public int SwarmMasterForceSync() { return swarmMaster == IntPtr.Zero ? -1 : Native.nimcp_swarm_master_force_sync(swarmMaster); }
        public uint SwarmMasterGetPeerCount() { return swarmMaster == IntPtr.Zero ? 0 : Native.nimcp_swarm_master_get_peer_count(swarmMaster); }

        public bool SwarmEdgeCreate(uint deviceId = 2) { CheckOpen(); var cfg = Native.nimcp_swarm_edge_config_default(); swarmEdge = Native.nimcp_swarm_edge_create(handle, cfg); return swarmEdge != IntPtr.Zero; }
        public void SwarmEdgeDestroy() { if (swarmEdge != IntPtr.Zero) { Native.nimcp_swarm_edge_destroy(swarmEdge); swarmEdge = IntPtr.Zero; } }
        public int SwarmEdgeStart() { return swarmEdge == IntPtr.Zero ? -1 : Native.nimcp_swarm_edge_start(swarmEdge); }
        public int SwarmEdgeStop() { return swarmEdge == IntPtr.Zero ? -1 : Native.nimcp_swarm_edge_stop(swarmEdge); }
        public bool SwarmEdgeIsConnected() { return swarmEdge != IntPtr.Zero && Native.nimcp_swarm_edge_is_connected(swarmEdge); }
        public int SwarmEdgeSubmitGradients(float[] gradients) { return swarmEdge == IntPtr.Zero ? -1 : Native.nimcp_swarm_edge_submit_gradients(swarmEdge, gradients, (uint)gradients.Length); }

        public bool SensorHubCreate(uint maxSensors = 32) { sensorHub = Native.nimcp_sensor_hub_create(maxSensors); return sensorHub != IntPtr.Zero; }
        public void SensorHubDestroy() { if (sensorHub != IntPtr.Zero) { Native.nimcp_sensor_hub_destroy(sensorHub); sensorHub = IntPtr.Zero; } }
        public uint SensorGetCount() { return sensorHub == IntPtr.Zero ? 0 : Native.nimcp_sensor_get_count(sensorHub); }
        public float[] SensorComposeFeatures(uint maxFeatures = 1024) {
            if (sensorHub == IntPtr.Zero) return new float[0];
            float[] features = new float[maxFeatures];
            int count = Native.nimcp_sensor_compose_feature_vector(sensorHub, features, maxFeatures);
            if (count < 0) return new float[0];
            float[] result = new float[count]; Array.Copy(features, result, count); return result;
        }

        public bool WatchdogCreate(uint timeoutMs = 500, float maxMagnitude = 1.0f) { var cfg = Native.nimcp_watchdog_config_default(); safetyWatchdog = Native.nimcp_watchdog_create(cfg); return safetyWatchdog != IntPtr.Zero; }
        public void WatchdogDestroy() { if (safetyWatchdog != IntPtr.Zero) { Native.nimcp_watchdog_destroy(safetyWatchdog); safetyWatchdog = IntPtr.Zero; } }
        public int WatchdogArm() { return safetyWatchdog == IntPtr.Zero ? -1 : Native.nimcp_watchdog_arm(safetyWatchdog); }
        public int WatchdogDisarm() { return safetyWatchdog == IntPtr.Zero ? -1 : Native.nimcp_watchdog_disarm(safetyWatchdog); }
        public void WatchdogHeartbeat() { if (safetyWatchdog != IntPtr.Zero) Native.nimcp_watchdog_heartbeat(safetyWatchdog); }
        public bool WatchdogValidateOutput(float[] output) { return safetyWatchdog != IntPtr.Zero && Native.nimcp_watchdog_validate_output(safetyWatchdog, output, (uint)output.Length) == 0; }
        public float[] WatchdogGetSafeOutput(uint numOutputs = 32) {
            if (safetyWatchdog == IntPtr.Zero) return new float[0];
            float[] output = new float[numOutputs];
            Native.nimcp_watchdog_get_safe_output(safetyWatchdog, output, numOutputs); return output;
        }
        public void WatchdogEstop() { if (safetyWatchdog != IntPtr.Zero) Native.nimcp_watchdog_estop(safetyWatchdog); }
        public int WatchdogReset() { return safetyWatchdog == IntPtr.Zero ? -1 : Native.nimcp_watchdog_reset(safetyWatchdog); }
        public string WatchdogGetState() {
            if (safetyWatchdog == IntPtr.Zero) return "NONE";
            int state = Native.nimcp_watchdog_get_state(safetyWatchdog);
            IntPtr namePtr = Native.nimcp_watchdog_state_name(state);
            return namePtr == IntPtr.Zero ? "UNKNOWN" : Marshal.PtrToStringAnsi(namePtr);
        }

        public bool Ros2BridgeCreate() { CheckOpen(); var cfg = Native.nimcp_ros2_config_default(); ros2Bridge = Native.nimcp_ros2_bridge_create(handle, cfg); return ros2Bridge != IntPtr.Zero; }
        public void Ros2BridgeDestroy() { if (ros2Bridge != IntPtr.Zero) { Native.nimcp_ros2_bridge_destroy(ros2Bridge); ros2Bridge = IntPtr.Zero; } }
        public int Ros2BridgeStart() { return ros2Bridge == IntPtr.Zero ? -1 : Native.nimcp_ros2_bridge_start(ros2Bridge); }
        public int Ros2BridgeStop() { return ros2Bridge == IntPtr.Zero ? -1 : Native.nimcp_ros2_bridge_stop(ros2Bridge); }
        public int Ros2BridgeInjectSensor(string topic, float[] data) { return ros2Bridge == IntPtr.Zero ? -1 : Native.nimcp_ros2_bridge_inject_sensor(ros2Bridge, topic, data, (uint)data.Length); }
        public float[] Ros2BridgeGetLastCmd(uint maxCount = 32) {
            if (ros2Bridge == IntPtr.Zero) return new float[0];
            float[] data = new float[maxCount];
            int got = Native.nimcp_ros2_bridge_get_last_cmd(ros2Bridge, data, maxCount);
            if (got < 0) return new float[0];
            float[] result = new float[got]; Array.Copy(data, result, got); return result;
        }

        public bool MavlinkCreate(string connString = "udp:14550") { var cfg = Native.nimcp_mavlink_config_default(); mavlinkBridge = Native.nimcp_mavlink_bridge_create(cfg); return mavlinkBridge != IntPtr.Zero; }
        public void MavlinkDestroy() { if (mavlinkBridge != IntPtr.Zero) { Native.nimcp_mavlink_bridge_destroy(mavlinkBridge); mavlinkBridge = IntPtr.Zero; } }
        public int MavlinkConnect() { return mavlinkBridge == IntPtr.Zero ? -1 : Native.nimcp_mavlink_bridge_connect(mavlinkBridge); }
        public int MavlinkDisconnect() { return mavlinkBridge == IntPtr.Zero ? -1 : Native.nimcp_mavlink_bridge_disconnect(mavlinkBridge); }
        public int MavlinkStart() { return mavlinkBridge == IntPtr.Zero ? -1 : Native.nimcp_mavlink_bridge_start(mavlinkBridge); }
        public int MavlinkStop() { return mavlinkBridge == IntPtr.Zero ? -1 : Native.nimcp_mavlink_bridge_stop(mavlinkBridge); }
        public int MavlinkSetVelocity(float vx, float vy, float vz, float yawRate) { return mavlinkBridge == IntPtr.Zero ? -1 : Native.nimcp_mavlink_set_velocity(mavlinkBridge, vx, vy, vz, yawRate); }
        public int MavlinkArm(bool arm = true) { return mavlinkBridge == IntPtr.Zero ? -1 : Native.nimcp_mavlink_arm(mavlinkBridge, arm); }
        public int MavlinkTakeoff(float altitude = 5.0f) { return mavlinkBridge == IntPtr.Zero ? -1 : Native.nimcp_mavlink_takeoff(mavlinkBridge, altitude); }
        public int MavlinkLand() { return mavlinkBridge == IntPtr.Zero ? -1 : Native.nimcp_mavlink_land(mavlinkBridge); }
        public int MavlinkGoto(double lat, double lon, float alt = 10.0f) { return mavlinkBridge == IntPtr.Zero ? -1 : Native.nimcp_mavlink_goto(mavlinkBridge, lat, lon, alt); }
        public int MavlinkRtl() { return mavlinkBridge == IntPtr.Zero ? -1 : Native.nimcp_mavlink_rtl(mavlinkBridge); }
        public float[] MavlinkComposeFeatures() {
            if (mavlinkBridge == IntPtr.Zero) return new float[0];
            float[] features = new float[14]; // NIMCP_MAVLINK_FEATURE_COUNT
            int count = Native.nimcp_mavlink_compose_features(mavlinkBridge, features, 14);
            if (count < 0) return new float[0];
            float[] result = new float[count]; Array.Copy(features, result, count); return result;
        }

        // --- Memory Store ---

        /// <summary>
        /// Get memory store statistics.
        /// </summary>
        public Dictionary<string, object> MemoryStoreStats()
        {
            CheckOpen();
            var stats = new Native.NativeMemoryStoreStats();
            int ret = Native.nimcp_brain_memory_store_stats(handle, ref stats);
            if (ret != 0) return null;
            return new Dictionary<string, object>
            {
                { "total_engrams", stats.total_engrams },
                { "total_concepts", stats.total_concepts },
                { "total_relations", stats.total_relations },
                { "total_autobio", stats.total_autobio },
                { "total_writes", stats.total_writes },
                { "total_reads", stats.total_reads },
                { "cache_hits", stats.cache_hits },
                { "cache_misses", stats.cache_misses },
                { "db_size_bytes", stats.db_size_bytes }
            };
        }

        /// <summary>
        /// Search memory by text (FTS5 on labels).
        /// </summary>
        public ulong[] MemorySearchText(string query, uint maxResults = 10)
        {
            CheckOpen();
            ulong[] ids = new ulong[maxResults];
            uint count = 0;
            Native.nimcp_brain_memory_search_text(
                handle, query, maxResults, ids, ref count);
            ulong[] result = new ulong[count];
            Array.Copy(ids, result, count);
            return result;
        }

        /// <summary>
        /// Search memory by embedding similarity.
        /// Returns array of (id, distance) tuples.
        /// </summary>
        public (ulong Id, float Distance)[] MemorySearchSimilar(
            float[] embedding, uint topK = 5)
        {
            CheckOpen();
            ulong[] ids = new ulong[topK];
            float[] distances = new float[topK];
            uint count = 0;
            Native.nimcp_brain_memory_search_similar(
                handle, embedding, (uint)embedding.Length,
                topK, ids, distances, ref count);
            var result = new (ulong, float)[count];
            for (uint i = 0; i < count; i++)
                result[i] = (ids[i], distances[i]);
            return result;
        }

        /// <summary>
        /// Check if memory store is healthy.
        /// </summary>
        public bool MemoryIsHealthy()
        {
            CheckOpen();
            return Native.nimcp_brain_memory_is_healthy(handle);
        }

        // --- OOD Detection ---

        /// <summary>
        /// Get out-of-distribution detection statistics.
        /// </summary>
        public Dictionary<string, object> OodStats()
        {
            CheckOpen();
            var stats = new Native.NativeOodStats();
            int ret = Native.nimcp_brain_ood_stats(handle, ref stats);
            if (ret != 0) return null;
            return new Dictionary<string, object>
            {
                { "total_checks", stats.total_checks },
                { "ood_detected", stats.ood_detected },
                { "in_distribution", stats.in_distribution },
                { "avg_ood_score", stats.avg_ood_score },
                { "ood_rate", stats.ood_rate }
            };
        }

        // --- Audit Trail ---

        /// <summary>
        /// Log an audit event.
        /// </summary>
        public int AuditLog(string description, uint severity = 0,
            string details = "")
        {
            CheckOpen();
            return Native.nimcp_brain_audit_log(
                handle, description, severity, details);
        }

        /// <summary>
        /// Search audit trail by minimum severity.
        /// Returns array of (id, severity) tuples.
        /// </summary>
        public (ulong Id, float Severity)[] AuditSearch(
            uint minSeverity = 0, uint maxResults = 100)
        {
            CheckOpen();
            ulong[] ids = new ulong[maxResults];
            float[] severities = new float[maxResults];
            uint count = 0;
            Native.nimcp_brain_audit_search(
                handle, minSeverity, maxResults,
                ids, severities, ref count);
            var result = new (ulong, float)[count];
            for (uint i = 0; i < count; i++)
                result[i] = (ids[i], severities[i]);
            return result;
        }

        // --- Probe ---

        public BrainProbe Probe()
        {
            CheckOpen();
            var np = new Native.NativeBrainProbe();
            Helper.CheckStatus(Native.nimcp_brain_probe(handle, ref np));
            return new BrainProbe
            {
                TaskName = np.task_name, Size = (BrainSize)np.size,
                Task = (BrainTask)np.task, NumNeurons = np.num_neurons,
                NumSynapses = np.num_synapses,
                NumActiveSynapses = np.num_active_synapses,
                TotalInferences = np.total_inferences,
                TotalLearningSteps = np.total_learning_steps,
                AvgSparsity = np.avg_sparsity,
                AvgInferenceTimeUs = np.avg_inference_time_us,
                CurrentLearningRate = np.current_learning_rate,
                Accuracy = np.accuracy, MemoryBytes = np.memory_bytes,
                NumInputs = np.num_inputs, NumOutputs = np.num_outputs,
                IsCowClone = np.is_cow_clone, CowRefCount = np.cow_ref_count,
                CowSharedBytes = np.cow_shared_bytes,
                CowPrivateBytes = np.cow_private_bytes
            };
        }

        public void BroadcastProbe()
        {
            CheckOpen();
            Helper.CheckStatus(Native.nimcp_brain_broadcast_probe(handle));
        }

        // --- IDisposable ---

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!disposed)
            {
                if (handle != IntPtr.Zero)
                {
                    Native.nimcp_brain_destroy(handle);
                    handle = IntPtr.Zero;
                }
                disposed = true;
            }
        }

        ~Brain() { Dispose(false); }
    }

    // ========================================================================
    // BrainSnapshot (COW)
    // ========================================================================

    public class BrainSnapshot : IDisposable
    {
        internal IntPtr Handle { get; private set; }
        private bool disposed;

        internal BrainSnapshot(IntPtr handle) { Handle = handle; }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!disposed)
            {
                if (Handle != IntPtr.Zero)
                {
                    Native.nimcp_brain_snapshot_destroy(Handle);
                    Handle = IntPtr.Zero;
                }
                disposed = true;
            }
        }

        ~BrainSnapshot() { Dispose(false); }
    }

    // ========================================================================
    // Network Class
    // ========================================================================

    public class Network : IDisposable
    {
        private IntPtr handle;
        private bool disposed;

        public Network(uint numInputs, uint numOutputs,
                       uint numHidden, float learningRate = 0.01f)
        {
            handle = Native.nimcp_network_create(
                numInputs, numOutputs, numHidden, learningRate);
            if (handle == IntPtr.Zero)
                throw new NIMCPException("Failed to create network");
        }

        public void Forward(float[] inputs, float[] outputs)
        {
            if (disposed) throw new ObjectDisposedException(nameof(Network));
            Helper.CheckStatus(Native.nimcp_network_forward(
                handle, inputs, (uint)inputs.Length,
                outputs, (uint)outputs.Length));
        }

        public void Train(float[] inputs, float[] targets)
        {
            if (disposed) throw new ObjectDisposedException(nameof(Network));
            Helper.CheckStatus(Native.nimcp_network_train(
                handle, inputs, (uint)inputs.Length,
                targets, (uint)targets.Length));
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!disposed)
            {
                if (handle != IntPtr.Zero)
                {
                    Native.nimcp_network_destroy(handle);
                    handle = IntPtr.Zero;
                }
                disposed = true;
            }
        }

        ~Network() { Dispose(false); }
    }

    // ========================================================================
    // Ethics Class
    // ========================================================================

    public class Ethics : IDisposable
    {
        private IntPtr handle;
        private bool disposed;

        public Ethics()
        {
            handle = Native.nimcp_ethics_create();
            if (handle == IntPtr.Zero)
                throw new NIMCPException("Failed to create ethics module");
        }

        public float Check(float[] situation)
        {
            if (disposed) throw new ObjectDisposedException(nameof(Ethics));
            float score = 0;
            Helper.CheckStatus(Native.nimcp_ethics_check(
                handle, situation, (uint)situation.Length, ref score));
            return score;
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!disposed)
            {
                if (handle != IntPtr.Zero)
                {
                    Native.nimcp_ethics_destroy(handle);
                    handle = IntPtr.Zero;
                }
                disposed = true;
            }
        }

        ~Ethics() { Dispose(false); }
    }

    // ========================================================================
    // KnowledgeGraph Class
    // ========================================================================

    public class KnowledgeGraph : IDisposable
    {
        private IntPtr handle;
        private bool disposed;

        public KnowledgeGraph()
        {
            handle = Native.nimcp_knowledge_create();
            if (handle == IntPtr.Zero)
                throw new NIMCPException("Failed to create knowledge graph");
        }

        public void AddFact(string subject, string predicate, string obj)
        {
            if (disposed) throw new ObjectDisposedException(nameof(KnowledgeGraph));
            Helper.CheckStatus(Native.nimcp_knowledge_add_fact(
                handle, subject, predicate, obj));
        }

        public string Query(string query, uint maxLen = 1024)
        {
            if (disposed) throw new ObjectDisposedException(nameof(KnowledgeGraph));
            var buf = new StringBuilder((int)maxLen);
            Helper.CheckStatus(Native.nimcp_knowledge_query(
                handle, query, buf, maxLen));
            return buf.ToString();
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!disposed)
            {
                if (handle != IntPtr.Zero)
                {
                    Native.nimcp_knowledge_destroy(handle);
                    handle = IntPtr.Zero;
                }
                disposed = true;
            }
        }

        ~KnowledgeGraph() { Dispose(false); }
    }
}
