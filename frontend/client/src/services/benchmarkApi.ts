import api from './api';
import type { BenchmarkInfo, BenchmarkSummary } from '../types';

export interface BenchmarkRequest {
  benchmark_id: string;
  brain_size?: number;
  strategy?: string;
  epochs?: number;
  include_cognitive?: boolean;
}

export const listBenchmarks = () => api.get<BenchmarkInfo[]>('/benchmarks/available');

export const runBenchmark = (req: BenchmarkRequest) =>
  api.post('/benchmarks/run', req);

export const getBenchmarkStatus = () => api.get<{ running: boolean; current_benchmark?: string }>('/benchmarks/status');

export const getBenchmarkResults = () => api.get<BenchmarkSummary>('/benchmarks/results');

export const stopBenchmark = () => api.post('/benchmarks/stop');
