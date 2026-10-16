// Package nimcp provides Go bindings for the NIMCP C library via CGo.
//
// This package wraps the entire public C API (nimcp.h v2.6.3) including:
//   - Brain: high-level learning, training pipeline, callbacks, COW, workspace
//   - Network: low-level neural network
//   - Ethics: ethical evaluation
//   - KnowledgeGraph: fact storage and querying
//
// All handle types implement io.Closer for deterministic resource cleanup.
package nimcp

/*
#cgo CFLAGS: -I${SRCDIR}/../../../include -I${SRCDIR}
#cgo LDFLAGS: -L${SRCDIR}/../../../build/lib -lnimcp -Wl,-rpath,${SRCDIR}/../../../build/lib
#include "nimcp.h"
#include "nimcp_go_helpers.h"
#include "edge/nimcp_edge.h"
#include "edge/nimcp_swarm_runtime.h"
// Forward decls: SNN input scale (no public header)
extern void nimcp_snn_set_input_scale(float);
extern float nimcp_snn_get_input_scale(void);
#include "edge/nimcp_sensor.h"
#include "edge/nimcp_safety_watchdog.h"
#include "edge/nimcp_ros2_bridge.h"
#include "edge/nimcp_mavlink_bridge.h"
#include <stdlib.h>
#include <string.h>

// Forward declaration of Go callback dispatcher (defined via //export below)
// Note: no 'const' — CGo //export doesn't support const pointer params
extern nimcp_callback_action_t goCallbackTrampoline(
    nimcp_callback_event_t event,
    nimcp_callback_metrics_t* metrics,
    void* user_data);
*/
import "C"

import (
	"fmt"
	"sync"
	"unsafe"
)

// ============================================================================
// Error Types
// ============================================================================

// ErrorCode represents NIMCP status codes.
type ErrorCode int

const (
	OK         ErrorCode = 0
	ErrGeneric ErrorCode = 1000
	ErrNullArg ErrorCode = 1003
	ErrInvalid ErrorCode = 1004
	ErrMemory  ErrorCode = 2000
	ErrIO      ErrorCode = 4000
)

// NimcpError represents an error returned by the NIMCP C library.
type NimcpError struct {
	Code    ErrorCode
	Message string
}

func (e *NimcpError) Error() string {
	return fmt.Sprintf("nimcp error %d: %s", e.Code, e.Message)
}

func checkStatus(status C.nimcp_status_t) error {
	if status == C.NIMCP_OK {
		return nil
	}
	code := ErrorCode(status)
	msg := C.GoString(C.nimcp_get_error())
	if msg == "" {
		msg = fmt.Sprintf("error code %d", code)
	}
	return &NimcpError{Code: code, Message: msg}
}

// ============================================================================
// Enums
// ============================================================================

// BrainSize represents brain size presets.
type BrainSize int

const (
	BrainTiny   BrainSize = 0
	BrainSmall  BrainSize = 1
	BrainMedium BrainSize = 2
	BrainLarge  BrainSize = 3
)

// BrainTask represents task templates.
type BrainTask int

const (
	TaskClassification  BrainTask = 0
	TaskRegression      BrainTask = 1
	TaskPatternMatching BrainTask = 2
	TaskSequence        BrainTask = 3
	TaskAssociation     BrainTask = 4
)

// NetworkType represents network architecture types.
type NetworkType int

const (
	NetworkAdaptive NetworkType = 0
	NetworkSNN      NetworkType = 1
	NetworkLNN      NetworkType = 2
	NetworkCNN      NetworkType = 3
	NetworkHybrid   NetworkType = 4
)

// SNNTrainMethod represents SNN training methods.
type SNNTrainMethod int

const (
	SNNTrainSTDP        SNNTrainMethod = 0
	SNNTrainRSTDP       SNNTrainMethod = 1
	SNNTrainEProp       SNNTrainMethod = 2
	SNNTrainSurrogate   SNNTrainMethod = 3
	SNNTrainHomeostatic SNNTrainMethod = 4
)

// LNNTrainMethod represents LNN training methods.
type LNNTrainMethod int

const (
	LNNTrainAdjoint LNNTrainMethod = 0
	LNNTrainBPTT    LNNTrainMethod = 1
	LNNTrainRTRL    LNNTrainMethod = 2
	LNNTrainEProp   LNNTrainMethod = 3
)

// LossType represents loss function types.
type LossType int

const (
	LossMSE          LossType = 0
	LossCrossEntropy LossType = 1
	LossBinaryCE     LossType = 2
	LossHuber        LossType = 3
	LossMAE          LossType = 4
	LossFocal        LossType = 5
	LossKLDiv        LossType = 6
)

// OptimizerType represents optimizer types.
type OptimizerType int

const (
	OptSGD      OptimizerType = 0
	OptMomentum OptimizerType = 1
	OptAdam     OptimizerType = 2
	OptAdamW    OptimizerType = 3
	OptRMSProp  OptimizerType = 4
	OptAdagrad  OptimizerType = 5
)

// SchedulerType represents LR scheduler types.
type SchedulerType int

const (
	SchedConstant        SchedulerType = 0
	SchedStep            SchedulerType = 1
	SchedExponential     SchedulerType = 2
	SchedCosine          SchedulerType = 3
	SchedWarmupCosine    SchedulerType = 4
	SchedReduceOnPlateau SchedulerType = 5
	SchedCyclic          SchedulerType = 6
)

// CallbackEvent represents training callback event types.
type CallbackEvent int

const (
	CBStepComplete   CallbackEvent = 0
	CBEpochComplete  CallbackEvent = 1
	CBLossComputed   CallbackEvent = 2
	CBWeightsUpdated CallbackEvent = 3
	CBLRChanged      CallbackEvent = 4
	CBConvergence    CallbackEvent = 5
	CBDivergence     CallbackEvent = 6
	CBCheckpoint     CallbackEvent = 7
)

// CallbackAction represents callback return actions.
type CallbackAction int

const (
	CallbackActionContinue   CallbackAction = 0
	CallbackActionStop       CallbackAction = 1
	CallbackActionSkip       CallbackAction = 2
	CallbackActionRollback   CallbackAction = 3
	CallbackActionReduceLR   CallbackAction = 4
	CallbackActionIncreaseLR CallbackAction = 5
)

// CognitiveModule represents cognitive module identifiers.
type CognitiveModule int

const (
	ModuleNone             CognitiveModule = 0
	ModulePerception       CognitiveModule = 1
	ModuleWorkingMemory    CognitiveModule = 2
	ModuleExecutive        CognitiveModule = 3
	ModuleTheoryOfMind     CognitiveModule = 4
	ModuleEthics           CognitiveModule = 5
	ModuleAttention        CognitiveModule = 6
	ModuleEmotion          CognitiveModule = 7
	ModuleSalience         CognitiveModule = 8
	ModuleMotor            CognitiveModule = 9
	ModuleLanguage         CognitiveModule = 10
	ModuleMetacognition    CognitiveModule = 11
	ModuleCuriosity        CognitiveModule = 12
	ModuleIntrospection    CognitiveModule = 13
	ModulePredictive       CognitiveModule = 14
	ModuleConsolidation    CognitiveModule = 15
	ModuleEpisodicMemory   CognitiveModule = 16
	ModuleSemanticMemory   CognitiveModule = 17
	ModuleWellbeing        CognitiveModule = 18
	ModuleMentalHealth     CognitiveModule = 19
	ModuleGoalMotivation   CognitiveModule = 20
	ModuleCognitiveControl CognitiveModule = 21
	ModuleCustomStart      CognitiveModule = 100
)

// ============================================================================
// Data Structures
// ============================================================================

// TrainingConfig holds training pipeline configuration.
type TrainingConfig struct {
	LossType      LossType
	OptimizerType OptimizerType
	SchedulerType SchedulerType

	LearningRate float32
	WeightDecay  float32
	Momentum     float32
	Beta1        float32
	Beta2        float32
	Epsilon      float32

	SchedulerStepSize uint32
	SchedulerGamma    float32
	WarmupSteps       uint32

	EnableGradientClipping bool
	GradientClipValue      float32

	EnableBiologicalModulation bool
	BiologicalBlend            float32

	NetworkType NetworkType

	SNNMethod        SNNTrainMethod
	SNNEligibilityTau float32
	SNNRewardTau      float32
	SNNSurrogateBeta  float32

	LNNMethod                  LNNTrainMethod
	LNNBPTTTruncation          uint32
	LNNUseAdjointCheckpointing bool
}

func (tc *TrainingConfig) toC() C.nimcp_training_config_t {
	var c C.nimcp_training_config_t
	c.loss_type = C.nimcp_api_loss_t(tc.LossType)
	c.optimizer_type = C.nimcp_api_optimizer_t(tc.OptimizerType)
	c.scheduler_type = C.nimcp_api_scheduler_t(tc.SchedulerType)
	c.learning_rate = C.float(tc.LearningRate)
	c.weight_decay = C.float(tc.WeightDecay)
	c.momentum = C.float(tc.Momentum)
	c.beta1 = C.float(tc.Beta1)
	c.beta2 = C.float(tc.Beta2)
	c.epsilon = C.float(tc.Epsilon)
	c.scheduler_step_size = C.uint(tc.SchedulerStepSize)
	c.scheduler_gamma = C.float(tc.SchedulerGamma)
	c.warmup_steps = C.uint(tc.WarmupSteps)
	c.enable_gradient_clipping = C.bool(tc.EnableGradientClipping)
	c.gradient_clip_value = C.float(tc.GradientClipValue)
	c.enable_biological_modulation = C.bool(tc.EnableBiologicalModulation)
	c.biological_blend = C.float(tc.BiologicalBlend)
	c.network_type = C.nimcp_network_type_t(tc.NetworkType)
	c.snn_method = C.nimcp_snn_train_method_t(tc.SNNMethod)
	c.snn_eligibility_tau = C.float(tc.SNNEligibilityTau)
	c.snn_reward_tau = C.float(tc.SNNRewardTau)
	c.snn_surrogate_beta = C.float(tc.SNNSurrogateBeta)
	c.lnn_method = C.nimcp_lnn_train_method_t(tc.LNNMethod)
	c.lnn_bptt_truncation = C.uint(tc.LNNBPTTTruncation)
	c.lnn_use_adjoint_checkpointing = C.bool(tc.LNNUseAdjointCheckpointing)
	return c
}

// DefaultTrainingConfig returns sensible default training configuration.
func DefaultTrainingConfig() TrainingConfig {
	c := C.nimcp_training_config_default()
	return TrainingConfig{
		LossType:                   LossType(c.loss_type),
		OptimizerType:              OptimizerType(c.optimizer_type),
		SchedulerType:              SchedulerType(c.scheduler_type),
		LearningRate:               float32(c.learning_rate),
		WeightDecay:                float32(c.weight_decay),
		Momentum:                   float32(c.momentum),
		Beta1:                      float32(c.beta1),
		Beta2:                      float32(c.beta2),
		Epsilon:                    float32(c.epsilon),
		SchedulerStepSize:          uint32(c.scheduler_step_size),
		SchedulerGamma:             float32(c.scheduler_gamma),
		WarmupSteps:                uint32(c.warmup_steps),
		EnableGradientClipping:     bool(c.enable_gradient_clipping),
		GradientClipValue:          float32(c.gradient_clip_value),
		EnableBiologicalModulation: bool(c.enable_biological_modulation),
		BiologicalBlend:            float32(c.biological_blend),
		NetworkType:                NetworkType(c.network_type),
		SNNMethod:                  SNNTrainMethod(c.snn_method),
		SNNEligibilityTau:          float32(c.snn_eligibility_tau),
		SNNRewardTau:               float32(c.snn_reward_tau),
		SNNSurrogateBeta:           float32(c.snn_surrogate_beta),
		LNNMethod:                  LNNTrainMethod(c.lnn_method),
		LNNBPTTTruncation:          uint32(c.lnn_bptt_truncation),
		LNNUseAdjointCheckpointing: bool(c.lnn_use_adjoint_checkpointing),
	}
}

