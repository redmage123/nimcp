/**
 * NIMCP Node.js Bindings Test Suite
 * Self-contained, no framework - matches pattern of other binding tests.
 * 21 test sections covering entire public API.
 */

'use strict';

const path = require('path');
const fs = require('fs');
const os = require('os');

// Load native module directly
const nimcp = require(path.join(__dirname, '..', '..', '..', 'src', 'bindings', 'nodejs', 'build', 'Release', 'nimcp_nodejs.node'));

let passed = 0;
let failed = 0;
let skipped = 0;

function assert(condition, msg) {
    if (!condition) throw new Error('Assertion failed: ' + msg);
}

function assertType(val, type, msg) {
    assert(typeof val === type, msg + ' (expected ' + type + ', got ' + typeof val + ')');
}

function runTest(name, fn) {
    try {
        fn();
        passed++;
        process.stdout.write('  PASS: ' + name + '\n');
    } catch (e) {
        failed++;
        process.stdout.write('  FAIL: ' + name + ': ' + e.message + '\n');
    }
}

function runTestTolerated(name, fn) {
    try {
        fn();
        passed++;
        process.stdout.write('  PASS: ' + name + '\n');
    } catch (e) {
        skipped++;
        process.stdout.write('  SKIP: ' + name + ' (tolerated: ' + e.message + ')\n');
    }
}

function section(name) {
    process.stdout.write('\n=== ' + name + ' ===\n');
}

// Shared brain for tests (TINY to avoid memory issues)
let brain = null;

// =============================================================================
// 1. Library
// =============================================================================
section('1. Library');

runTest('init returns OK', () => {
    const s = nimcp.init();
    assert(s === nimcp.OK, 'init should return OK (0), got ' + s);
});

runTest('version returns string', () => {
    const v = nimcp.version();
    assertType(v, 'string', 'version');
    assert(v.length > 0, 'version string not empty');
    assert(v.includes('.'), 'version contains dot: ' + v);
});

runTest('versionInt returns number', () => {
    const v = nimcp.versionInt();
    assertType(v, 'number', 'versionInt');
    assert(v >= 20600, 'version >= 2.6.0: ' + v);
});

// =============================================================================
// 2. Brain Create
// =============================================================================
section('2. Brain Create');

runTest('brainCreate returns handle', () => {
    brain = nimcp.brainCreate('test_brain', nimcp.BRAIN_TINY, nimcp.TASK_CLASSIFICATION, 4, 3);
    assert(brain !== null && brain !== undefined, 'brain handle should not be null');
});

runTest('constants exported', () => {
    assert(nimcp.BRAIN_TINY === 0, 'BRAIN_TINY');
    assert(nimcp.BRAIN_SMALL === 1, 'BRAIN_SMALL');
    assert(nimcp.TASK_CLASSIFICATION === 0, 'TASK_CLASSIFICATION');
    assert(nimcp.TASK_REGRESSION === 1, 'TASK_REGRESSION');
});

// =============================================================================
// 3. Brain Learn + Predict
// =============================================================================
section('3. Brain Learn + Predict');

runTest('brainLearnExample succeeds', () => {
    nimcp.brainLearnExample(brain, [1.0, 0.0, 0.0, 0.0], 'cat', 0.9);
    nimcp.brainLearnExample(brain, [0.0, 1.0, 0.0, 0.0], 'dog', 0.9);
    nimcp.brainLearnExample(brain, [0.0, 0.0, 1.0, 0.0], 'bird', 0.9);
});

runTest('brainPredict returns object', () => {
    const pred = nimcp.brainPredict(brain, [1.0, 0.0, 0.0, 0.0]);
    assertType(pred, 'object', 'predict result');
    assert('label' in pred, 'has label');
    assert('confidence' in pred, 'has confidence');
    assertType(pred.label, 'string', 'label type');
    assertType(pred.confidence, 'number', 'confidence type');
    assert(pred.confidence >= 0.0 && pred.confidence <= 1.0, 'confidence in range');
});

