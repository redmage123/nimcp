import api from './api';
import type { BrainCreate, BrainInfo, BrainDetail, BrainProbe, AthenaStatus, ProbeConfig, ProbeData, AvatarState, SpeechResult, AvatarIdentity } from '../types';

export const createBrain = (data: BrainCreate) =>
  api.post<{ id: number; probe: BrainProbe }>('/brains/', data);

export const listBrains = () =>
  api.get<BrainInfo[]>('/brains/');

export const getBrain = (id: number) =>
  api.get<BrainInfo>(`/brains/${id}`);

export const getBrainDetail = (id: number) =>
  api.get<BrainDetail>(`/brains/${id}`);

export const renameBrain = (id: number, name: string) =>
  api.patch(`/brains/${id}`, { name });

export const deleteBrain = (id: number) =>
  api.delete(`/brains/${id}`);

export const resizeBrain = (id: number, num_neurons: number) =>
  api.post<{ id: number; num_neurons: number; probe: BrainProbe }>(`/brains/${id}/resize`, { num_neurons });

export const probeBrain = (id: number) =>
  api.get<BrainProbe>(`/brains/${id}/probe`);

export const probeHistory = (id: number) =>
  api.get<(BrainProbe & { timestamp: number })[]>(`/brains/${id}/probe/history`);

export const predict = (id: number, features: number[]) =>
  api.post<{ label: string; confidence: number }>(`/brains/${id}/predict`, { features });

export const learn = (id: number, features: number[], label: string | number, confidence = 1.0) =>
  api.post(`/brains/${id}/learn`, { features, label, confidence });

export const createSnapshot = (id: number, name: string) =>
  api.post(`/brains/${id}/snapshots`, { name });

export const listSnapshots = (id: number) =>
  api.get<{ name: string; path: string }[]>(`/brains/${id}/snapshots`);

export const restoreSnapshot = (id: number, name: string) =>
  api.post(`/brains/${id}/snapshots/${name}/restore`);

export const deleteSnapshot = (id: number, name: string) =>
  api.delete(`/brains/${id}/snapshots/${name}`);

export const cowSnapshot = (id: number) =>
  api.post(`/brains/${id}/cow/snapshot`);

export const cowRestore = (id: number) =>
  api.post(`/brains/${id}/cow/restore`);

// Admin endpoints
export const getAthenaStatus = () =>
  api.get<AthenaStatus>('/admin/athena/status');

export const saveAthena = () =>
  api.post('/admin/athena/save');

export const listUsers = () =>
  api.get<{ username: string; role: string }[]>('/admin/users');

export const updateUserRole = (username: string, role: string) =>
  api.patch(`/admin/users/${username}`, { role });

export const listProbeConfigs = () =>
  api.get<ProbeConfig[]>('/admin/probes');

export const saveProbeConfig = (config: Partial<ProbeConfig>) =>
  api.post<ProbeConfig>('/admin/probes', config);

export const deleteProbeConfig = (probeId: string) =>
  api.delete(`/admin/probes/${probeId}`);

export const getAuthMe = () =>
  api.get<{ username: string; role: string }>('/auth/me');

/** Fetch live probe data for real-time monitoring. Defaults to Athena (brain_id=0). */
export const fetchLiveProbeData = (brainId: number = 0) =>
  api.get<ProbeData>(`/admin/probes/live`, { params: { brain_id: brainId } });

// Speech & Avatar
export const speak = (id: number, semanticVector?: number[]) =>
  api.post<SpeechResult>(`/brains/${id}/speak`, { semantic_vector: semanticVector });

export const getAvatarState = (id: number) =>
  api.get<AvatarState>(`/brains/${id}/avatar`);

// Identity / Self-Image
export const getIdentity = (id: number) =>
  api.get<AvatarIdentity>(`/brains/${id}/identity`);

export const setIdentity = (id: number, identity: Partial<AvatarIdentity>) =>
  api.put<AvatarIdentity>(`/brains/${id}/identity`, identity);