// TrainingResult holds results from a training step.
type TrainingResult struct {
	Loss         float32
	LearningRate float32
	Step         uint32
	EarlyStopped bool
	GradientNorm float32
}

// CallbackConfig holds callback configuration.
type CallbackConfig struct {
	EnableAutoCheckpoint bool
	CheckpointInterval   uint32
	EnableEarlyStopping  bool
	Patience             uint32
	MinDelta             float32
	DivergenceThreshold  float32
	LogInterval          uint32
}

func (cc *CallbackConfig) toC() C.nimcp_callback_config_t {
	var c C.nimcp_callback_config_t
	c.enable_auto_checkpoint = C.bool(cc.EnableAutoCheckpoint)
	c.checkpoint_interval = C.uint(cc.CheckpointInterval)
	c.enable_early_stopping = C.bool(cc.EnableEarlyStopping)
	c.patience = C.uint(cc.Patience)
	c.min_delta = C.float(cc.MinDelta)
	c.divergence_threshold = C.float(cc.DivergenceThreshold)
	c.log_interval = C.uint(cc.LogInterval)
	return c
}

// DefaultCallbackConfig returns sensible default callback configuration.
func DefaultCallbackConfig() CallbackConfig {
	c := C.nimcp_callback_config_default()
	return CallbackConfig{
		EnableAutoCheckpoint: bool(c.enable_auto_checkpoint),
		CheckpointInterval:   uint32(c.checkpoint_interval),
		EnableEarlyStopping:  bool(c.enable_early_stopping),
		Patience:             uint32(c.patience),
		MinDelta:             float32(c.min_delta),
		DivergenceThreshold:  float32(c.divergence_threshold),
		LogInterval:          uint32(c.log_interval),
	}
}

// CallbackMetrics holds training metrics passed to callbacks.
type CallbackMetrics struct {
	Step         uint64
	Epoch        uint64
	Loss         float32
	LossEMA      float32
	LearningRate float32
	GradientNorm float32
	StepTimeUS   uint64
	IsConverging bool
	IsDiverging  bool
}

// SnapshotInfo holds snapshot metadata.
type SnapshotInfo struct {
	Name         string
	Description  string
	Timestamp    uint64
	FileSize     uint32
	IsCompressed bool
	IsEncrypted  bool
}

// BrainProbe holds comprehensive brain state statistics.
type BrainProbe struct {
	TaskName          string
	Size              BrainSize
	Task              BrainTask
	NumNeurons        uint32
	NumSynapses       uint32
	NumActiveSynapses uint32
	TotalInferences   uint64
	TotalLearningSteps uint64
	AvgSparsity       float32
	AvgInferenceTimeUS float32
	CurrentLearningRate float32
	Accuracy          float32
	MemoryBytes       uint64
	NumInputs         uint32
	NumOutputs        uint32
	IsCOWClone        bool
	COWRefCount       uint32
	COWSharedBytes    uint64
	COWPrivateBytes   uint64
}

// Phasor represents an oscillation phasor (amplitude + phase).
type Phasor struct {
	Amplitude float32
	Phase     float32
}

// ============================================================================
// Callback Registry (CGo callback trampoline)
// ============================================================================

// CallbackFunc is the Go callback function signature.
type CallbackFunc func(event CallbackEvent, metrics *CallbackMetrics) CallbackAction

var (
	callbackMu   sync.RWMutex
	callbackMap  = make(map[uint64]CallbackFunc)
	callbackNext uint64 = 1
)

func registerGoCallback(fn CallbackFunc) uint64 {
	callbackMu.Lock()
	defer callbackMu.Unlock()
	key := callbackNext
	callbackNext++
	callbackMap[key] = fn
	return key
}

func unregisterGoCallback(key uint64) {
	callbackMu.Lock()
	defer callbackMu.Unlock()
	delete(callbackMap, key)
}

//export goCallbackTrampoline
func goCallbackTrampoline(event C.nimcp_callback_event_t, metrics *C.nimcp_callback_metrics_t, userData unsafe.Pointer) C.nimcp_callback_action_t {
	key := uint64(uintptr(userData))
	callbackMu.RLock()
	cb, ok := callbackMap[key]
	callbackMu.RUnlock()
	if !ok {
		return C.nimcp_callback_action_t(CallbackActionContinue)
	}
	var m CallbackMetrics
	if metrics != nil {
		m = CallbackMetrics{
			Step:         uint64(metrics.step),
			Epoch:        uint64(metrics.epoch),
			Loss:         float32(metrics.loss),
			LossEMA:      float32(metrics.loss_ema),
			LearningRate: float32(metrics.learning_rate),
			GradientNorm: float32(metrics.gradient_norm),
			StepTimeUS:   uint64(metrics.step_time_us),
			IsConverging: bool(metrics.is_converging),
			IsDiverging:  bool(metrics.is_diverging),
		}
	}
	action := cb(CallbackEvent(event), &m)
	return C.nimcp_callback_action_t(action)
}

// ============================================================================
// Library Lifecycle
// ============================================================================

// Init initializes the NIMCP library. Call once at startup.
func Init() error {
	return checkStatus(C.nimcp_init())
}

// Shutdown cleans up the NIMCP library. Call once at exit.
func Shutdown() {
	C.nimcp_shutdown()
}

// Version returns the NIMCP version string.
func Version() string {
	return C.GoString(C.nimcp_version())
}

// VersionInt returns the NIMCP version as integer (MAJOR*10000 + MINOR*100 + PATCH).
func VersionInt() int {
	return int(C.nimcp_version_int())
}

// GetError returns the last error message from the C library.
func GetError() string {
	return C.GoString(C.nimcp_get_error())
}

// ============================================================================
// Brain
// ============================================================================

// Brain represents a NIMCP brain instance.
type Brain struct {
	handle        C.nimcp_brain_t
	callbackKeys  []uint64 // track registered callback keys for cleanup
	swarmMaster   *C.nimcp_swarm_master_t
	swarmEdge     *C.nimcp_swarm_edge_runtime_t
	sensorHub     *C.nimcp_sensor_hub_t
	watchdog      *C.nimcp_safety_watchdog_t
	ros2Bridge    *C.nimcp_ros2_bridge_t
	mavlinkBridge *C.nimcp_mavlink_bridge_t
}

// NewBrain creates a new brain with preset configuration.
func NewBrain(name string, size BrainSize, task BrainTask, numInputs, numOutputs uint32) (*Brain, error) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	handle := C.nimcp_brain_create(cName, C.nimcp_brain_size_t(size), C.nimcp_brain_task_t(task),
		C.uint(numInputs), C.uint(numOutputs))
	if handle == nil {
		return nil, &NimcpError{Code: ErrGeneric, Message: GetError()}
	}
	return &Brain{handle: handle}, nil
}

// LoadBrain loads a brain from file.
func LoadBrain(filepath string) (*Brain, error) {
	cPath := C.CString(filepath)
	defer C.free(unsafe.Pointer(cPath))
	handle := C.nimcp_brain_load(cPath)
	if handle == nil {
		return nil, &NimcpError{Code: ErrIO, Message: GetError()}
	}
	return &Brain{handle: handle}, nil
}

// NewBrainFromConfig creates a brain from YAML/JSON config file.
func NewBrainFromConfig(configPath string) (*Brain, error) {
	cPath := C.CString(configPath)
	defer C.free(unsafe.Pointer(cPath))
	handle := C.nimcp_brain_create_from_config(cPath)
	if handle == nil {
		return nil, &NimcpError{Code: ErrGeneric, Message: GetError()}
	}
	return &Brain{handle: handle}, nil
}

// Close destroys the brain and frees resources.
func (b *Brain) Close() error {
	if b.handle != nil {
		C.nimcp_brain_destroy(b.handle)
		b.handle = nil
	}
	for _, key := range b.callbackKeys {
		unregisterGoCallback(key)
	}
	b.callbackKeys = nil
	return nil
}

// Learn teaches the brain from a single example.
func (b *Brain) Learn(features []float32, label string, confidence float32) error {
	cLabel := C.CString(label)
	defer C.free(unsafe.Pointer(cLabel))
	return checkStatus(C.nimcp_brain_learn_example(b.handle,
		(*C.float)(unsafe.Pointer(&features[0])), C.uint(len(features)),
		cLabel, C.float(confidence)))
}

// Predict makes a classification prediction.
func (b *Brain) Predict(features []float32) (label string, confidence float32, err error) {
	var cLabel [64]C.char
	var cConf C.float
	status := C.nimcp_brain_predict(b.handle,
		(*C.float)(unsafe.Pointer(&features[0])), C.uint(len(features)),
		&cLabel[0], &cConf)
	if err := checkStatus(status); err != nil {
		return "", 0, err
	}
	return C.GoString(&cLabel[0]), float32(cConf), nil
}

// Infer runs inference and returns raw output vector.
func (b *Brain) Infer(features []float32, numOutputs uint32) ([]float32, error) {
	outputs := make([]float32, numOutputs)
	status := C.nimcp_brain_infer(b.handle,
		(*C.float)(unsafe.Pointer(&features[0])), C.uint(len(features)),
		(*C.float)(unsafe.Pointer(&outputs[0])), C.uint(numOutputs))
	if err := checkStatus(status); err != nil {
		return nil, err
	}
	return outputs, nil
}

// Save saves the brain to file.
func (b *Brain) Save(filepath string) error {
	cPath := C.CString(filepath)
	defer C.free(unsafe.Pointer(cPath))
	return checkStatus(C.nimcp_brain_save(b.handle, cPath))
}

// --- Training Pipeline ---

// ConfigureTraining sets up the training pipeline.
func (b *Brain) ConfigureTraining(config *TrainingConfig) error {
	c := config.toC()
	return checkStatus(C.nimcp_brain_configure_training(b.handle, &c))
}

// TrainStep performs a single training step.
func (b *Brain) TrainStep(features, targets []float32) (*TrainingResult, error) {
	var cResult C.nimcp_training_result_t
	status := C.nimcp_brain_train_step(b.handle,
		(*C.float)(unsafe.Pointer(&features[0])), C.uint(len(features)),
		(*C.float)(unsafe.Pointer(&targets[0])), C.uint(len(targets)),
		&cResult)
	if err := checkStatus(status); err != nil {
		return nil, err
	}
	return &TrainingResult{
		Loss:         float32(cResult.loss),
		LearningRate: float32(cResult.learning_rate),
		Step:         uint32(cResult.step),
		EarlyStopped: bool(cResult.early_stopped),
		GradientNorm: float32(cResult.gradient_norm),
	}, nil
}

// TrainBatch trains on a batch of examples.
func (b *Brain) TrainBatch(features, targets []float32, batchSize, numFeatures, numTargets uint32) (*TrainingResult, error) {
	var cResult C.nimcp_training_result_t
	status := C.nimcp_brain_train_batch(b.handle,
		(*C.float)(unsafe.Pointer(&features[0])),
		(*C.float)(unsafe.Pointer(&targets[0])),
		C.uint(batchSize), C.uint(numFeatures), C.uint(numTargets),
		&cResult)
	if err := checkStatus(status); err != nil {
		return nil, err
	}
	return &TrainingResult{
		Loss:         float32(cResult.loss),
		LearningRate: float32(cResult.learning_rate),
		Step:         uint32(cResult.step),
		EarlyStopped: bool(cResult.early_stopped),
		GradientNorm: float32(cResult.gradient_norm),
	}, nil
}

// GetTrainingStats returns current training statistics.
func (b *Brain) GetTrainingStats() (totalSteps uint64, totalLoss float32, currentLR float32, err error) {
	var cSteps C.ulong
	var cLoss, cLR C.float
	status := C.nimcp_brain_get_training_stats(b.handle, &cSteps, &cLoss, &cLR)
	if err := checkStatus(status); err != nil {
		return 0, 0, 0, err
	}
	return uint64(cSteps), float32(cLoss), float32(cLR), nil
}

