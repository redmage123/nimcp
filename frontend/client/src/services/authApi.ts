import axios from 'axios';

const raw = axios.create({ baseURL: '/api/auth' });

export const login = (username: string, password: string) =>
  raw.post<{ ok: boolean; username: string; role: string }>('/login', { username, password });

export const register = (username: string, password: string) =>
  raw.post<{ ok: boolean; username: string; role: string }>('/register', { username, password });