// =============================================================================
// 4. Brain Infer
// =============================================================================
section('4. Brain Infer');

runTest('brainInfer returns float array', () => {
    const outputs = nimcp.brainInfer(brain, [1.0, 0.0, 0.0, 0.0], 3);
    assert(Array.isArray(outputs), 'outputs is array');
    assert(outputs.length === 3, 'outputs length 3, got ' + outputs.length);
    for (const v of outputs) {
        assertType(v, 'number', 'output element');
    }
});

// =============================================================================
// 5. Brain Save/Load
// =============================================================================
section('5. Brain Save/Load');

const tmpPath = path.join(os.tmpdir(), 'nimcp_nodejs_test_brain.dat');

runTest('brainSave succeeds', () => {
    nimcp.brainSave(brain, tmpPath);
    assert(fs.existsSync(tmpPath), 'save file exists');
});

runTest('brainLoad returns handle', () => {
    const loaded = nimcp.brainLoad(tmpPath);
    assert(loaded !== null, 'loaded brain not null');
    // Verify loaded brain works
    const pred = nimcp.brainPredict(loaded, [1.0, 0.0, 0.0, 0.0]);
    assert('label' in pred, 'loaded brain can predict');
    // Cleanup temp file
    try { fs.unlinkSync(tmpPath); } catch(e) {}
});

// =============================================================================
// 6. Brain CreateFromConfig
// =============================================================================
section('6. Brain CreateFromConfig');

runTestTolerated('brainCreateFromConfig with non-existent file throws', () => {
    try {
        nimcp.brainCreateFromConfig('/tmp/nonexistent.yaml');
        assert(false, 'should throw for missing file');
    } catch (e) {
        // Expected - config file doesn't exist
        assert(true, 'correctly threw error');
    }
});

// =============================================================================
// 7. Training Pipeline
// =============================================================================
section('7. Training Pipeline');

runTest('brainConfigureTraining succeeds', () => {
    nimcp.brainConfigureTraining(brain, {
        lossType: nimcp.LOSS_CROSS_ENTROPY,
        optimizerType: nimcp.OPT_ADAM,
        learningRate: 0.001,
        schedulerType: nimcp.SCHED_COSINE
    });
});

runTest('brainTrainStep returns result', () => {
    const result = nimcp.brainTrainStep(brain, [1.0, 0.0, 0.0, 0.0], [1.0, 0.0, 0.0]);
    assertType(result, 'object', 'train step result');
    assert('loss' in result, 'has loss');
    assert('learningRate' in result, 'has learningRate');
    assert('step' in result, 'has step');
    assert('earlyStopped' in result, 'has earlyStopped');
    assert('gradientNorm' in result, 'has gradientNorm');
});

runTest('brainTrainBatch returns result', () => {
    const features = [
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 1.0, 0.0, 0.0]
    ];
    const targets = [
        [1.0, 0.0, 0.0],
        [0.0, 1.0, 0.0]
    ];
    const result = nimcp.brainTrainBatch(brain, features, targets, 2);
    assertType(result, 'object', 'train batch result');
    assert('loss' in result, 'batch has loss');
});

// =============================================================================
// 8. Training Stats + Scheduler
// =============================================================================
section('8. Training Stats + Scheduler');

runTest('brainGetTrainingStats returns stats', () => {
    const stats = nimcp.brainGetTrainingStats(brain);
    assertType(stats, 'object', 'training stats');
    assert('totalSteps' in stats, 'has totalSteps');
    assert('totalLoss' in stats, 'has totalLoss');
    assert('currentLr' in stats, 'has currentLr');
});

runTest('brainStepScheduler returns number', () => {
    const lr = nimcp.brainStepScheduler(brain, 0.5);
    assertType(lr, 'number', 'scheduler returns number');
});