// StepScheduler updates the learning rate scheduler.
func (b *Brain) StepScheduler(validationMetric float32) float32 {
	return float32(C.nimcp_brain_step_scheduler(b.handle, C.float(validationMetric)))
}

// --- Callbacks ---

// EnableCallbacks enables the training callback system.
func (b *Brain) EnableCallbacks(config *CallbackConfig) error {
	c := config.toC()
	return checkStatus(C.nimcp_brain_enable_callbacks(b.handle, &c))
}

// DisableCallbacks disables the training callback system.
func (b *Brain) DisableCallbacks() error {
	return checkStatus(C.nimcp_brain_disable_callbacks(b.handle))
}

// RegisterCallback registers a Go callback for a training event.
// Returns the callback ID (>0) on success.
func (b *Brain) RegisterCallback(event CallbackEvent, fn CallbackFunc, name string) (uint32, error) {
	key := registerGoCallback(fn)

	var cName *C.char
	if name != "" {
		cName = C.CString(name)
		defer C.free(unsafe.Pointer(cName))
	}

	id := C.nimcp_brain_register_callback(b.handle,
		C.nimcp_callback_event_t(event),
		C.nimcp_training_callback_fn(C.goCallbackTrampoline),
		unsafe.Pointer(uintptr(key)),
		cName)
	if id == 0 {
		unregisterGoCallback(key)
		return 0, &NimcpError{Code: ErrGeneric, Message: "failed to register callback"}
	}
	b.callbackKeys = append(b.callbackKeys, key)
	return uint32(id), nil
}

// UnregisterCallback unregisters a callback by ID.
func (b *Brain) UnregisterCallback(callbackID uint32) error {
	return checkStatus(C.nimcp_brain_unregister_callback(b.handle, C.uint(callbackID)))
}

// GetCallbackStats returns callback statistics.
func (b *Brain) GetCallbackStats() (totalFired uint64, avgTimeUS float32, earlyStops uint32, err error) {
	var cFired C.ulong
	var cAvg C.float
	var cStops C.uint
	status := C.nimcp_brain_get_callback_stats(b.handle, &cFired, &cAvg, &cStops)
	if err := checkStatus(status); err != nil {
		return 0, 0, 0, err
	}
	return uint64(cFired), float32(cAvg), uint32(cStops), nil
}

// --- Resize ---

// Resize manually resizes brain to a specific neuron count.
func (b *Brain) Resize(newNeuronCount uint32) bool {
	return bool(C.nimcp_brain_resize(b.handle, C.uint(newNeuronCount)))
}

// AutoResize automatically resizes based on hardware and utilization.
func (b *Brain) AutoResize() bool {
	return bool(C.nimcp_brain_auto_resize(b.handle))
}

// GetNeuronCount returns current neuron count.
func (b *Brain) GetNeuronCount() uint32 {
	return uint32(C.nimcp_brain_get_neuron_count(b.handle))
}

// GetUtilizationMetrics returns brain utilization and saturation.
func (b *Brain) GetUtilizationMetrics() (utilization, saturation float32, ok bool) {
	var cUtil, cSat C.float
	result := C.nimcp_brain_get_utilization_metrics(b.handle, &cUtil, &cSat)
	return float32(cUtil), float32(cSat), bool(result)
}

// --- Per-network training toggles (runtime-dynamic, no rebuild required) ---
//
// These setters flip a config flag read by brain_learn_vector on every
// training step. Flips take effect immediately. Useful for ablation
// studies, SNN-only recovery, and round-robin training.

// SetTrainAnn enables or disables adaptive/ANN training.
func (b *Brain) SetTrainAnn(enabled bool) {
	C.nimcp_brain_set_train_ann(b.handle, C.bool(enabled))
}

// GetTrainAnn returns whether adaptive/ANN training is enabled.
func (b *Brain) GetTrainAnn() bool {
	return bool(C.nimcp_brain_get_train_ann(b.handle))
}

// SetTrainCnn enables or disables CNN (and cortex CNN) training.
func (b *Brain) SetTrainCnn(enabled bool) {
	C.nimcp_brain_set_train_cnn(b.handle, C.bool(enabled))
}

// GetTrainCnn returns whether CNN training is enabled.
func (b *Brain) GetTrainCnn() bool {
	return bool(C.nimcp_brain_get_train_cnn(b.handle))
}

// SetTrainSnn enables or disables SNN training.
func (b *Brain) SetTrainSnn(enabled bool) {
	C.nimcp_brain_set_train_snn(b.handle, C.bool(enabled))
}

// GetTrainSnn returns whether SNN training is enabled.
func (b *Brain) GetTrainSnn() bool {
	return bool(C.nimcp_brain_get_train_snn(b.handle))
}

// SetTrainLnn enables or disables LNN training.
func (b *Brain) SetTrainLnn(enabled bool) {
	C.nimcp_brain_set_train_lnn(b.handle, C.bool(enabled))
}

// GetTrainLnn returns whether LNN training is enabled.
func (b *Brain) GetTrainLnn() bool {
	return bool(C.nimcp_brain_get_train_lnn(b.handle))
}

// SetSnnOnlyRecovery enables the SNN-only recovery preset — freezes
// ANN/CNN/LNN while keeping SNN training active. Used to let the SNN
// re-converge against a stable ensemble after large BPTT behavior changes.
func (b *Brain) SetSnnOnlyRecovery(enabled bool) {
	C.nimcp_brain_set_snn_only_recovery(b.handle, C.bool(enabled))
}

// GetSnnOnlyRecovery returns whether SNN-only recovery mode is active.
func (b *Brain) GetSnnOnlyRecovery() bool {
	return bool(C.nimcp_brain_get_snn_only_recovery(b.handle))
}

// SetEnsembleWarmupScale sets a probabilistic gate on non-SNN training
// updates in [0.0, 1.0]. 1.0 = full-rate (default), 0.0 = fully frozen,
// intermediate values make each non-SNN training step run with that
// probability (Monte-Carlo). Used to ramp ANN/CNN/LNN back in gradually
// after SNN-only recovery. Out-of-range values are clamped on the C side.
func (b *Brain) SetEnsembleWarmupScale(scale float32) {
	C.nimcp_brain_set_ensemble_warmup_scale(b.handle, C.float(scale))
}

// GetEnsembleWarmupScale returns the current ensemble warmup scale.
func (b *Brain) GetEnsembleWarmupScale() float32 {
	return float32(C.nimcp_brain_get_ensemble_warmup_scale(b.handle))
}

// --- Named Snapshots ---

// SnapshotSave saves a named snapshot of the brain state.
func (b *Brain) SnapshotSave(name, description string) error {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	var cDesc *C.char
	if description != "" {
		cDesc = C.CString(description)
		defer C.free(unsafe.Pointer(cDesc))
	}
	return checkStatus(C.nimcp_brain_snapshot_save(b.handle, cName, cDesc))
}

// SnapshotRestore restores brain from a named snapshot.
func (b *Brain) SnapshotRestore(name string) (*Brain, error) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	handle := C.nimcp_brain_snapshot_restore(b.handle, cName)
	if handle == nil {
		return nil, &NimcpError{Code: ErrGeneric, Message: GetError()}
	}
	return &Brain{handle: handle}, nil
}

// SnapshotList lists all available snapshots.
func (b *Brain) SnapshotList(maxCount uint32) ([]SnapshotInfo, error) {
	infos := make([]C.nimcp_brain_snapshot_info_t, maxCount)
	var outCount C.uint
	status := C.nimcp_brain_snapshot_list(b.handle, &infos[0], C.uint(maxCount), &outCount)
	if err := checkStatus(status); err != nil {
		return nil, err
	}
	result := make([]SnapshotInfo, int(outCount))
	for i := 0; i < int(outCount); i++ {
		result[i] = SnapshotInfo{
			Name:         C.GoString(&infos[i].name[0]),
			Description:  C.GoString(&infos[i].description[0]),
			Timestamp:    uint64(infos[i].timestamp),
			FileSize:     uint32(infos[i].file_size),
			IsCompressed: bool(infos[i].is_compressed),
			IsEncrypted:  bool(infos[i].is_encrypted),
		}
	}
	return result, nil
}

// SnapshotDelete deletes a named snapshot.
func (b *Brain) SnapshotDelete(name string) error {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	return checkStatus(C.nimcp_brain_snapshot_delete(b.handle, cName))
}

// --- Copy-on-Write ---

// BrainSnapshot represents a COW snapshot handle.
type BrainSnapshot struct {
	handle C.nimcp_brain_snapshot_t
}

// CloneCOW creates a lightweight COW clone of the brain.
func (b *Brain) CloneCOW() (*Brain, error) {
	handle := C.nimcp_brain_clone_cow(b.handle)
	if handle == nil {
		return nil, &NimcpError{Code: ErrGeneric, Message: GetError()}
	}
	return &Brain{handle: handle}, nil
}

// SnapshotCOW creates a zero-copy snapshot using COW.
func (b *Brain) SnapshotCOW() (*BrainSnapshot, error) {
	handle := C.nimcp_brain_snapshot_cow(b.handle)
	if handle == nil {
		return nil, &NimcpError{Code: ErrGeneric, Message: GetError()}
	}
	return &BrainSnapshot{handle: handle}, nil
}

// RestoreCOW restores brain state from a COW snapshot.
func (b *Brain) RestoreCOW(snapshot *BrainSnapshot) error {
	return checkStatus(C.nimcp_brain_restore_cow(b.handle, snapshot.handle))
}

// Close destroys the snapshot and releases references.
func (s *BrainSnapshot) Close() error {
	if s.handle != nil {
		C.nimcp_brain_snapshot_destroy(s.handle)
		s.handle = nil
	}
	return nil
}

// --- Working Memory ---

// WorkingMemoryAdd adds an item to working memory.
func (b *Brain) WorkingMemoryAdd(data []float32, salience float32) error {
	return checkStatus(C.nimcp_brain_working_memory_add(b.handle,
		(*C.float)(unsafe.Pointer(&data[0])), C.uint(len(data)),
		C.float(salience)))
}

// WorkingMemoryGet retrieves an item from working memory by index.
func (b *Brain) WorkingMemoryGet(index uint32) ([]float32, error) {
	var size C.uint
	ptr := C.nimcp_brain_working_memory_get(b.handle, C.uint(index), &size)
	if ptr == nil {
		return nil, &NimcpError{Code: ErrGeneric, Message: "working memory item not found"}
	}
	n := int(size)
	result := make([]float32, n)
	cSlice := unsafe.Slice((*float32)(unsafe.Pointer(ptr)), n)
	copy(result, cSlice)
	return result, nil
}

// WorkingMemoryStats returns current size and capacity.
func (b *Brain) WorkingMemoryStats() (currentSize, capacity uint32, err error) {
	var cSize, cCap C.uint
	status := C.nimcp_brain_working_memory_stats(b.handle, &cSize, &cCap)
	if err := checkStatus(status); err != nil {
		return 0, 0, err
	}
	return uint32(cSize), uint32(cCap), nil
}

// WorkingMemoryRefresh refreshes an item to prevent decay.
func (b *Brain) WorkingMemoryRefresh(index uint32) error {
	return checkStatus(C.nimcp_brain_working_memory_refresh(b.handle, C.uint(index)))
}

// --- Global Workspace ---

// WorkspaceCompete submits content for global workspace competition.
func (b *Brain) WorkspaceCompete(module CognitiveModule, content []float32, strength float32) error {
	return checkStatus(C.nimcp_brain_workspace_compete(b.handle,
		C.nimcp_cognitive_module_t(module),
		(*C.float)(unsafe.Pointer(&content[0])), C.uint(len(content)),
		C.float(strength)))
}

