import api from './api';
import type { ScriptInfo, ScriptStatus } from '../types';

export const listScripts = () =>
  api.get<ScriptInfo[]>('/scripts/');

export const runScript = (scriptId: string, brainId: number) =>
  api.post<ScriptStatus>('/scripts/run', { script_id: scriptId, brain_id: brainId });

export const scriptStatus = () =>
  api.get<ScriptStatus>('/scripts/status');

export const stopScript = () =>
  api.post('/scripts/stop');
