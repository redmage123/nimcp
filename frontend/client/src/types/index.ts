export interface BrainProbe {
  task_name: string;
  size: number;
  task: number;
  num_neurons: number;
  num_synapses: number;
  num_active_synapses: number;
  total_inferences: number;
  total_learning_steps: number;
  avg_sparsity: number;
  avg_inference_time_us: number;
  current_learning_rate: number;
  accuracy: number;
  memory_bytes: number;
  num_inputs: number;
  num_outputs: number;
  utilization: number;
  last_loss: number;
  is_cow_clone: boolean;
  cow_ref_count: number;
  cow_shared_bytes: number;
  cow_private_bytes: number;
}

export interface BrainInfo {
  id: number;
  name: string;
  created_at: string;
  dataset: string | null;
  parent_id: number | null;
  probe: BrainProbe | null;
}

export interface BrainDetail {
  id: number;
  name: string;
  created_at: string;
  size: number;
  size_label: string;
  task: number;
  task_label: string;
  num_inputs: number;
  num_outputs: number;
  dataset: string | null;
  parent_id: number | null;
  probe: BrainProbe | null;
  total_inferences: number;
  total_learning_steps: number;
  accuracy: number;
  last_loss: number;
  loss_history: number[];
}

export interface BrainCreate {
  name: string;
  size: number;
  task?: number;
  num_inputs?: number;
  num_outputs?: number;
  num_neurons?: number;
}

export interface CognitiveState {
  confidence: number;
  label: string;
  utilization: number;
  sparsity: number;
  neuron_count: number;
  total_inferences: number;
  total_learning_steps: number;
  explanation: string;
  output_vector: number[];
  num_active_neurons: number;
  inference_time_us: number;
}

export interface TrainingConfig {
  loss_type: number;
  optimizer_type: number;
  scheduler_type: number;
  learning_rate: number;
  weight_decay: number;
  momentum: number;
  beta1: number;
  beta2: number;
  epsilon: number;
  scheduler_step_size: number;
  scheduler_gamma: number;
  warmup_steps: number;
  enable_gradient_clipping: boolean;
  gradient_clip_value: number;
  enable_biological_modulation: boolean;
  biological_blend: number;
}

export interface TrainingProgress {
  brain_id: number;
  running: boolean;
  epoch: number;
  step: number;
  total_steps: number;
  loss: number;
  accuracy: number;
  learning_rate: number;
  elapsed_seconds: number;
}

export interface DatasetInfo {
  id: string;
  name: string;
  num_inputs: number;
  num_outputs: number;
  num_classes: number;
  description: string;
  total_examples: number;
  is_builtin: boolean;
}

export interface ChatMessage {
  id: number;
  sender: 'user' | 'brain';
  text: string;
  label?: string | number;
  confidence?: number;
  time_ms?: number;
  mode?: string;
  explanation?: string;
  sparsity?: number;
  num_active_neurons?: number;
  inference_time_us?: number;
  output_vector?: number[];
  cognitive_state?: Partial<CognitiveState>;
  // Speech & Avatar
  spoken_text?: string;
  speech_confidence?: number;
  speech_fluency?: number;
  avatar?: AvatarState;
}

export interface WSMessage {
  type: string;
  [key: string]: unknown;
}

export interface ScriptInfo {
  id: string;
  name: string;
  description: string;
  exists: boolean;
}

export interface ScriptStatus {
  script_id?: string;
  brain_id?: number;
  status: string;
  exit_code?: number | null;
  stdout_lines?: string[];
  total_lines?: number;
}

export type Tab = 'dashboard' | 'training' | 'chat' | 'datasets' | 'benchmarks';

export interface AuthState {
  username: string;
  role: 'admin' | 'user';
}

export interface Conversation {
  id: string;
  title: string;
  brain_id: number;
  created_at: string;
  updated_at: string;
  message_count: number;
}

export interface ConversationMessage {
  role: 'user' | 'brain';
  text: string;
  timestamp: string;
  metadata?: Record<string, unknown>;
}

export interface ConversationDetail {
  id: string;
  title: string;
  brain_id: number;
  created_at: string;
  updated_at: string;
  messages: ConversationMessage[];
}

export type AppView = 'chat' | 'dashboard' | 'training' | 'probes' | 'monitor';

export interface ProbeConfig {
  id: string;
  name: string;
  brain_id: number;
  metrics: string[];
  refresh_ms: number;
  chart_type: 'line' | 'bar' | 'gauge';
  alert_thresholds?: Record<string, number>;
}

export interface CognitiveMetrics {
  working_memory_capacity: number;
  working_memory_occupancy: number;
  oscillation_coherence: number;
  pac_index: number;
  workspace_broadcasts: number;
  workspace_avg_strength: number;
  ethics_separation: number;
  ethics_harmful_score: number;
  ethics_beneficial_score: number;
  knowledge_concepts: number;
  knowledge_coverage: number;
}