// WorkspaceRead reads current global workspace broadcast.
func (b *Brain) WorkspaceRead(maxDim uint32) (content []float32, source CognitiveModule, err error) {
	buf := make([]float32, maxDim)
	var actualDim C.uint
	var cSource C.nimcp_cognitive_module_t
	status := C.nimcp_brain_workspace_read(b.handle,
		(*C.float)(unsafe.Pointer(&buf[0])), C.uint(maxDim),
		&actualDim, &cSource)
	if err := checkStatus(status); err != nil {
		return nil, ModuleNone, err
	}
	return buf[:int(actualDim)], CognitiveModule(cSource), nil
}

// WorkspaceSubscribe subscribes a module to workspace broadcasts.
func (b *Brain) WorkspaceSubscribe(module CognitiveModule) error {
	return checkStatus(C.nimcp_brain_workspace_subscribe(b.handle,
		C.nimcp_cognitive_module_t(module)))
}

// WorkspaceUnsubscribe unsubscribes a module from workspace broadcasts.
func (b *Brain) WorkspaceUnsubscribe(module CognitiveModule) error {
	return checkStatus(C.nimcp_brain_workspace_unsubscribe(b.handle,
		C.nimcp_cognitive_module_t(module)))
}

// WorkspaceHasBroadcast checks if workspace has an active broadcast.
func (b *Brain) WorkspaceHasBroadcast() (bool, error) {
	var has C.bool
	status := C.nimcp_brain_workspace_has_broadcast(b.handle, &has)
	if err := checkStatus(status); err != nil {
		return false, err
	}
	return bool(has), nil
}

// WorkspaceStats returns workspace statistics.
func (b *Brain) WorkspaceStats() (totalBroadcasts, totalCompetitions uint32, avgStrength float32, err error) {
	var cBcast, cComp C.uint
	var cAvg C.float
	status := C.nimcp_brain_workspace_stats(b.handle, &cBcast, &cComp, &cAvg)
	if err := checkStatus(status); err != nil {
		return 0, 0, 0, err
	}
	return uint32(cBcast), uint32(cComp), float32(cAvg), nil
}

// --- Oscillations ---

// EnableOscillations enables or disables complex oscillation features.
func (b *Brain) EnableOscillations(enable bool) bool {
	return bool(C.nimcp_enable_complex_oscillations(b.handle, C.bool(enable)))
}

// IsOscillationsEnabled checks if oscillations are enabled.
func (b *Brain) IsOscillationsEnabled() bool {
	return bool(C.nimcp_is_complex_oscillations_enabled(b.handle))
}

// GetPhasor returns the oscillation phasor for a neuron.
func (b *Brain) GetPhasor(neuronID uint32) Phasor {
	p := C.nimcp_get_oscillation_phasor(b.handle, C.uint(neuronID))
	return Phasor{Amplitude: float32(p.amplitude), Phase: float32(p.phase)}
}

// GetPhaseCoherence computes phase coherence across neurons.
func (b *Brain) GetPhaseCoherence(neuronIDs []uint32) float32 {
	return float32(C.nimcp_get_phase_coherence(b.handle,
		(*C.uint)(unsafe.Pointer(&neuronIDs[0])), C.uint(len(neuronIDs))))
}

// GetPACModulation computes phase-amplitude coupling modulation index.
func (b *Brain) GetPACModulation(thetaFreq, gammaFreq float32) float32 {
	return float32(C.nimcp_get_pac_modulation(b.handle, C.float(thetaFreq), C.float(gammaFreq)))
}

// --- Probe ---

// Probe returns comprehensive brain state statistics.
func (b *Brain) Probe() (*BrainProbe, error) {
	var cp C.nimcp_brain_probe_t
	status := C.nimcp_brain_probe(b.handle, &cp)
	if err := checkStatus(status); err != nil {
		return nil, err
	}
	return &BrainProbe{
		TaskName:            C.GoString(&cp.task_name[0]),
		Size:                BrainSize(cp.size),
		Task:                BrainTask(cp.task),
		NumNeurons:          uint32(cp.num_neurons),
		NumSynapses:         uint32(cp.num_synapses),
		NumActiveSynapses:   uint32(cp.num_active_synapses),
		TotalInferences:     uint64(cp.total_inferences),
		TotalLearningSteps:  uint64(cp.total_learning_steps),
		AvgSparsity:         float32(cp.avg_sparsity),
		AvgInferenceTimeUS:  float32(cp.avg_inference_time_us),
		CurrentLearningRate: float32(cp.current_learning_rate),
		Accuracy:            float32(cp.accuracy),
		MemoryBytes:         uint64(cp.memory_bytes),
		NumInputs:           uint32(cp.num_inputs),
		NumOutputs:          uint32(cp.num_outputs),
		IsCOWClone:          bool(cp.is_cow_clone),
		COWRefCount:         uint32(cp.cow_ref_count),
		COWSharedBytes:      uint64(cp.cow_shared_bytes),
		COWPrivateBytes:     uint64(cp.cow_private_bytes),
	}, nil
}

// BroadcastProbe broadcasts brain probe data via bio-async.
func (b *Brain) BroadcastProbe() error {
	return checkStatus(C.nimcp_brain_broadcast_probe(b.handle))
}

// ============================================================================
// Network
// ============================================================================

// Network represents a low-level neural network.
type Network struct {
	handle     C.nimcp_network_t
	numOutputs int
}

// NewNetwork creates a new neural network.
func NewNetwork(numInputs, numOutputs, numHidden uint32, learningRate float32) (*Network, error) {
	handle := C.nimcp_network_create(C.uint(numInputs), C.uint(numOutputs),
		C.uint(numHidden), C.float(learningRate))
	if handle == nil {
		return nil, &NimcpError{Code: ErrGeneric, Message: GetError()}
	}
	return &Network{handle: handle, numOutputs: int(numOutputs)}, nil
}

// Close destroys the network.
func (n *Network) Close() error {
	if n.handle != nil {
		C.nimcp_network_destroy(n.handle)
		n.handle = nil
	}
	return nil
}

// Forward performs a forward pass.
func (n *Network) Forward(inputs []float32) ([]float32, error) {
	outputs := make([]float32, n.numOutputs)
	status := C.nimcp_network_forward(n.handle,
		(*C.float)(unsafe.Pointer(&inputs[0])), C.uint(len(inputs)),
		(*C.float)(unsafe.Pointer(&outputs[0])), C.uint(n.numOutputs))
	if err := checkStatus(status); err != nil {
		return nil, err
	}
	return outputs, nil
}

// Train trains on a single example.
func (n *Network) Train(inputs, targets []float32) error {
	return checkStatus(C.nimcp_network_train(n.handle,
		(*C.float)(unsafe.Pointer(&inputs[0])), C.uint(len(inputs)),
		(*C.float)(unsafe.Pointer(&targets[0])), C.uint(len(targets))))
}

// ============================================================================
// Ethics
// ============================================================================

// Ethics represents an ethics evaluation module.
type Ethics struct {
	handle C.nimcp_ethics_t
}

// NewEthics creates a new ethics module.
func NewEthics() (*Ethics, error) {
	handle := C.nimcp_ethics_create()
	if handle == nil {
		return nil, &NimcpError{Code: ErrGeneric, Message: GetError()}
	}
	return &Ethics{handle: handle}, nil
}

// Close destroys the ethics module.
func (e *Ethics) Close() error {
	if e.handle != nil {
		C.nimcp_ethics_destroy(e.handle)
		e.handle = nil
	}
	return nil
}

// Check evaluates the ethical score of a situation.
func (e *Ethics) Check(situation []float32) (float32, error) {
	var score C.float
	status := C.nimcp_ethics_check(e.handle,
		(*C.float)(unsafe.Pointer(&situation[0])), C.uint(len(situation)),
		&score)
	if err := checkStatus(status); err != nil {
		return 0, err
	}
	return float32(score), nil
}

// ============================================================================
// KnowledgeGraph
// ============================================================================

// KnowledgeGraph represents a knowledge graph.
type KnowledgeGraph struct {
	handle C.nimcp_knowledge_t
}

// NewKnowledgeGraph creates a new knowledge graph.
func NewKnowledgeGraph() (*KnowledgeGraph, error) {
	handle := C.nimcp_knowledge_create()
	if handle == nil {
		return nil, &NimcpError{Code: ErrGeneric, Message: GetError()}
	}
	return &KnowledgeGraph{handle: handle}, nil
}

// Close destroys the knowledge graph.
func (kg *KnowledgeGraph) Close() error {
	if kg.handle != nil {
		C.nimcp_knowledge_destroy(kg.handle)
		kg.handle = nil
	}
	return nil
}

// AddFact adds a subject-predicate-object triple.
func (kg *KnowledgeGraph) AddFact(subject, predicate, object string) error {
	cSubj := C.CString(subject)
	defer C.free(unsafe.Pointer(cSubj))
	cPred := C.CString(predicate)
	defer C.free(unsafe.Pointer(cPred))
	cObj := C.CString(object)
	defer C.free(unsafe.Pointer(cObj))
	return checkStatus(C.nimcp_knowledge_add_fact(kg.handle, cSubj, cPred, cObj))
}

// Query queries the knowledge graph.
func (kg *KnowledgeGraph) Query(query string) (string, error) {
	cQuery := C.CString(query)
	defer C.free(unsafe.Pointer(cQuery))
	var buf [1024]C.char
	status := C.nimcp_knowledge_query(kg.handle, cQuery, &buf[0], 1024)
	if err := checkStatus(status); err != nil {
		return "", err
	}
	return C.GoString(&buf[0]), nil
}

// ============================================================================
// Group 1 — Sensory/Multimodal
// ============================================================================

// SensoryOpts holds optional parameters for SubmitSensory.
type SensoryOpts struct {
	Width     uint32
	Height    uint32
	Channels  uint32
	NSegments uint32
}

// SubmitSensory stages sensory data for cross-modal cortex CNN processing.
// Supported modalities: "visual", "audio", "speech", "somatosensory".
// Optional opts (at most one) provides width/height/channels for visual,
// n_segments for somatosensory.
func (b *Brain) SubmitSensory(modality string, data []float32, opts ...SensoryOpts) error {
	if len(data) == 0 {
		return &NimcpError{Code: ErrInvalid, Message: "empty data slice"}
	}
	cMod := C.CString(modality)
	defer C.free(unsafe.Pointer(cMod))
	var o SensoryOpts
	if len(opts) > 0 {
		o = opts[0]
	}
	status := C.go_brain_submit_sensory(b.handle, cMod,
		(*C.float)(unsafe.Pointer(&data[0])), C.uint(len(data)),
		C.uint(o.Width), C.uint(o.Height),
		C.uint(o.Channels), C.uint(o.NSegments))
	return checkStatus(status)
}

// VisualCortexProcess runs an image through the brain's visual cortex CNN
// and returns the extracted feature vector.
func (b *Brain) VisualCortexProcess(pixels []float32, width, height, channels uint32) ([]float32, error) {
	if len(pixels) == 0 {
		return nil, &NimcpError{Code: ErrInvalid, Message: "empty pixel data"}
	}
	const maxFeat = 1024
	features := make([]float32, maxFeat)
	n := C.go_brain_visual_cortex_process(b.handle,
		(*C.float)(unsafe.Pointer(&pixels[0])), C.uint(len(pixels)),
		C.uint(width), C.uint(height), C.uint(channels),
		(*C.float)(unsafe.Pointer(&features[0])), C.uint(maxFeat))
	if n == 0 {
		return nil, &NimcpError{Code: ErrGeneric, Message: "visual cortex processing failed or unavailable"}
	}
	return features[:int(n)], nil
}

// ============================================================================
// Group 2 — Avatar/Metrics
// ============================================================================

