import api from './api';
import type { TrainingConfig, TrainingProgress } from '../types';

export const configureTraining = (brainId: number, config: TrainingConfig) =>
  api.post(`/training/${brainId}/configure`, config);

export const startTraining = (brainId: number, datasetId: string, epochs: number, batchSize = 1) =>
  api.post<TrainingProgress>(`/training/${brainId}/start`, {
    dataset_id: datasetId, epochs, batch_size: batchSize,
  });

export const stopTraining = (brainId: number) =>
  api.post(`/training/${brainId}/stop`);

export const trainingStatus = (brainId: number) =>
  api.get<TrainingProgress>(`/training/${brainId}/status`);

export const learnBatch = (brainId: number, datasetId: string, count: number) =>
  api.post(`/training/${brainId}/learn-batch`, { dataset_id: datasetId, count });
