import api from './api';

export function processAudio(brainId: number, samples: number[]) {
  return api.post<{ features: number[] }>('/media/process-audio', {
    brain_id: brainId,
    samples,
  });
}

export function processVideo(brainId: number, pixels: number[], width: number, height: number, channels: number = 3) {
  return api.post<{ features: number[] }>('/media/process-video', {
    brain_id: brainId,
    pixels,
    width,
    height,
    channels,
  });
}

export function composeFeatures(textFeatures: number[], audioFeatures: number[], visualFeatures: number[]) {
  return api.post<{ features: number[] }>('/media/compose', {
    text_features: textFeatures,
    audio_features: audioFeatures,
    visual_features: visualFeatures,
  });
}