// GetAvatarState returns the full avatar face/emotion/voice state.
func (b *Brain) GetAvatarState() (map[string]interface{}, error) {
	var state C.nimcp_avatar_state_t
	status := C.nimcp_brain_get_avatar_state(b.handle, &state)
	if err := checkStatus(status); err != nil {
		return nil, err
	}
	return map[string]interface{}{
		// Viseme / mouth
		"mouth_open":       float32(state.mouth_open),
		"lip_round":        float32(state.lip_round),
		"lip_upper":        float32(state.lip_upper),
		"lip_lower":        float32(state.lip_lower),
		"tongue_position":  float32(state.tongue_position),
		"current_viseme":   uint8(state.current_viseme),
		// FACS AUs
		"au1_inner_brow_raise": float32(state.au1_inner_brow_raise),
		"au2_outer_brow_raise": float32(state.au2_outer_brow_raise),
		"au4_brow_lower":       float32(state.au4_brow_lower),
		"au5_upper_lid_raise":  float32(state.au5_upper_lid_raise),
		"au6_cheek_raise":      float32(state.au6_cheek_raise),
		"au7_lid_tighten":      float32(state.au7_lid_tighten),
		"au9_nose_wrinkle":     float32(state.au9_nose_wrinkle),
		"au10_upper_lip_raise": float32(state.au10_upper_lip_raise),
		"au12_lip_corner_pull": float32(state.au12_lip_corner_pull),
		"au15_lip_corner_drop": float32(state.au15_lip_corner_drop),
		"au17_chin_raise":      float32(state.au17_chin_raise),
		"au20_lip_stretch":     float32(state.au20_lip_stretch),
		"au23_lip_tighten":     float32(state.au23_lip_tighten),
		"au25_lips_part":       float32(state.au25_lips_part),
		"au26_jaw_drop":        float32(state.au26_jaw_drop),
		"au28_lip_suck":        float32(state.au28_lip_suck),
		// Emotion
		"valence":           float32(state.valence),
		"arousal":           float32(state.arousal),
		"dominance":         float32(state.dominance),
		"emotion_id":        uint32(state.emotion_id),
		"emotion_intensity": float32(state.emotion_intensity),
		// Gaze + head
		"gaze_x":     float32(state.gaze_x),
		"gaze_y":     float32(state.gaze_y),
		"head_pitch":  float32(state.head_pitch),
		"head_yaw":    float32(state.head_yaw),
		"head_roll":   float32(state.head_roll),
		"blink":       float32(state.blink),
		// Voice
		"pitch_hz":      float32(state.pitch_hz),
		"speaking_rate": float32(state.speaking_rate),
		"volume":        float32(state.volume),
		// Metadata
		"timestamp_us": uint64(state.timestamp_us),
		"is_speaking":  bool(state.is_speaking),
	}, nil
}

// GetNetworkMetrics returns per-network training loss EMA and step counts.
func (b *Brain) GetNetworkMetrics() (map[string]interface{}, error) {
	var emaANN, emaCNN, emaSNN, emaLNN C.float
	var annSteps, cnnSteps, snnSteps, lnnSteps C.ulong
	ok := C.nimcp_brain_get_network_metrics(b.handle,
		&emaANN, &emaCNN, &emaSNN, &emaLNN,
		&annSteps, &cnnSteps, &snnSteps, &lnnSteps)
	if !bool(ok) {
		return nil, &NimcpError{Code: ErrGeneric, Message: "get_network_metrics failed"}
	}
	return map[string]interface{}{
		"ann_loss":  float32(emaANN),
		"cnn_loss":  float32(emaCNN),
		"snn_loss":  float32(emaSNN),
		"lnn_loss":  float32(emaLNN),
		"ann_steps": uint64(annSteps),
		"cnn_steps": uint64(cnnSteps),
		"snn_steps": uint64(snnSteps),
		"lnn_steps": uint64(lnnSteps),
	}, nil
}

// GetCortexCNNMetrics returns per-cortex CNN processor metrics.
// Keys: "visual", "audio", "speech", "somato".
func (b *Brain) GetCortexCNNMetrics() (map[string]interface{}, error) {
	typeKeys := [4]string{"visual", "audio", "speech", "somato"}
	result := make(map[string]interface{})
	for ci := 0; ci < 4; ci++ {
		var m C.go_cortex_cnn_metrics_t
		if bool(C.go_brain_get_cortex_cnn_metrics(b.handle, C.int(ci), &m)) {
			result[typeKeys[ci]] = map[string]interface{}{
				"last_loss":      float32(m.last_loss),
				"ema_loss":       float32(m.ema_loss),
				"forward_steps":  uint64(m.forward_steps),
				"backward_steps": uint64(m.backward_steps),
				"embedding_norm": float32(m.embedding_norm),
				"confidence":     float32(m.confidence),
				"embedding_dim":  uint32(m.embedding_dim),
				"num_params":     uint32(m.num_params),
			}
		}
	}
	return result, nil
}

// ============================================================================
// Group 3 — Core Inference
// ============================================================================

// DecideFull runs the full cognitive decision pipeline and returns a rich result.
func (b *Brain) DecideFull(features []float32) (map[string]interface{}, error) {
	if len(features) == 0 {
		return nil, &NimcpError{Code: ErrInvalid, Message: "empty features"}
	}
	var label [64]C.char
	var confidence C.float
	var explanation [256]C.char
	const maxOutput = 4096
	var outputVec [maxOutput]C.float
	outputSize := C.uint(maxOutput)
	var numActive C.uint
	var sparsity C.float
	var inferenceTimeUS C.ulong

	status := C.nimcp_brain_decide_full(b.handle,
		(*C.float)(unsafe.Pointer(&features[0])), C.uint(len(features)),
		&label[0], &confidence, &explanation[0],
		&outputVec[0], &outputSize,
		&numActive, &sparsity, &inferenceTimeUS)
	if err := checkStatus(status); err != nil {
		return nil, err
	}

	vecLen := int(outputSize)
	if vecLen > maxOutput {
		vecLen = maxOutput
	}
	vec := make([]float32, vecLen)
	for i := 0; i < vecLen; i++ {
		vec[i] = float32(outputVec[i])
	}

	return map[string]interface{}{
		"label":              C.GoString(&label[0]),
		"confidence":         float32(confidence),
		"explanation":        C.GoString(&explanation[0]),
		"output_vector":      vec,
		"num_active_neurons": uint32(numActive),
		"sparsity":           float32(sparsity),
		"inference_time_us":  uint64(inferenceTimeUS),
	}, nil
}

// GetTranscript returns the cognitive transcript from the last DecideFull call.
func (b *Brain) GetTranscript() ([]interface{}, error) {
	const maxEntries = 32
	var summaries [maxEntries][256]C.char
	var saliences [maxEntries]C.float
	var confidences [maxEntries]C.float
	var modules [maxEntries]*C.char

	count := C.nimcp_brain_get_last_transcript(b.handle,
		&summaries[0], &saliences[0], &confidences[0], &modules[0],
		C.uint(maxEntries))

	n := int(count)
	result := make([]interface{}, n)
	for i := 0; i < n; i++ {
		modName := "unknown"
		if modules[i] != nil {
			modName = C.GoString(modules[i])
		}
		result[i] = map[string]interface{}{
			"module":     modName,
			"summary":    C.GoString(&summaries[i][0]),
			"salience":   float32(saliences[i]),
			"confidence": float32(confidences[i]),
		}
	}
	return result, nil
}

// GetCognitiveStats returns per-module cognitive training statistics.
func (b *Brain) GetCognitiveStats() (map[string]interface{}, error) {
	var steps [13]C.uint
	var losses [13]C.float
	var count C.uint

	status := C.nimcp_brain_get_cognitive_stats(b.handle, &steps[0], &losses[0], &count)
	if err := checkStatus(status); err != nil {
		return nil, err
	}

	moduleNames := []string{
		"grounded_language", "knowledge", "vae", "fep_parietal",
		"physics_nn", "pred_hierarchy", "jepa", "creative",
		"self_heal", "intuition", "fep_orchestrator",
	}

	result := make(map[string]interface{})
	n := int(count)
	if n > 11 {
		n = 11
	}
	for i := 0; i < n; i++ {
		result[moduleNames[i]] = map[string]interface{}{
			"steps":     uint32(steps[i]),
			"last_loss": float32(losses[i]),
		}
	}
	return result, nil
}

// GetAccuracy returns the running label-match accuracy (EMA).
func (b *Brain) GetAccuracy() float32 {
	return float32(C.nimcp_brain_get_accuracy(b.handle))
}

// ============================================================================
// Group 4 — LNN/SNN/CNN
// ============================================================================

// LNNCreate creates an NCP-architecture LNN temporal processor on the brain.
// Default values: nSensory=128, nInter=64, nCommand=32, nOutput=64.
func (b *Brain) LNNCreate(nSensory, nInter, nCommand, nOutput uint32) error {
	ok := C.go_brain_lnn_create(b.handle,
		C.uint(nSensory), C.uint(nInter),
		C.uint(nCommand), C.uint(nOutput))
	if !bool(ok) {
		return &NimcpError{Code: ErrGeneric, Message: "failed to create LNN network"}
	}
	return nil
}

// LNNGetStats returns LNN network statistics.
func (b *Brain) LNNGetStats() (map[string]interface{}, error) {
	var stats C.go_lnn_stats_t
	if !bool(C.go_brain_lnn_get_stats(b.handle, &stats)) {
		return nil, &NimcpError{Code: ErrGeneric, Message: "LNN stats unavailable (network not created?)"}
	}
	return map[string]interface{}{
		"forward_steps":  uint64(stats.forward_steps),
		"backward_steps": uint64(stats.backward_steps),
		"total_ode_evals": uint64(stats.ode_evaluations),
		"avg_tau":         float32(stats.avg_tau),
		"state_norm":      float32(stats.state_norm),
		"gradient_norm":   float32(stats.gradient_norm),
		"nan_count":       uint32(stats.nan_count),
		"inf_count":       uint32(stats.inf_count),
	}, nil
}

// SNNGetStats returns SNN network statistics.
func (b *Brain) SNNGetStats() (map[string]interface{}, error) {
	var stats C.go_snn_stats_t
	if !bool(C.go_brain_snn_get_stats(b.handle, &stats)) {
		return nil, &NimcpError{Code: ErrGeneric, Message: "SNN stats unavailable (network not created?)"}
	}
	return map[string]interface{}{
		"total_steps":        uint64(stats.total_steps),
		"total_spikes":       uint64(stats.total_spikes),
		"mean_firing_rate":   float32(stats.mean_firing_rate),
		"max_firing_rate":    float32(stats.max_firing_rate),
		"sparsity":           float32(stats.sparsity),
		"synchrony":          float32(stats.synchrony),
		"spikes_per_sample":  float32(stats.spikes_per_sample),
		"silent_neurons":     uint32(stats.silent_neurons),
		"hyperactive_neurons": uint32(stats.hyperactive_neurons),
		"health":             int(stats.health),
		"memory_usage_bytes": uint64(stats.memory_usage_bytes),
	}, nil
}

// SNNSetInputScale sets the SNN input amplification factor.
func SNNSetInputScale(scale float32) {
	C.nimcp_snn_set_input_scale(C.float(scale))
}

// SNNGetInputScale returns the current SNN input scale factor.
func SNNGetInputScale() float32 {
	return float32(C.nimcp_snn_get_input_scale())
}

// CNNGetStats returns CNN trainer statistics.
func (b *Brain) CNNGetStats() (map[string]interface{}, error) {
	var stats C.go_cnn_stats_t
	if !bool(C.go_brain_cnn_get_stats(b.handle, &stats)) {
		return nil, &NimcpError{Code: ErrGeneric, Message: "CNN stats unavailable (trainer not created?)"}
	}
	return map[string]interface{}{
		"num_layers":     uint32(stats.num_layers),
		"num_parameters": uint64(stats.num_parameters),
		"num_labels":     uint32(stats.num_labels),
		"active":         bool(stats.active),
	}, nil
}

// ============================================================================
// Group 5 — Configuration
// ============================================================================

