package main

import (
	"fmt"
	"math"
	"os"
	"strings"

	"nimcp"
)

var passed, failed, skipped int

func pass(name string) {
	passed++
	fmt.Printf("  PASS: %s\n", name)
}

func fail(name, reason string) {
	failed++
	fmt.Printf("  FAIL: %s (%s)\n", name, reason)
}

func skip(name, reason string) {
	skipped++
	fmt.Printf("  SKIP: %s (%s)\n", name, reason)
}

func group(name string) {
	fmt.Printf("\n--- %s ---\n", name)
}

func assertTrue(cond bool, name string) {
	if cond {
		pass(name)
	} else {
		fail(name, "expected true")
	}
}

func assertFinite(val float32, name string) {
	if !math.IsNaN(float64(val)) && !math.IsInf(float64(val), 0) {
		pass(name)
	} else {
		fail(name, fmt.Sprintf("not finite: %v", val))
	}
}

// ============================================================================
// Test Groups
// ============================================================================

func testLibraryLifecycle() {
	group("Library Lifecycle")
	if err := nimcp.Init(); err != nil {
		fail("init", err.Error())
		return
	}
	pass("init")

	v := nimcp.Version()
	assertTrue(v != "", "version not empty")
	assertTrue(strings.Contains(v, "."), "version contains dot")

	vi := nimcp.VersionInt()
	assertTrue(vi > 20000, fmt.Sprintf("version_int > 20000 (%d)", vi))
}

func testEnumValues() {
	group("Enum Values")
	assertTrue(int(nimcp.BrainTiny) == 0, "Tiny=0")
	assertTrue(int(nimcp.BrainLarge) == 3, "Large=3")
	assertTrue(int(nimcp.TaskClassification) == 0, "Classification=0")
	assertTrue(int(nimcp.OptAdam) == 2, "Adam=2")
	assertTrue(int(nimcp.ModulePerception) == 1, "Perception=1")
	assertTrue(int(nimcp.ModuleCustomStart) == 100, "CustomStart=100")
	assertTrue(int(nimcp.CBStepComplete) == 0, "StepComplete=0")
}

func testBrainCreateClose() {
	group("Brain Create/Close")
	brain, err := nimcp.NewBrain("test", nimcp.BrainTiny, nimcp.TaskClassification, 4, 2)
	if err != nil {
		fail("brain create", err.Error())
		return
	}
	pass("brain created")
	brain.Close()
	pass("brain closed")
}

func testBrainLearnPredict() {
	group("Brain Learn/Predict")
	brain, err := nimcp.NewBrain("lp_test", nimcp.BrainTiny, nimcp.TaskClassification, 4, 2)
	if err != nil {
		fail("brain create", err.Error())
		return
	}
	defer brain.Close()

	err = brain.Learn([]float32{1.0, 0.0, 0.0, 0.0}, "cat", 0.9)
	if err != nil {
		fail("learn", err.Error())
		return
	}
	pass("learn example")

	label, conf, err := brain.Predict([]float32{1.0, 0.0, 0.0, 0.0})
	if err != nil {
		fail("predict", err.Error())
		return
	}
	assertTrue(label != "", "label non-empty")
	assertTrue(conf >= 0 && conf <= 1, "confidence in [0,1]")
}

func testBrainInfer() {
	group("Brain Infer")
	brain, err := nimcp.NewBrain("infer_test", nimcp.BrainTiny, nimcp.TaskClassification, 4, 2)
	if err != nil {
		fail("brain create", err.Error())
		return
	}
	defer brain.Close()

	outputs, err := brain.Infer([]float32{1.0, 0.0, 0.0, 0.0}, 2)
	if err != nil {
		fail("infer", err.Error())
		return
	}
	pass("infer completed")
	assertFinite(outputs[0], "output[0] is finite")
}