export interface BenchmarkResult {
  benchmark_id: string;
  category: string;
  accuracy: number;
  reference_scores: Record<string, number>;
  sparsity: number;
  efficiency: number;
  cognitive: CognitiveMetrics | null;
  train_time_ms: number;
  infer_time_ms: number;
  train_time_seconds: number;
  inference_time_us: number;
  active_neuron_ratio: number;
}

export interface BenchmarkSummary {
  overall_ml_accuracy: number;
  overall_genai_accuracy: number;
  cognitive_health_score: number;
  results: BenchmarkResult[];
  timestamp: string;
}

export interface BenchmarkInfo {
  id: string;
  name: string;
  category: string;
  description: string;
}

export interface AvatarState {
  // Mouth / viseme
  mouth_open: number;
  lip_round: number;
  lip_upper: number;
  lip_lower: number;
  tongue_position: number;
  current_viseme: number;
  // FACS Action Units
  au1_inner_brow_raise: number;
  au2_outer_brow_raise: number;
  au4_brow_lower: number;
  au5_upper_lid_raise: number;
  au6_cheek_raise: number;
  au7_lid_tighten: number;
  au9_nose_wrinkle: number;
  au10_upper_lip_raise: number;
  au12_lip_corner_pull: number;
  au15_lip_corner_drop: number;
  au17_chin_raise: number;
  au20_lip_stretch: number;
  au23_lip_tighten: number;
  au25_lips_part: number;
  au26_jaw_drop: number;
  au28_lip_suck: number;
  // Emotion
  valence: number;
  arousal: number;
  dominance: number;
  emotion_id: number;
  emotion_intensity: number;
  // Gaze & head
  gaze_x: number;
  gaze_y: number;
  head_pitch: number;
  head_yaw: number;
  head_roll: number;
  blink: number;
  // Voice
  pitch_hz: number;
  speaking_rate: number;
  volume: number;
  // Metadata
  timestamp_us: number;
  is_speaking: boolean;
}

export interface SpeechResult {
  text: string;
  confidence: number;
  fluency: number;
}

export interface AvatarIdentity {
  skin_hue: number;
  skin_saturation: number;
  skin_lightness: number;
  eye_color_r: number;
  eye_color_g: number;
  eye_color_b: number;
  hair_color_r: number;
  hair_color_g: number;
  hair_color_b: number;
  hair_length: number;
  hair_style: string;
  face_width: number;
  face_height: number;
  lip_fullness: number;
  lip_color_r: number;
  lip_color_g: number;
  lip_color_b: number;
  nose_width: number;
  brow_thickness: number;
  brow_color_r: number;
  brow_color_g: number;
  brow_color_b: number;
  cheek_roundness: number;
  chin_shape: number;
  freckles: number;
  voice_pitch: number;
  voice_warmth: number;
}

export interface AthenaStatus {
  loaded: boolean;
  name?: string;
  num_neurons?: number;
  total_inferences?: number;
  total_learning_steps?: number;
  accuracy?: number;
  memory_bytes?: number;
}

/** Full probe data returned by GET /api/admin/probes/live */
export interface ProbeData {
  // Core brain metrics
  num_neurons: number;
  num_synapses: number;
  accuracy: number;
  last_loss: number;
  last_gradient_norm: number;
  gpu_available: boolean;
  total_learning_steps: number;
  num_inputs: number;
  num_outputs: number;

  // Training dynamics
  weight_l2_norm: number;
  weight_mean_abs: number;
  weight_max_abs: number;
  weight_sampled_synapses: number;
  ema_gradient_norm: number;
  ema_loss: number;
  layer_grad_norms: number[];

  // Learning quality
  mean_label_accuracy: number;
  worst_label_accuracy: number;
  num_labels_tracked: number;
  confidence_calibration: number;
  learning_velocity: number;
  prediction_entropy: number;
  synapse_growth: number;

  // Brain health
  memory_rss_bytes: number;
  gpu_vram_bytes: number;
  neuron_utilization: number;
  immune_total_exceptions: number;
  immune_inflammation: number;

  // Conversational readiness
  vocabulary_size: number;
  response_diversity: number;

  // Extra fields from existing probe
  utilization?: number;
  avg_inference_time_us?: number;
  total_inferences?: number;
  num_active_synapses?: number;
  avg_sparsity?: number;
  current_learning_rate?: number;
  memory_bytes?: number;

  // Language & Emotion (from avatar state)
  emotion_valence?: number;
  emotion_arousal?: number;
  emotion_id?: number;
  emotion_intensity?: number;
  is_speaking?: boolean;
  speech_pitch_hz?: number;

  // Allow extra keys from C probe
  [key: string]: unknown;
}