// SetFastTraining enables or disables fast training mode.
func (b *Brain) SetFastTraining(enabled bool) error {
	if !bool(C.go_brain_set_fast_training(b.handle, C.bool(enabled))) {
		return &NimcpError{Code: ErrGeneric, Message: "set_fast_training failed (brain not initialized?)"}
	}
	return nil
}

// SetTaskType sets the brain's task strategy.
// Valid values: "regression", "classification", "pattern", "association".
func (b *Brain) SetTaskType(taskType string) error {
	cTask := C.CString(taskType)
	defer C.free(unsafe.Pointer(cTask))
	if !bool(C.go_brain_set_task_type(b.handle, cTask)) {
		return &NimcpError{Code: ErrInvalid, Message: fmt.Sprintf("unknown task type: %q (use regression/classification/pattern/association)", taskType)}
	}
	return nil
}

// EnableBiologicalPlasticity wires or unwires the biological plasticity bridge
// (TPB + EDP + coordinator) into the learn path.
func (b *Brain) EnableBiologicalPlasticity(enabled bool) error {
	if !bool(C.go_brain_enable_biological_plasticity(b.handle, C.bool(enabled))) {
		return &NimcpError{Code: ErrGeneric, Message: "enable_biological_plasticity failed (brain not initialized?)"}
	}
	return nil
}

// EnableMultiNetwork enables ensemble training across all network architectures
// (ANN, SNN, LNN, CNN).
func (b *Brain) EnableMultiNetwork() error {
	rc := C.go_brain_enable_multi_network(b.handle)
	if rc < 0 {
		return &NimcpError{Code: ErrGeneric, Message: "failed to enable multi-network training"}
	}
	return nil
}

// ============================================================================
// Group 6 — Brain State
// ============================================================================

// MedullaGetArousal returns the medulla arousal level [0.0, 1.0].
func (b *Brain) MedullaGetArousal() float32 {
	return float32(C.go_brain_medulla_get_arousal(b.handle))
}

// SleepGetPressure returns the current sleep pressure (adenosine) [0.0, 1.0].
func (b *Brain) SleepGetPressure() float32 {
	return float32(C.go_brain_sleep_get_pressure(b.handle))
}

// BGGetDopamine returns the basal ganglia dopamine level [0.0, 1.0].
func (b *Brain) BGGetDopamine() float32 {
	return float32(C.go_brain_bg_get_dopamine(b.handle))
}

// SubstrateGetHealth returns the computational substrate health status.
// Returns one of: "OPTIMAL", "STRESSED", "COMPROMISED", "CRITICAL", "UNKNOWN".
func (b *Brain) SubstrateGetHealth() string {
	return C.GoString(C.go_brain_substrate_get_health(b.handle))
}

// FocusAttention hints the attention system to prioritize a modality.
// Modality: "visual", "audio", "speech", "somatosensory".
func (b *Brain) FocusAttention(modality string) error {
	cMod := C.CString(modality)
	defer C.free(unsafe.Pointer(cMod))
	if !bool(C.go_brain_focus_attention(b.handle, cMod)) {
		return &NimcpError{Code: ErrGeneric, Message: "focus_attention failed"}
	}
	return nil
}

// ============================================================================
// Group 7 — Edge Brain API
// ============================================================================

// EdgeResizeResult contains the result of an edge resize operation.
type EdgeResizeResult struct {
	Status        int
	TargetNeurons uint32
	Mode          string
}

// EdgeResize resizes the brain's neural network at runtime.
// Mode: "contract" (default), "expand", "rebalance".
func (b *Brain) EdgeResize(targetNeurons uint32, mode string, knowledgeTransfer bool) (*EdgeResizeResult, error) {
	config := C.nimcp_resize_config_default()
	config.target_neuron_count = C.uint32_t(targetNeurons)
	config.enable_knowledge_transfer = C.bool(knowledgeTransfer)

	switch mode {
	case "expand":
		config.mode = C.NIMCP_RESIZE_EXPAND
	case "rebalance":
		config.mode = C.NIMCP_RESIZE_REBALANCE
	default:
		config.mode = C.NIMCP_RESIZE_CONTRACT
		mode = "contract"
	}

	ret := int(C.nimcp_edge_brain_resize(b.handle, &config))
	return &EdgeResizeResult{Status: ret, TargetNeurons: targetNeurons, Mode: mode}, nil
}

// EdgeResizeCheckResult contains the dry-run resize feasibility report.
type EdgeResizeCheckResult struct {
	Feasible      bool
	NeuronsBefore uint32
	NeuronsAfter  uint32
	RamDeltaMB    float32
	Reason        string
}

// EdgeResizeCheck performs a dry-run resize check without executing.
func (b *Brain) EdgeResizeCheck(targetNeurons uint32) *EdgeResizeCheckResult {
	config := C.nimcp_resize_config_default()
	config.target_neuron_count = C.uint32_t(targetNeurons)
	config.mode = C.NIMCP_RESIZE_CONTRACT
	var report C.nimcp_resize_report_t
	C.nimcp_edge_brain_resize_check(b.handle, &config, &report)
	return &EdgeResizeCheckResult{
		Feasible:      bool(report.feasible),
		NeuronsBefore: uint32(report.neurons_before),
		NeuronsAfter:  uint32(report.neurons_after),
		RamDeltaMB:    float32(report.estimated_ram_delta_mb),
		Reason:        C.GoString(&report.reason[0]),
	}
}

// EdgeDistillResult contains the result of knowledge distillation.
type EdgeDistillResult struct {
	Status            int
	AccuracyRetention float32
	NeuronsSelected   uint32
	CompressionRatio  float32
	TeacherLoss       float32
	StudentLoss       float32
	StepsTrained      uint32
}

// EdgeDistill distills the brain into a smaller student brain.
func (b *Brain) EdgeDistill(targetNeurons uint32, temperature float32, steps uint32,
	includeSNN, includeLNN, includeCNN bool) (*EdgeDistillResult, error) {
	config := C.nimcp_distill_config_default()
	config.target_neurons = C.uint32_t(targetNeurons)
	config.temperature = C.float(temperature)
	config.distillation_steps = C.uint32_t(steps)
	config.include_snn = C.bool(includeSNN)
	config.include_lnn = C.bool(includeLNN)
	config.include_cnn = C.bool(includeCNN)

	var report C.nimcp_distill_report_t
	var student C.nimcp_brain_t
	ret := int(C.nimcp_brain_distill(b.handle, &student, &config, &report))
	return &EdgeDistillResult{
		Status:            ret,
		AccuracyRetention: float32(report.accuracy_retention),
		NeuronsSelected:   uint32(report.neurons_selected),
		CompressionRatio:  float32(report.compression_ratio),
		TeacherLoss:       float32(report.teacher_loss),
		StudentLoss:       float32(report.student_loss),
		StepsTrained:      uint32(report.steps_trained),
	}, nil
}

// EdgeOptimizeResult contains the result of device optimization.
type EdgeOptimizeResult struct {
	Status              int
	NeuronCount         uint32
	SubsystemsEnabled   uint32
	EstimatedRamMB      float32
	EstimatedInferenceMs float32
	AccuracyRetention   float32
}

// EdgeOptimizeForDevice auto-optimizes the brain for a target device profile.
func (b *Brain) EdgeOptimizeForDevice(ramMB, cpuCores uint32,
	hasCamera, hasIMU, hasMotorControl, hasNetwork bool, role string) (*EdgeOptimizeResult, error) {
	profile := C.nimcp_device_profile_default()
	profile.ram_mb = C.uint32_t(ramMB)
	profile.cpu_cores = C.uint32_t(cpuCores)
	profile.has_camera = C.bool(hasCamera)
	profile.has_imu = C.bool(hasIMU)
	profile.has_motor_control = C.bool(hasMotorControl)
	profile.has_network = C.bool(hasNetwork)

	switch role {
	case "sensor":
		profile.role = C.NIMCP_DEVICE_SENSOR
	case "actuator":
		profile.role = C.NIMCP_DEVICE_ACTUATOR
	case "coordinator":
		profile.role = C.NIMCP_DEVICE_COORDINATOR
	default:
		profile.role = C.NIMCP_DEVICE_GENERAL
	}

	var report C.nimcp_optimization_report_t
	var child C.nimcp_brain_t
	ret := int(C.nimcp_brain_optimize_for_device(b.handle, &profile, &child, &report))
	return &EdgeOptimizeResult{
		Status:              ret,
		NeuronCount:         uint32(report.neuron_count),
		SubsystemsEnabled:   uint32(report.subsystems_enabled),
		EstimatedRamMB:      float32(report.estimated_ram_mb),
		EstimatedInferenceMs: float32(report.estimated_inference_ms),
		AccuracyRetention:   float32(report.accuracy_retention),
	}, nil
}

// EdgeQuantizeResult contains the result of weight quantization.
type EdgeQuantizeResult struct {
	Status    int
	Precision string
}

// EdgeQuantize quantizes the brain's weights in-place.
func (b *Brain) EdgeQuantize(precision string, calibrationSamples uint32) (*EdgeQuantizeResult, error) {
	config := C.nimcp_quantize_config_default()
	config.calibration_samples = C.uint32_t(calibrationSamples)

	switch precision {
	case "fp16":
		config.weight_precision = C.NIMCP_QUANT_FP16
	case "int8_affine":
		config.weight_precision = C.NIMCP_QUANT_INT8_AFFINE
	case "int4":
		config.weight_precision = C.NIMCP_QUANT_INT4
	case "ternary":
		config.weight_precision = C.NIMCP_QUANT_TERNARY
	default:
		config.weight_precision = C.NIMCP_QUANT_INT8_SYMMETRIC
		precision = "int8_symmetric"
	}

	ret := int(C.nimcp_brain_quantize(b.handle, &config))
	return &EdgeQuantizeResult{Status: ret, Precision: precision}, nil
}

// EdgeScoreImportance scores neuron importance (activity, connectivity, weight magnitude).
func (b *Brain) EdgeScoreImportance(numNeurons uint32) []float32 {
	if numNeurons == 0 {
		numNeurons = 1000
	}
	scores := make([]float32, numNeurons)
	C.nimcp_edge_score_neuron_importance(b.handle,
		(*C.float)(unsafe.Pointer(&scores[0])), C.uint32_t(numNeurons))
	return scores
}

// ============================================================================
// Group 8 — Swarm / Sensor / Watchdog / ROS2 / MAVLink API
// ============================================================================

// SwarmMasterCreate creates a swarm master runtime.
func (b *Brain) SwarmMasterCreate(deviceID, listenPort, syncIntervalMs, heartbeatTimeoutMs, minDevices uint32) error {
	cfg := C.nimcp_swarm_master_config_default()
	cfg.device_id = C.uint32_t(deviceID)
	cfg.listen_port = C.uint16_t(listenPort)
	cfg.sync_interval_ms = C.uint32_t(syncIntervalMs)
	cfg.heartbeat_timeout_ms = C.uint32_t(heartbeatTimeoutMs)
	cfg.min_devices_for_sync = C.uint32_t(minDevices)
	master := C.nimcp_swarm_master_create(b.handle, &cfg)
	if master == nil {
		return &NimcpError{Code: ErrGeneric, Message: "failed to create swarm master"}
	}
	b.swarmMaster = master
	return nil
}

// SwarmMasterDestroy destroys the swarm master.
func (b *Brain) SwarmMasterDestroy() {
	if b.swarmMaster != nil {
		C.nimcp_swarm_master_destroy(b.swarmMaster)
		b.swarmMaster = nil
	}
}

// SwarmMasterStart starts the swarm master event loop.
func (b *Brain) SwarmMasterStart() int { if b.swarmMaster == nil { return -1 }; return int(C.nimcp_swarm_master_start(b.swarmMaster)) }

// SwarmMasterStop stops the swarm master.
func (b *Brain) SwarmMasterStop() int { if b.swarmMaster == nil { return -1 }; return int(C.nimcp_swarm_master_stop(b.swarmMaster)) }

