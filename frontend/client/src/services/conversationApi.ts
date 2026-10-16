import api from './api';
import type { Conversation, ConversationDetail } from '../types';

export function listConversations() {
  return api.get<Conversation[]>('/conversations/');
}

export function createConversation(brainId: number = 0, title: string = 'New conversation') {
  return api.post<{ id: string; brain_id: number; title: string }>('/conversations/', {
    brain_id: brainId,
    title,
  });
}

export function getConversation(convId: string) {
  return api.get<ConversationDetail>(`/conversations/${convId}`);
}

export function deleteConversation(convId: string) {
  return api.delete(`/conversations/${convId}`);
}

export function renameConversation(convId: string, title: string) {
  return api.patch(`/conversations/${convId}`, { title });
}