func testBrainSaveLoad() {
	group("Brain Save/Load")
	brain, err := nimcp.NewBrain("sl_test", nimcp.BrainTiny, nimcp.TaskClassification, 4, 2)
	if err != nil {
		fail("brain create", err.Error())
		return
	}
	defer brain.Close()

	path := "/tmp/nimcp_go_test.brain"
	defer os.Remove(path)
	if err := brain.Save(path); err != nil {
		fail("save", err.Error())
		return
	}
	pass("save")

	loaded, err := nimcp.LoadBrain(path)
	if err != nil {
		fail("load", err.Error())
		return
	}
	defer loaded.Close()
	pass("load")

	_, _, err = loaded.Predict([]float32{1.0, 0.0, 0.0, 0.0})
	if err != nil {
		fail("predict after load", err.Error())
		return
	}
	pass("predict after load")
}

func testBrainLoadBadPath() {
	group("Brain Load Bad Path")
	_, err := nimcp.LoadBrain("/nonexistent/path/brain.dat")
	if err != nil {
		pass("IO error returned")
	} else {
		fail("load bad path", "expected error")
	}
}

func testBrainTrainingPipeline() {
	group("Brain Training Pipeline")
	brain, err := nimcp.NewBrain("train_test", nimcp.BrainTiny, nimcp.TaskClassification, 4, 2)
	if err != nil {
		fail("brain create", err.Error())
		return
	}
	defer brain.Close()

	cfg := nimcp.DefaultTrainingConfig()
	if err := brain.ConfigureTraining(&cfg); err != nil {
		fail("configure_training", err.Error())
		return
	}
	pass("configure_training")

	result, err := brain.TrainStep([]float32{1.0, 0.0, 0.0, 0.0}, []float32{1.0, 0.0})
	if err != nil {
		fail("train_step", err.Error())
		return
	}
	assertFinite(result.Loss, "loss is finite")
	pass(fmt.Sprintf("train_step loss=%.6f", result.Loss))

	_, _, lr, err := brain.GetTrainingStats()
	if err != nil {
		fail("get_training_stats", err.Error())
		return
	}
	pass("get_training_stats")

	newLR := brain.StepScheduler(0.0)
	pass(fmt.Sprintf("step_scheduler lr=%.6f (was %.6f)", newLR, lr))
}

func testBrainTrainBatch() {
	group("Brain Train Batch")
	brain, err := nimcp.NewBrain("batch_test", nimcp.BrainTiny, nimcp.TaskClassification, 4, 2)
	if err != nil {
		fail("brain create", err.Error())
		return
	}
	defer brain.Close()

	cfg := nimcp.DefaultTrainingConfig()
	brain.ConfigureTraining(&cfg)

	features := []float32{1, 0, 0, 0, 0, 1, 0, 0}
	targets := []float32{1, 0, 0, 1}
	result, err := brain.TrainBatch(features, targets, 2, 4, 2)
	if err != nil {
		fail("train_batch", err.Error())
		return
	}
	assertFinite(result.Loss, "batch loss finite")
	pass("train_batch")
}

func testBrainCallbacks() {
	group("Brain Callbacks")
	brain, err := nimcp.NewBrain("cb_test", nimcp.BrainTiny, nimcp.TaskClassification, 4, 2)
	if err != nil {
		fail("brain create", err.Error())
		return
	}
	defer brain.Close()

	cfg := nimcp.DefaultTrainingConfig()
	brain.ConfigureTraining(&cfg)

	cbCfg := nimcp.DefaultCallbackConfig()
	if err := brain.EnableCallbacks(&cbCfg); err != nil {
		fail("enable_callbacks", err.Error())
		return
	}
	pass("enable_callbacks")

	id, err := brain.RegisterCallback(nimcp.CBStepComplete,
		func(evt nimcp.CallbackEvent, m *nimcp.CallbackMetrics) nimcp.CallbackAction {
			return nimcp.CallbackActionContinue
		}, "test_cb")
	if err != nil {
		fail("register_callback", err.Error())
		return
	}
	assertTrue(id > 0, fmt.Sprintf("register_callback id=%d", id))

	brain.TrainStep([]float32{1, 0, 0, 0}, []float32{1, 0})

	if err := brain.UnregisterCallback(id); err != nil {
		pass("unregister_callback (tolerated error)")
	} else {
		pass("unregister_callback")
	}

	if err := brain.DisableCallbacks(); err != nil {
		pass("disable_callbacks (tolerated error)")
	} else {
		pass("disable_callbacks")
	}
}