// SwarmMasterKick removes a peer from the swarm.
func (b *Brain) SwarmMasterKick(deviceID uint32) int { if b.swarmMaster == nil { return -1 }; return int(C.nimcp_swarm_master_kick(b.swarmMaster, C.uint32_t(deviceID))) }

// SwarmMasterForceSync triggers an immediate sync round.
func (b *Brain) SwarmMasterForceSync() int { if b.swarmMaster == nil { return -1 }; return int(C.nimcp_swarm_master_force_sync(b.swarmMaster)) }

// SwarmMasterGetPeerCount returns the number of active peers.
func (b *Brain) SwarmMasterGetPeerCount() uint32 { if b.swarmMaster == nil { return 0 }; return uint32(C.nimcp_swarm_master_get_peer_count(b.swarmMaster)) }

// SwarmMasterGetPeerInfo returns peer info for a device.
func (b *Brain) SwarmMasterGetPeerInfo(deviceID uint32) (map[string]interface{}, error) {
	if b.swarmMaster == nil { return nil, &NimcpError{Code: ErrGeneric, Message: "swarm master not created"} }
	var entry C.nimcp_peer_entry_t
	ret := C.nimcp_swarm_master_get_peer_info(b.swarmMaster, C.uint32_t(deviceID), &entry)
	if ret != 0 { return nil, &NimcpError{Code: ErrGeneric, Message: "peer not found"} }
	return map[string]interface{}{
		"device_id": uint32(entry.device_id), "state": int(entry.state),
		"address": C.GoString(&entry.address[0]), "port": uint32(entry.port),
		"missed_heartbeats": uint32(entry.missed_heartbeats),
		"anomaly_count": uint32(entry.anomaly_count),
		"quarantined": bool(entry.quarantined),
		"gradient_norm_ema": float32(entry.gradient_norm_ema),
	}, nil
}

// SwarmEdgeCreate creates a swarm edge runtime.
func (b *Brain) SwarmEdgeCreate(deviceID, heartbeatIntervalMs, reconnectDelayMs uint32, enableLocalLearning bool) error {
	cfg := C.nimcp_swarm_edge_config_default()
	cfg.device_id = C.uint32_t(deviceID)
	cfg.heartbeat_interval_ms = C.uint32_t(heartbeatIntervalMs)
	cfg.reconnect_delay_ms = C.uint32_t(reconnectDelayMs)
	cfg.enable_local_learning = C.bool(enableLocalLearning)
	edge := C.nimcp_swarm_edge_create(b.handle, &cfg)
	if edge == nil { return &NimcpError{Code: ErrGeneric, Message: "failed to create swarm edge"} }
	b.swarmEdge = edge
	return nil
}

// SwarmEdgeDestroy destroys the swarm edge.
func (b *Brain) SwarmEdgeDestroy() { if b.swarmEdge != nil { C.nimcp_swarm_edge_destroy(b.swarmEdge); b.swarmEdge = nil } }

// SwarmEdgeStart starts the swarm edge.
func (b *Brain) SwarmEdgeStart() int { if b.swarmEdge == nil { return -1 }; return int(C.nimcp_swarm_edge_start(b.swarmEdge)) }

// SwarmEdgeStop stops the swarm edge.
func (b *Brain) SwarmEdgeStop() int { if b.swarmEdge == nil { return -1 }; return int(C.nimcp_swarm_edge_stop(b.swarmEdge)) }

// SwarmEdgeIsConnected checks if the edge is connected.
func (b *Brain) SwarmEdgeIsConnected() bool { if b.swarmEdge == nil { return false }; return bool(C.nimcp_swarm_edge_is_connected(b.swarmEdge)) }

// SwarmEdgeSubmitGradients submits local gradients to the master.
func (b *Brain) SwarmEdgeSubmitGradients(gradients []float32) int {
	if b.swarmEdge == nil || len(gradients) == 0 { return -1 }
	return int(C.nimcp_swarm_edge_submit_gradients(b.swarmEdge, (*C.float)(unsafe.Pointer(&gradients[0])), C.uint32_t(len(gradients))))
}

// SensorHubCreate creates a sensor hub. Returns non-nil on error.
func (b *Brain) SensorHubCreate(maxSensors uint32) error {
	hub := C.nimcp_sensor_hub_create(C.uint32_t(maxSensors))
	if hub == nil { return &NimcpError{Code: ErrGeneric, Message: "failed to create sensor hub"} }
	b.sensorHub = hub
	return nil
}

// SensorHubDestroy destroys the sensor hub.
func (b *Brain) SensorHubDestroy() { if b.sensorHub != nil { C.nimcp_sensor_hub_destroy(b.sensorHub); b.sensorHub = nil } }

// SensorRegister registers a sensor with the hub.
func (b *Brain) SensorRegister(sensorID, sensorType, format uint32, name string, sampleRate float32, maxData uint32) int {
	if b.sensorHub == nil { return -1 }
	var desc C.nimcp_sensor_descriptor_t
	desc.sensor_id = C.uint32_t(sensorID)
	desc._type = C.nimcp_sensor_type_t(sensorType)
	desc.format = C.nimcp_sensor_format_t(format)
	cName := C.CString(name); defer C.free(unsafe.Pointer(cName))
	C.strncpy(&desc.name[0], cName, 63)
	desc.sample_rate_hz = C.float(sampleRate)
	desc.max_data_count = C.uint32_t(maxData)
	return int(C.nimcp_sensor_register(b.sensorHub, &desc))
}

// SensorSubmitReading submits a sensor reading.
func (b *Brain) SensorSubmitReading(sensorID uint32, data []float32, confidence float32) int {
	if b.sensorHub == nil || len(data) == 0 { return -1 }
	var reading C.nimcp_sensor_reading_t
	reading.sensor_id = C.uint32_t(sensorID)
	reading.data = (*C.float)(unsafe.Pointer(&data[0]))
	reading.data_count = C.uint32_t(len(data))
	reading.confidence = C.float(confidence)
	reading.valid = C.bool(true)
	return int(C.nimcp_sensor_submit_reading(b.sensorHub, &reading))
}

// SensorGetCount returns the number of registered sensors.
func (b *Brain) SensorGetCount() uint32 { if b.sensorHub == nil { return 0 }; return uint32(C.nimcp_sensor_get_count(b.sensorHub)) }

// SensorComposeFeatures composes a feature vector from all sensors.
func (b *Brain) SensorComposeFeatures(maxFeatures uint32) []float32 {
	if b.sensorHub == nil { return nil }
	features := make([]float32, maxFeatures)
	count := int(C.nimcp_sensor_compose_feature_vector(b.sensorHub, (*C.float)(unsafe.Pointer(&features[0])), C.uint32_t(maxFeatures)))
	if count < 0 { return nil }
	return features[:count]
}

// WatchdogCreate creates a safety watchdog.
func (b *Brain) WatchdogCreate(timeoutMs uint32, action int, maxMagnitude float32, maxOutputs uint32) error {
	cfg := C.nimcp_watchdog_config_default()
	cfg.timeout_ms = C.uint32_t(timeoutMs)
	cfg.action = C.nimcp_safe_action_t(action)
	cfg.validation.max_output_magnitude = C.float(maxMagnitude)
	cfg.max_outputs = C.uint32_t(maxOutputs)
	wd := C.nimcp_watchdog_create(&cfg)
	if wd == nil { return &NimcpError{Code: ErrGeneric, Message: "failed to create watchdog"} }
	b.watchdog = wd
	return nil
}

// WatchdogDestroy destroys the watchdog.
func (b *Brain) WatchdogDestroy() { if b.watchdog != nil { C.nimcp_watchdog_destroy(b.watchdog); b.watchdog = nil } }

// WatchdogArm arms the watchdog.
func (b *Brain) WatchdogArm() int { if b.watchdog == nil { return -1 }; return int(C.nimcp_watchdog_arm(b.watchdog)) }

// WatchdogDisarm disarms the watchdog.
func (b *Brain) WatchdogDisarm() int { if b.watchdog == nil { return -1 }; return int(C.nimcp_watchdog_disarm(b.watchdog)) }

// WatchdogHeartbeat sends a heartbeat.
func (b *Brain) WatchdogHeartbeat() { if b.watchdog != nil { C.nimcp_watchdog_heartbeat(b.watchdog) } }

// WatchdogValidateOutput validates brain output. Returns true if valid.
func (b *Brain) WatchdogValidateOutput(output []float32) bool {
	if b.watchdog == nil || len(output) == 0 { return false }
	return C.nimcp_watchdog_validate_output(b.watchdog, (*C.float)(unsafe.Pointer(&output[0])), C.uint32_t(len(output))) == 0
}

// WatchdogGetSafeOutput returns safe output values.
func (b *Brain) WatchdogGetSafeOutput(numOutputs uint32) []float32 {
	if b.watchdog == nil { return nil }
	out := make([]float32, numOutputs)
	C.nimcp_watchdog_get_safe_output(b.watchdog, (*C.float)(unsafe.Pointer(&out[0])), C.uint32_t(numOutputs))
	return out
}

// WatchdogEstop triggers an emergency stop.
func (b *Brain) WatchdogEstop() { if b.watchdog != nil { C.nimcp_watchdog_estop(b.watchdog) } }

// WatchdogReset resets the watchdog.
func (b *Brain) WatchdogReset() int { if b.watchdog == nil { return -1 }; return int(C.nimcp_watchdog_reset(b.watchdog)) }

// WatchdogGetState returns the current watchdog state name.
func (b *Brain) WatchdogGetState() string {
	if b.watchdog == nil { return "NONE" }
	return C.GoString(C.nimcp_watchdog_state_name(C.nimcp_watchdog_get_state(b.watchdog)))
}

// Ros2BridgeCreate creates a ROS 2 bridge.
func (b *Brain) Ros2BridgeCreate(nodeName string, cmdRate, infRate float32, inputDim uint32, subIMU, subOdom bool) error {
	cfg := C.nimcp_ros2_config_default()
	cName := C.CString(nodeName); defer C.free(unsafe.Pointer(cName))
	cfg.node_name = cName
	cfg.cmd_rate_hz = C.float(cmdRate)
	cfg.inference_rate_hz = C.float(infRate)
	cfg.brain_input_dim = C.uint32_t(inputDim)
	cfg.subscribe_imu = C.bool(subIMU)
	cfg.subscribe_odom = C.bool(subOdom)
	bridge := C.nimcp_ros2_bridge_create(b.handle, &cfg)
	if bridge == nil { return &NimcpError{Code: ErrGeneric, Message: "failed to create ROS2 bridge"} }
	b.ros2Bridge = bridge
	return nil
}

// Ros2BridgeDestroy destroys the ROS 2 bridge.
func (b *Brain) Ros2BridgeDestroy() { if b.ros2Bridge != nil { C.nimcp_ros2_bridge_destroy(b.ros2Bridge); b.ros2Bridge = nil } }

// Ros2BridgeStart starts the ROS 2 bridge.
func (b *Brain) Ros2BridgeStart() int { if b.ros2Bridge == nil { return -1 }; return int(C.nimcp_ros2_bridge_start(b.ros2Bridge)) }

// Ros2BridgeStop stops the ROS 2 bridge.
func (b *Brain) Ros2BridgeStop() int { if b.ros2Bridge == nil { return -1 }; return int(C.nimcp_ros2_bridge_stop(b.ros2Bridge)) }

// Ros2BridgeInjectSensor injects sensor data.
func (b *Brain) Ros2BridgeInjectSensor(topic string, data []float32) int {
	if b.ros2Bridge == nil || len(data) == 0 { return -1 }
	cTopic := C.CString(topic); defer C.free(unsafe.Pointer(cTopic))
	return int(C.nimcp_ros2_bridge_inject_sensor(b.ros2Bridge, cTopic, (*C.float)(unsafe.Pointer(&data[0])), C.uint32_t(len(data))))
}

