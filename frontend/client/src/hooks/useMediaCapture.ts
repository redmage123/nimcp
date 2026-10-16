import { useState, useRef, useCallback } from 'react';

interface MediaCaptureState {
  micActive: boolean;
  camActive: boolean;
  startMic: () => Promise<void>;
  stopMic: () => void;
  startCam: () => Promise<void>;
  stopCam: () => void;
  captureAudioSamples: () => number[];
  captureFrame: () => { pixels: number[]; width: number; height: number } | null;
  videoRef: React.RefObject<HTMLVideoElement>;
}

export function useMediaCapture(): MediaCaptureState {
  const [micActive, setMicActive] = useState(false);
  const [camActive, setCamActive] = useState(false);
  const audioContextRef = useRef<AudioContext | null>(null);
  const audioStreamRef = useRef<MediaStream | null>(null);
  const analyserRef = useRef<AnalyserNode | null>(null);
  const videoStreamRef = useRef<MediaStream | null>(null);
  const videoRef = useRef<HTMLVideoElement>(null);
  const internalCanvasRef = useRef<HTMLCanvasElement | null>(null);

  const startMic = useCallback(async () => {
    try {
      const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
      audioStreamRef.current = stream;
      const ctx = new AudioContext();
      audioContextRef.current = ctx;
      const source = ctx.createMediaStreamSource(stream);
      const analyser = ctx.createAnalyser();
      analyser.fftSize = 2048;
      source.connect(analyser);
      analyserRef.current = analyser;
      setMicActive(true);
    } catch {
      console.warn('Microphone access denied');
    }
  }, []);

  const stopMic = useCallback(() => {
    audioStreamRef.current?.getTracks().forEach(t => t.stop());
    audioContextRef.current?.close();
    audioStreamRef.current = null;
    audioContextRef.current = null;
    analyserRef.current = null;
    setMicActive(false);
  }, []);

  const captureAudioSamples = useCallback((): number[] => {
    if (!analyserRef.current) return [];
    const analyser = analyserRef.current;
    const data = new Float32Array(analyser.fftSize);
    analyser.getFloatTimeDomainData(data);
    return Array.from(data);
  }, []);

  const startCam = useCallback(async () => {
    try {
      const stream = await navigator.mediaDevices.getUserMedia({
        video: { width: 160, height: 120 },
      });
      videoStreamRef.current = stream;
      if (videoRef.current) {
        videoRef.current.srcObject = stream;
        videoRef.current.play();
      }
      if (!internalCanvasRef.current) {
        internalCanvasRef.current = document.createElement('canvas');
        internalCanvasRef.current.width = 160;
        internalCanvasRef.current.height = 120;
      }
      setCamActive(true);
    } catch {
      console.warn('Camera access denied');
    }
  }, []);

  const stopCam = useCallback(() => {
    videoStreamRef.current?.getTracks().forEach(t => t.stop());
    videoStreamRef.current = null;
    if (videoRef.current) videoRef.current.srcObject = null;
    setCamActive(false);
  }, []);

  const captureFrame = useCallback((): { pixels: number[]; width: number; height: number } | null => {
    if (!videoRef.current || !internalCanvasRef.current || !camActive) return null;
    const canvas = internalCanvasRef.current;
    const ctx = canvas.getContext('2d');
    if (!ctx) return null;
    ctx.drawImage(videoRef.current, 0, 0, 160, 120);
    const imageData = ctx.getImageData(0, 0, 160, 120);
    // Convert RGBA to normalized RGB floats
    const pixels: number[] = [];
    for (let i = 0; i < imageData.data.length; i += 4) {
      pixels.push(imageData.data[i] / 255);     // R
      pixels.push(imageData.data[i + 1] / 255); // G
      pixels.push(imageData.data[i + 2] / 255); // B
    }
    return { pixels, width: 160, height: 120 };
  }, [camActive]);

  return {
    micActive, camActive,
    startMic, stopMic,
    startCam, stopCam,
    captureAudioSamples, captureFrame,
    videoRef,
  };
}