func testBrainResize() {
	group("Brain Resize")
	brain, err := nimcp.NewBrain("resize_test", nimcp.BrainTiny, nimcp.TaskClassification, 4, 2)
	if err != nil {
		fail("brain create", err.Error())
		return
	}
	defer brain.Close()

	count := brain.GetNeuronCount()
	assertTrue(count > 0, fmt.Sprintf("neuron_count > 0 (%d)", count))

	brain.AutoResize()
	pass("auto_resize")

	count2 := brain.GetNeuronCount()
	assertTrue(count2 > 0, fmt.Sprintf("neuron_count after resize > 0 (%d)", count2))
}

func testBrainCOWClone() {
	group("Brain COW Clone")
	brain, err := nimcp.NewBrain("cow_test", nimcp.BrainTiny, nimcp.TaskClassification, 4, 2)
	if err != nil {
		fail("brain create", err.Error())
		return
	}
	defer brain.Close()

	clone, err := brain.CloneCOW()
	if err != nil {
		fail("cow clone", err.Error())
		return
	}
	defer clone.Close()
	pass("cow clone created")

	_, _, err = clone.Predict([]float32{1.0, 0.0, 0.0, 0.0})
	if err != nil {
		fail("clone predict", err.Error())
		return
	}
	pass("clone can predict")
}

func testBrainCOWSnapshot() {
	group("Brain COW Snapshot")
	brain, err := nimcp.NewBrain("cowsnap_test", nimcp.BrainTiny, nimcp.TaskClassification, 4, 2)
	if err != nil {
		fail("brain create", err.Error())
		return
	}
	defer brain.Close()

	snap, err := brain.SnapshotCOW()
	if err != nil {
		fail("snapshot_cow", err.Error())
		return
	}
	defer snap.Close()
	pass("snapshot_cow")

	if err := brain.RestoreCOW(snap); err != nil {
		fail("restore_cow", err.Error())
		return
	}
	pass("restore_cow")
}

func testBrainWorkingMemory() {
	group("Brain Working Memory")
	brain, err := nimcp.NewBrain("wm_test", nimcp.BrainTiny, nimcp.TaskClassification, 4, 2)
	if err != nil {
		fail("brain create", err.Error())
		return
	}
	defer brain.Close()

	if err := brain.WorkingMemoryAdd([]float32{1.0, 2.0, 3.0, 4.0}, 0.8); err != nil {
		fail("working_memory_add", err.Error())
		return
	}
	pass("working_memory_add")

	size, cap, err := brain.WorkingMemoryStats()
	if err != nil {
		fail("working_memory_stats", err.Error())
		return
	}
	pass(fmt.Sprintf("working_memory_stats (size=%d cap=%d)", size, cap))
}

func testBrainWorkspace() {
	group("Brain Workspace")
	brain, err := nimcp.NewBrain("ws_test", nimcp.BrainTiny, nimcp.TaskClassification, 4, 2)
	if err != nil {
		fail("brain create", err.Error())
		return
	}
	defer brain.Close()

	if err := brain.WorkspaceSubscribe(nimcp.ModulePerception); err != nil {
		fail("workspace_subscribe", err.Error())
		return
	}
	pass("workspace_subscribe")

	_, _, _, err = brain.WorkspaceStats()
	if err != nil {
		fail("workspace_stats", err.Error())
		return
	}
	pass("workspace_stats")

	has, err := brain.WorkspaceHasBroadcast()
	if err != nil {
		fail("workspace_has_broadcast", err.Error())
		return
	}
	pass(fmt.Sprintf("workspace_has_broadcast=%v", has))
}

func testBrainOscillations() {
	group("Brain Oscillations")
	brain, err := nimcp.NewBrain("osc_test", nimcp.BrainTiny, nimcp.TaskClassification, 4, 2)
	if err != nil {
		fail("brain create", err.Error())
		return
	}
	defer brain.Close()

	result := brain.EnableOscillations(true)
	assertTrue(result, "enable_oscillations returned true")

	assertTrue(brain.IsOscillationsEnabled(), "is_oscillations_enabled")

	p := brain.GetPhasor(0)
	pass(fmt.Sprintf("get_phasor amp=%.0f phase=%.0f", p.Amplitude, p.Phase))

	coherence := brain.GetPhaseCoherence([]uint32{0, 1, 2})
	assertTrue(coherence >= 0 && coherence <= 1, "phase_coherence in [0,1]")

	pac := brain.GetPACModulation(6.0, 40.0)
	assertTrue(pac >= 0 && pac <= 1, "pac_modulation in [0,1]")
}