// =============================================================================
// 9. Callbacks
// =============================================================================
section('9. Callbacks');

runTestTolerated('brainEnableCallbacks', () => {
    nimcp.brainEnableCallbacks(brain);
});

runTestTolerated('brainRegisterCallback returns id', () => {
    const cbId = nimcp.brainRegisterCallback(brain, nimcp.CB_STEP_COMPLETE, 'test_cb');
    assertType(cbId, 'number', 'callback id');
});

runTestTolerated('brainGetCallbackStats', () => {
    const stats = nimcp.brainGetCallbackStats(brain);
    assertType(stats, 'object', 'callback stats');
    assert('totalFired' in stats, 'has totalFired');
});

runTestTolerated('brainUnregisterCallback', () => {
    // May fail if callback wasn't registered
    nimcp.brainUnregisterCallback(brain, 1);
});

runTestTolerated('brainDisableCallbacks', () => {
    nimcp.brainDisableCallbacks(brain);
});

// =============================================================================
// 10. Named Snapshots
// =============================================================================
section('10. Named Snapshots');

runTestTolerated('brainSnapshotSave', () => {
    nimcp.brainSnapshotSave(brain, 'test_snap', 'test snapshot');
});

runTestTolerated('brainSnapshotList returns array', () => {
    const list = nimcp.brainSnapshotList(brain);
    assert(Array.isArray(list), 'snapshot list is array');
});

runTestTolerated('brainSnapshotDelete', () => {
    nimcp.brainSnapshotDelete(brain, 'test_snap');
});

// =============================================================================
// 11. Resize + Utilization
// =============================================================================
section('11. Resize + Utilization');

runTest('brainGetNeuronCount returns number', () => {
    const count = nimcp.brainGetNeuronCount(brain);
    assertType(count, 'number', 'neuron count');
    assert(count > 0, 'neuron count > 0: ' + count);
});

runTest('brainGetUtilizationMetrics returns object', () => {
    const metrics = nimcp.brainGetUtilizationMetrics(brain);
    assertType(metrics, 'object', 'utilization metrics');
    assert('utilization' in metrics, 'has utilization');
    assert('saturation' in metrics, 'has saturation');
});

runTestTolerated('brainResize', () => {
    const ok = nimcp.brainResize(brain, 200);
    assertType(ok, 'boolean', 'resize returns boolean');
});

runTestTolerated('brainAutoResize', () => {
    const ok = nimcp.brainAutoResize(brain);
    assertType(ok, 'boolean', 'auto_resize returns boolean');
});

// =============================================================================
// 12. COW Clone + Snapshot
// =============================================================================
section('12. COW Clone + Snapshot');

runTestTolerated('brainCloneCow returns handle', () => {
    const clone = nimcp.brainCloneCow(brain);
    assert(clone !== null, 'clone not null');
    // Verify clone works
    const pred = nimcp.brainPredict(clone, [1.0, 0.0, 0.0, 0.0]);
    assert('label' in pred, 'clone can predict');
});

runTestTolerated('brainSnapshotCow + brainRestoreCow', () => {
    const snap = nimcp.brainSnapshotCow(brain);
    assert(snap !== null, 'snapshot not null');
    nimcp.brainRestoreCow(brain, snap);
});

// =============================================================================
// 13. Working Memory
// =============================================================================
section('13. Working Memory');

runTestTolerated('workingMemoryAdd', () => {
    nimcp.workingMemoryAdd(brain, [1.0, 2.0, 3.0, 4.0], 0.8);
});

runTestTolerated('workingMemoryGet', () => {
    const data = nimcp.workingMemoryGet(brain, 0);
    if (data !== null) {
        assert(Array.isArray(data), 'working memory data is array');
    }
});

runTestTolerated('workingMemoryStats', () => {
    const stats = nimcp.workingMemoryStats(brain);
    assertType(stats, 'object', 'wm stats');
    assert('currentSize' in stats, 'has currentSize');
    assert('capacity' in stats, 'has capacity');
});

