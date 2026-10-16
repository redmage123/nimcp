import api from './api';
import type { DatasetInfo } from '../types';

export const listDatasets = () =>
  api.get<DatasetInfo[]>('/datasets/');

export const getDataset = (id: string, count = 30) =>
  api.get(`/datasets/${id}`, { params: { count } });

export const uploadCSV = (file: File, name: string, labelColumn = 'label') => {
  const form = new FormData();
  form.append('file', file);
  form.append('name', name);
  form.append('label_column', labelColumn);
  return api.post<{ id: string; name: string }>('/datasets/upload', form);
};

export const deleteDataset = (id: string) =>
  api.delete(`/datasets/${id}`);

export const deleteDatasetsBatch = (ids: string[]) =>
  api.delete<{ deleted: string[]; not_found: string[]; protected: string[] }>(
    '/datasets/batch', { data: { ids } }
  );