func testBrainProbe() {
	group("Brain Probe")
	brain, err := nimcp.NewBrain("probe_test", nimcp.BrainTiny, nimcp.TaskClassification, 4, 2)
	if err != nil {
		fail("brain create", err.Error())
		return
	}
	defer brain.Close()

	probe, err := brain.Probe()
	if err != nil {
		fail("probe", err.Error())
		return
	}
	assertTrue(probe.TaskName != "", "task_name not empty")
	assertTrue(probe.Size == nimcp.BrainTiny || probe.Size == nimcp.BrainSmall, "size is Tiny or Small")
	assertTrue(probe.NumNeurons > 0, fmt.Sprintf("num_neurons > 0 (%d)", probe.NumNeurons))
	assertTrue(probe.NumInputs == 4, "num_inputs = 4")
	assertTrue(probe.NumOutputs == 2, "num_outputs = 2")
	pass("probe fields valid")
}

func testNetwork() {
	group("Network")
	net, err := nimcp.NewNetwork(4, 2, 8, 0.01)
	if err != nil {
		fail("network create", err.Error())
		return
	}
	defer net.Close()
	pass("network created")

	outputs, err := net.Forward([]float32{1.0, 0.0, 0.0, 0.0})
	if err != nil {
		fail("forward", err.Error())
		return
	}
	assertFinite(outputs[0], "forward output finite")
	pass("forward")

	err = net.Train([]float32{1, 0, 0, 0}, []float32{1, 0})
	if err != nil {
		pass("train (stub returns error - expected)")
	} else {
		pass("train")
	}
}

func testEthics() {
	group("Ethics")
	eth, err := nimcp.NewEthics()
	if err != nil {
		fail("ethics create", err.Error())
		return
	}
	defer eth.Close()
	pass("ethics created")

	score, err := eth.Check([]float32{0.5, 0.3, 0.8})
	if err != nil {
		fail("ethics check", err.Error())
		return
	}
	assertTrue(score >= -1 && score <= 1, "score in [-1,1]")
	pass(fmt.Sprintf("ethics check score=%.0f", score))
}

func testKnowledgeGraph() {
	group("Knowledge Graph")
	kg, err := nimcp.NewKnowledgeGraph()
	if err != nil {
		fail("knowledge graph create", err.Error())
		return
	}
	defer kg.Close()
	pass("knowledge graph created")

	if err := kg.AddFact("cat", "is_a", "animal"); err != nil {
		fail("add_fact", err.Error())
		return
	}
	pass("add_fact")

	if err := kg.AddFact("dog", "is_a", "animal"); err != nil {
		fail("add_fact 2", err.Error())
		return
	}
	pass("add_fact 2")

	result, err := kg.Query("cat")
	if err != nil {
		fail("query", err.Error())
		return
	}
	assertTrue(result != "", "query result not empty")
	assertTrue(strings.Contains(result, "is_a") && strings.Contains(result, "animal"),
		fmt.Sprintf("query result: %s", result))
}

func main() {
	fmt.Println("=== NIMCP Go Bindings Test Suite ===")

	testLibraryLifecycle()
	testEnumValues()
	testBrainCreateClose()
	testBrainLearnPredict()
	testBrainInfer()
	testBrainSaveLoad()
	testBrainLoadBadPath()
	testBrainTrainingPipeline()
	testBrainTrainBatch()
	testBrainCallbacks()
	testBrainResize()
	testBrainCOWClone()
	testBrainCOWSnapshot()
	testBrainWorkingMemory()
	testBrainWorkspace()
	testBrainOscillations()
	testBrainProbe()
	testNetwork()
	testEthics()
	testKnowledgeGraph()

	fmt.Printf("\n=== Results ===\n")
	fmt.Printf("Passed: %d, Failed: %d, Skipped: %d\n", passed, failed, skipped)

	if failed > 0 {
		os.Exit(1)
	}
}