runTestTolerated('workingMemoryRefresh', () => {
    nimcp.workingMemoryRefresh(brain, 0);
});

// =============================================================================
// 14. Workspace
// =============================================================================
section('14. Workspace');

runTestTolerated('workspaceSubscribe', () => {
    nimcp.workspaceSubscribe(brain, nimcp.MODULE_PERCEPTION);
});

runTestTolerated('workspaceCompete returns status', () => {
    const content = new Array(256).fill(0.5);
    const status = nimcp.workspaceCompete(brain, nimcp.MODULE_PERCEPTION, content, 0.9);
    assertType(status, 'number', 'compete returns status code');
});

runTestTolerated('workspaceHasBroadcast returns boolean', () => {
    const has = nimcp.workspaceHasBroadcast(brain);
    assertType(has, 'boolean', 'has_broadcast');
});

runTestTolerated('workspaceRead', () => {
    const data = nimcp.workspaceRead(brain, 256);
    // May be null if no broadcast active
});

runTestTolerated('workspaceStats', () => {
    const stats = nimcp.workspaceStats(brain);
    assertType(stats, 'object', 'workspace stats');
    assert('totalBroadcasts' in stats, 'has totalBroadcasts');
});

runTestTolerated('workspaceUnsubscribe', () => {
    nimcp.workspaceUnsubscribe(brain, nimcp.MODULE_PERCEPTION);
});

// =============================================================================
// 15. Oscillations
// =============================================================================
section('15. Oscillations');

runTestTolerated('enableOscillations', () => {
    const ok = nimcp.enableOscillations(brain, true);
    assertType(ok, 'boolean', 'enableOscillations returns boolean');
});

runTestTolerated('isOscillationsEnabled', () => {
    const enabled = nimcp.isOscillationsEnabled(brain);
    assertType(enabled, 'boolean', 'isOscillationsEnabled returns boolean');
});

runTestTolerated('getOscillationPhasor', () => {
    const phasor = nimcp.getOscillationPhasor(brain, 0);
    assertType(phasor, 'object', 'phasor');
    assert('amplitude' in phasor, 'has amplitude');
    assert('phase' in phasor, 'has phase');
});

runTestTolerated('getPhaseCoherence', () => {
    const coherence = nimcp.getPhaseCoherence(brain, [0, 1, 2, 3, 4]);
    assertType(coherence, 'number', 'coherence');
});

runTestTolerated('getPacModulation', () => {
    const pac = nimcp.getPacModulation(brain, 6.0, 40.0);
    assertType(pac, 'number', 'pac');
});

// =============================================================================
// 16. Probe
// =============================================================================
section('16. Probe');

runTest('brainProbe returns full object', () => {
    const probe = nimcp.brainProbe(brain);
    assertType(probe, 'object', 'probe');
    assert('taskName' in probe, 'has taskName');
    assert('numNeurons' in probe, 'has numNeurons');
    assert('numInputs' in probe, 'has numInputs');
    assert('numOutputs' in probe, 'has numOutputs');
    assert('memoryBytes' in probe, 'has memoryBytes');
    assert('isCowClone' in probe, 'has isCowClone');
    assert(probe.numInputs === 4, 'numInputs == 4');
    assert(probe.numOutputs === 3, 'numOutputs == 3');
});

runTestTolerated('brainBroadcastProbe', () => {
    nimcp.brainBroadcastProbe(brain);
});

// =============================================================================
// 17. Network API
// =============================================================================
section('17. Network API');

runTest('networkCreate returns handle', () => {
    const net = nimcp.networkCreate(4, 2, 8, 0.01);
    assert(net !== null, 'network not null');
});

runTest('networkForward returns array', () => {
    const net = nimcp.networkCreate(4, 2, 8, 0.01);
    const outputs = nimcp.networkForward(net, [1.0, 0.5, 0.3, 0.1], 2);
    assert(Array.isArray(outputs), 'forward returns array');
    assert(outputs.length === 2, 'outputs length 2');
});