// Ros2BridgeGetLastCmd returns the last motor command.
func (b *Brain) Ros2BridgeGetLastCmd(maxCount uint32) []float32 {
	if b.ros2Bridge == nil { return nil }
	data := make([]float32, maxCount)
	got := int(C.nimcp_ros2_bridge_get_last_cmd(b.ros2Bridge, (*C.float)(unsafe.Pointer(&data[0])), C.uint32_t(maxCount)))
	if got < 0 { return nil }
	return data[:got]
}

// MavlinkCreate creates a MAVLink bridge.
func (b *Brain) MavlinkCreate(connString string, connType int, baudRate, sysID uint32, geofence float32) error {
	cfg := C.nimcp_mavlink_config_default()
	cStr := C.CString(connString); defer C.free(unsafe.Pointer(cStr))
	C.strncpy(&cfg.connection_string[0], cStr, 255)
	cfg.conn_type = C.nimcp_mavlink_conn_type_t(connType)
	cfg.baud_rate = C.uint32_t(baudRate)
	cfg.system_id = C.uint8_t(sysID)
	cfg.geofence_radius_m = C.float(geofence)
	bridge := C.nimcp_mavlink_bridge_create(&cfg)
	if bridge == nil { return &NimcpError{Code: ErrGeneric, Message: "failed to create MAVLink bridge"} }
	b.mavlinkBridge = bridge
	return nil
}

// MavlinkDestroy destroys the MAVLink bridge.
func (b *Brain) MavlinkDestroy() { if b.mavlinkBridge != nil { C.nimcp_mavlink_bridge_destroy(b.mavlinkBridge); b.mavlinkBridge = nil } }

// MavlinkConnect opens the connection.
func (b *Brain) MavlinkConnect() int { if b.mavlinkBridge == nil { return -1 }; return int(C.nimcp_mavlink_bridge_connect(b.mavlinkBridge)) }

// MavlinkDisconnect closes the connection.
func (b *Brain) MavlinkDisconnect() int { if b.mavlinkBridge == nil { return -1 }; return int(C.nimcp_mavlink_bridge_disconnect(b.mavlinkBridge)) }

// MavlinkStart starts the receive thread.
func (b *Brain) MavlinkStart() int { if b.mavlinkBridge == nil { return -1 }; return int(C.nimcp_mavlink_bridge_start(b.mavlinkBridge)) }

// MavlinkStop stops the receive thread.
func (b *Brain) MavlinkStop() int { if b.mavlinkBridge == nil { return -1 }; return int(C.nimcp_mavlink_bridge_stop(b.mavlinkBridge)) }

// MavlinkGetAttitude returns the latest attitude data.
func (b *Brain) MavlinkGetAttitude() map[string]float64 {
	if b.mavlinkBridge == nil { return nil }
	var att C.nimcp_mavlink_attitude_t
	if C.nimcp_mavlink_get_attitude(b.mavlinkBridge, &att) != 0 { return nil }
	return map[string]float64{"roll": float64(att.roll), "pitch": float64(att.pitch), "yaw": float64(att.yaw), "rollspeed": float64(att.rollspeed), "pitchspeed": float64(att.pitchspeed), "yawspeed": float64(att.yawspeed)}
}

// MavlinkGetPosition returns the latest position data.
func (b *Brain) MavlinkGetPosition() map[string]float64 {
	if b.mavlinkBridge == nil { return nil }
	var pos C.nimcp_mavlink_position_t
	if C.nimcp_mavlink_get_position(b.mavlinkBridge, &pos) != 0 { return nil }
	return map[string]float64{"latitude": float64(pos.latitude), "longitude": float64(pos.longitude), "altitude_msl": float64(pos.altitude_msl), "altitude_rel": float64(pos.altitude_rel), "vx": float64(pos.vx), "vy": float64(pos.vy), "vz": float64(pos.vz), "heading": float64(pos.heading)}
}

// MavlinkGetBattery returns the latest battery data.
func (b *Brain) MavlinkGetBattery() map[string]float64 {
	if b.mavlinkBridge == nil { return nil }
	var bat C.nimcp_mavlink_battery_t
	if C.nimcp_mavlink_get_battery(b.mavlinkBridge, &bat) != 0 { return nil }
	return map[string]float64{"voltage": float64(bat.voltage), "current": float64(bat.current), "remaining_pct": float64(bat.remaining_pct), "consumed_mah": float64(bat.consumed_mah)}
}

// MavlinkSetVelocity sets velocity.
func (b *Brain) MavlinkSetVelocity(vx, vy, vz, yawRate float32) int { if b.mavlinkBridge == nil { return -1 }; return int(C.nimcp_mavlink_set_velocity(b.mavlinkBridge, C.float(vx), C.float(vy), C.float(vz), C.float(yawRate))) }

// MavlinkArm arms or disarms the vehicle.
func (b *Brain) MavlinkArm(arm bool) int { if b.mavlinkBridge == nil { return -1 }; return int(C.nimcp_mavlink_arm(b.mavlinkBridge, C.bool(arm))) }

// MavlinkTakeoff commands takeoff.
func (b *Brain) MavlinkTakeoff(altitude float32) int { if b.mavlinkBridge == nil { return -1 }; return int(C.nimcp_mavlink_takeoff(b.mavlinkBridge, C.float(altitude))) }

// MavlinkLand commands landing.
func (b *Brain) MavlinkLand() int { if b.mavlinkBridge == nil { return -1 }; return int(C.nimcp_mavlink_land(b.mavlinkBridge)) }

// MavlinkGoto commands go-to position.
func (b *Brain) MavlinkGoto(lat, lon float64, alt float32) int { if b.mavlinkBridge == nil { return -1 }; return int(C.nimcp_mavlink_goto(b.mavlinkBridge, C.double(lat), C.double(lon), C.float(alt))) }

// MavlinkRtl commands return to launch.
func (b *Brain) MavlinkRtl() int { if b.mavlinkBridge == nil { return -1 }; return int(C.nimcp_mavlink_rtl(b.mavlinkBridge)) }

// MavlinkComposeFeatures composes a brain-input feature vector from telemetry.
func (b *Brain) MavlinkComposeFeatures() []float32 {
	if b.mavlinkBridge == nil { return nil }
	features := make([]float32, C.NIMCP_MAVLINK_FEATURE_COUNT)
	count := int(C.nimcp_mavlink_compose_features(b.mavlinkBridge, (*C.float)(unsafe.Pointer(&features[0])), C.NIMCP_MAVLINK_FEATURE_COUNT))
	if count < 0 { return nil }
	return features[:count]
}

// ============================================================================
// Group 9 — Memory Store API
// ============================================================================

// MemoryStoreStats contains memory store statistics.
type MemoryStoreStats struct {
	TotalEngrams  uint64
	TotalConcepts uint64
	TotalRelations uint64
	TotalAutobio  uint64
	TotalWrites   uint64
	TotalReads    uint64
	CacheHits     uint64
	CacheMisses   uint64
	DBSizeBytes   uint64
}

// MemoryStoreStats returns statistics from the persistent memory store.
// Returns nil if no memory store is active.
func (b *Brain) MemoryStoreStats() *MemoryStoreStats {
	var out C.go_memory_store_stats_t
	if !bool(C.go_brain_memory_store_stats(b.handle, &out)) {
		return nil
	}
	return &MemoryStoreStats{
		TotalEngrams:  uint64(out.total_engrams),
		TotalConcepts: uint64(out.total_concepts),
		TotalRelations: uint64(out.total_relations),
		TotalAutobio:  uint64(out.total_autobio),
		TotalWrites:   uint64(out.total_writes),
		TotalReads:    uint64(out.total_reads),
		CacheHits:     uint64(out.cache_hits),
		CacheMisses:   uint64(out.cache_misses),
		DBSizeBytes:   uint64(out.db_size_bytes),
	}
}

// MemorySearchText searches memory by text query (FTS5 full-text search).
// Returns a list of matching engram IDs.
func (b *Brain) MemorySearchText(query string, maxResults uint32) []uint64 {
	cQuery := C.CString(query)
	defer C.free(unsafe.Pointer(cQuery))
	result := C.go_brain_memory_search_text(b.handle, cQuery, C.uint32_t(maxResults))
	defer C.go_free_id_array(&result)
	if result.count == 0 || result.ids == nil {
		return nil
	}
	ids := make([]uint64, result.count)
	cIDs := unsafe.Slice((*uint64)(unsafe.Pointer(result.ids)), result.count)
	copy(ids, cIDs)
	return ids
}

// SimilarityResult represents a single similarity search result.
type SimilarityResult struct {
	ID       uint64
	Distance float32
}

// MemorySearchSimilar searches memory by embedding similarity.
// Returns a list of (id, distance) pairs.
func (b *Brain) MemorySearchSimilar(embedding []float32, topK uint32) []SimilarityResult {
	if len(embedding) == 0 {
		return nil
	}
	result := C.go_brain_memory_search_similar(
		b.handle,
		(*C.float)(unsafe.Pointer(&embedding[0])),
		C.uint32_t(len(embedding)),
		C.uint32_t(topK))
	defer C.go_free_similarity_result(&result)
	if result.count == 0 || result.ids == nil {
		return nil
	}
	out := make([]SimilarityResult, result.count)
	cIDs := unsafe.Slice((*uint64)(unsafe.Pointer(result.ids)), result.count)
	cDists := unsafe.Slice((*float32)(unsafe.Pointer(result.distances)), result.count)
	for i := uint32(0); i < uint32(result.count); i++ {
		out[i] = SimilarityResult{ID: cIDs[i], Distance: cDists[i]}
	}
	return out
}

// MemoryIsHealthy returns true if the memory store has no unrecoverable flush errors.
func (b *Brain) MemoryIsHealthy() bool {
	return bool(C.go_brain_memory_is_healthy(b.handle))
}

// ============================================================================
// Group 9 — OOD Detection API
// ============================================================================

// OODStats contains out-of-distribution detection statistics.
type OODStats struct {
	TotalChecks    uint64
	OODDetected    uint64
	InDistribution uint64
	AvgOODScore    float32
	OODRate        float32
}

// OODStats returns out-of-distribution detection statistics.
// Returns nil if no OOD detector is active.
func (b *Brain) OODStats() *OODStats {
	var out C.go_ood_stats_t
	if !bool(C.go_brain_ood_stats(b.handle, &out)) {
		return nil
	}
	return &OODStats{
		TotalChecks:    uint64(out.total_checks),
		OODDetected:    uint64(out.ood_detected),
		InDistribution: uint64(out.in_distribution),
		AvgOODScore:    float32(out.avg_ood_score),
		OODRate:        float32(out.ood_rate),
	}
}

// ============================================================================
// Group 10 — Security Audit API
// ============================================================================

// AuditLog logs a security audit event to the memory store.
// Returns 0 on success, -1 on failure.
func (b *Brain) AuditLog(description string, severity uint32, details string) int {
	cDesc := C.CString(description)
	defer C.free(unsafe.Pointer(cDesc))
	cDetails := C.CString(details)
	defer C.free(unsafe.Pointer(cDetails))
	return int(C.go_brain_audit_log(b.handle, cDesc, C.uint32_t(severity), cDetails))
}

// AuditEntry represents a single audit search result.
type AuditEntry struct {
	ID       uint64
	Severity float32
}

// AuditSearch searches the security audit trail.
func (b *Brain) AuditSearch(minSeverity, maxResults uint32) []AuditEntry {
	result := C.go_brain_audit_search(b.handle, C.uint32_t(minSeverity), C.uint32_t(maxResults))
	defer C.go_free_similarity_result(&result)
	if result.count == 0 || result.ids == nil {
		return nil
	}
	out := make([]AuditEntry, result.count)
	cIDs := unsafe.Slice((*uint64)(unsafe.Pointer(result.ids)), result.count)
	cDists := unsafe.Slice((*float32)(unsafe.Pointer(result.distances)), result.count)
	for i := uint32(0); i < uint32(result.count); i++ {
		out[i] = AuditEntry{ID: cIDs[i], Severity: cDists[i]}
	}
	return out
}