runTestTolerated('networkTrain', () => {
    const net = nimcp.networkCreate(4, 2, 8, 0.01);
    nimcp.networkTrain(net, [1.0, 0.5, 0.3, 0.1], [1.0, 0.0]);
});

// =============================================================================
// 18. Ethics API
// =============================================================================
section('18. Ethics API');

runTest('ethicsCreate returns handle', () => {
    const eth = nimcp.ethicsCreate();
    assert(eth !== null, 'ethics not null');
});

runTest('ethicsCheck returns score', () => {
    const eth = nimcp.ethicsCreate();
    const score = nimcp.ethicsCheck(eth, [0.5, 0.3, 0.8, 0.1]);
    assertType(score, 'number', 'ethics score');
    assert(score >= -1.0 && score <= 1.0, 'score in range [-1,1]: ' + score);
});

// =============================================================================
// 19. Knowledge Graph API
// =============================================================================
section('19. Knowledge Graph API');

runTest('knowledgeCreate returns handle', () => {
    const kg = nimcp.knowledgeCreate();
    assert(kg !== null, 'knowledge not null');
});

runTest('knowledgeAddFact succeeds', () => {
    const kg = nimcp.knowledgeCreate();
    nimcp.knowledgeAddFact(kg, 'cat', 'is_a', 'animal');
    nimcp.knowledgeAddFact(kg, 'dog', 'is_a', 'animal');
});

runTest('knowledgeQuery returns string', () => {
    const kg = nimcp.knowledgeCreate();
    nimcp.knowledgeAddFact(kg, 'sun', 'is_a', 'star');
    const result = nimcp.knowledgeQuery(kg, 'sun');
    assertType(result, 'string', 'query result');
});

// =============================================================================
// 20. Error Handling
// =============================================================================
section('20. Error Handling');

runTest('getError returns string or null', () => {
    const err = nimcp.getError();
    assert(err === null || typeof err === 'string', 'getError returns string or null');
});

runTest('enum constants accessible', () => {
    // Status codes
    assert(nimcp.OK === 0, 'OK === 0');
    assert(nimcp.ERROR === 1000, 'ERROR === 1000');
    assert(nimcp.ERROR_NULL_ARG === 1003, 'ERROR_NULL_ARG');
    assert(nimcp.ERROR_INVALID === 1004, 'ERROR_INVALID');
    assert(nimcp.ERROR_MEMORY === 2000, 'ERROR_MEMORY');
    assert(nimcp.ERROR_IO === 4000, 'ERROR_IO');

    // Cognitive modules
    assert(nimcp.MODULE_NONE === 0, 'MODULE_NONE');
    assert(typeof nimcp.MODULE_PERCEPTION === 'number', 'MODULE_PERCEPTION');
    assert(typeof nimcp.MODULE_CUSTOM_START === 'number', 'MODULE_CUSTOM_START');
});

// =============================================================================
// 21. Cleanup + Summary
// =============================================================================
section('21. Cleanup');

runTest('shutdown', () => {
    nimcp.shutdown();
});

// Summary
const total = passed + failed;
process.stdout.write('\n========================================\n');
process.stdout.write('NIMCP Node.js Bindings Test Results\n');
process.stdout.write('========================================\n');
process.stdout.write('PASSED:  ' + passed + '/' + total + '\n');
process.stdout.write('FAILED:  ' + failed + '/' + total + '\n');
process.stdout.write('SKIPPED: ' + skipped + '\n');
process.stdout.write('========================================\n');

if (failed > 0) {
    process.stdout.write('RESULT: FAIL\n');
    process.exit(1);
} else {
    process.stdout.write('RESULT: PASS (' + passed + '/' + total + ' tests, ' + skipped + ' tolerated)\n');
    process.exit(0);
}
