import { useRef, useEffect, useCallback } from 'react';
import type { AvatarState, AvatarIdentity } from '../../types';

const EMOTIONS = [
  'happy', 'sad', 'angry', 'afraid', 'disgusted', 'surprised',
  'interested', 'confused', 'frustrated', 'bored', 'proud',
  'ashamed', 'enraged', 'hateful', 'despairing', 'panicked',
  'calm', 'contemptuous', 'neutral',
];

interface Props {
  avatar: AvatarState | null;
  identity?: AvatarIdentity | null;
  width?: number;
  height?: number;
}

function hslToRgb(h: number, s: number, l: number): string {
  return `hsl(${h}, ${(s * 100).toFixed(0)}%, ${(l * 100).toFixed(0)}%)`;
}

function rgbStr(r: number, g: number, b: number): string {
  return `rgb(${Math.round(r * 255)}, ${Math.round(g * 255)}, ${Math.round(b * 255)})`;
}

export function AvatarFace({ avatar, identity, width = 220, height = 260 }: Props) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const stateRef = useRef<AvatarState | null>(avatar);
  const identityRef = useRef<AvatarIdentity | null | undefined>(identity);
  stateRef.current = avatar;
  identityRef.current = identity;

  const draw = useCallback(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const W = canvas.width;
    const H = canvas.height;
    const cx = W / 2;
    const cy = H / 2 - 10;
    ctx.clearRect(0, 0, W, H);

    const s = stateRef.current || {} as Partial<AvatarState>;
    const id = identityRef.current;
    const blink = s.blink || 0;
    const gazeX = (s.gaze_x || 0) * 30;
    const gazeY = (s.gaze_y || 0) * 30;
    const headYaw = (s.head_yaw || 0) * 0.5;

    // Identity-driven colors
    const skinColor = id
      ? hslToRgb(id.skin_hue, id.skin_saturation, id.skin_lightness)
      : '#f5d0a9';
    const skinStroke = id
      ? hslToRgb(id.skin_hue, id.skin_saturation, id.skin_lightness * 0.8)
      : '#d4a574';
    const eyeColor = id
      ? rgbStr(id.eye_color_r, id.eye_color_g, id.eye_color_b)
      : '#4a7ab5';
    const browColor = id
      ? rgbStr(id.brow_color_r, id.brow_color_g, id.brow_color_b)
      : '#8B6914';
    const lipColor = id
      ? rgbStr(id.lip_color_r, id.lip_color_g, id.lip_color_b)
      : '#cc7777';
    const lipDarkColor = id
      ? rgbStr(id.lip_color_r * 0.7, id.lip_color_g * 0.5, id.lip_color_b * 0.5)
      : '#8B2252';
    const hairColor = id
      ? rgbStr(id.hair_color_r, id.hair_color_g, id.hair_color_b)
      : '#3a2510';

    // Face proportions from identity
    const faceW = 75 * (id ? 0.85 + id.face_width * 0.3 : 1);
    const faceH = 92 * (id ? 0.9 + id.face_height * 0.2 : 1);
    const noseW = id ? 4 + id.nose_width * 6 : 6;
    const browThick = id ? 1.5 + id.brow_thickness * 2 : 2.5;
    const lipFull = id ? id.lip_fullness : 0.5;
    const cheekR = id ? id.cheek_roundness : 0.5;
    const chinR = id ? id.chin_shape : 0.5;
    const freckles = id ? id.freckles : 0;
    const hairLen = id ? id.hair_length : 0;

    // Face outline
    ctx.save();
    ctx.translate(cx + headYaw, cy);

    // Hair behind face (long hair)
    if (hairLen > 0.3) {
      const hairDrop = 40 + hairLen * 80;
      ctx.beginPath();
      ctx.ellipse(0, -10, faceW + 8, faceH + hairDrop * 0.4, 0, 0, Math.PI * 2);
      ctx.fillStyle = hairColor;
      ctx.fill();
    }

    // Face shape — blend ellipse based on cheek/chin
    ctx.beginPath();
    ctx.ellipse(0, chinR * 5, faceW, faceH, 0, 0, Math.PI * 2);
    ctx.fillStyle = skinColor;
    ctx.fill();
    ctx.strokeStyle = skinStroke;
    ctx.lineWidth = 1.5;
    ctx.stroke();

    // Freckles
    if (freckles > 0.1) {
      const freckleCount = Math.round(freckles * 30);
      ctx.fillStyle = hslToRgb(25, 0.5, (id?.skin_lightness || 0.72) * 0.7);
      for (let i = 0; i < freckleCount; i++) {
        // Deterministic pseudo-random positions on cheeks/nose
        const angle = (i * 137.508) * Math.PI / 180; // golden angle
        const dist = 15 + (i % 7) * 5;
        const fx = Math.cos(angle) * dist;
        const fy = -5 + Math.sin(angle) * dist * 0.6;
        if (Math.abs(fx) < faceW * 0.6 && fy > -25 && fy < 20) {
          ctx.beginPath();
          ctx.arc(fx, fy, 1.2, 0, Math.PI * 2);
          ctx.fill();
        }
      }
    }

    // Hair on top
    ctx.beginPath();
    ctx.ellipse(0, -faceH * 0.55, faceW + 5, 35, 0, Math.PI, Math.PI * 2);
    ctx.fillStyle = hairColor;
    ctx.fill();

    // Eyebrows
    const browRaise = (s.au1_inner_brow_raise || 0) + (s.au2_outer_brow_raise || 0);
    const browLower = s.au4_brow_lower || 0;
    const browY = -38 - browRaise * 12 + browLower * 8;
    ctx.strokeStyle = browColor;
    ctx.lineWidth = browThick;
    for (const side of [-1, 1]) {
      ctx.beginPath();
      ctx.moveTo(side * 42, browY);
      ctx.quadraticCurveTo(side * 25, browY - 4 - browRaise * 6, side * 8, browY + browLower * 3);
      ctx.stroke();
    }

    // Eyes
    const lidRaise = s.au5_upper_lid_raise || 0;
    const lidTighten = s.au7_lid_tighten || 0;
    const eyeOpenH = 10 * (1 - blink) * (1 + lidRaise * 0.5 - lidTighten * 0.3);
    const cheekRaise = s.au6_cheek_raise || 0;
    const eyeSquint = cheekRaise * 3;

    for (const side of [-1, 1]) {
      const ex = side * 25;
      const ey = -20 + eyeSquint;
      ctx.beginPath();
      ctx.ellipse(ex, ey, 13, Math.max(1, eyeOpenH), 0, 0, Math.PI * 2);
      ctx.fillStyle = '#fff';
      ctx.fill();
      ctx.strokeStyle = '#999';
      ctx.lineWidth = 0.8;
      ctx.stroke();
      if (eyeOpenH > 2) {
        const irisX = ex + gazeX * 0.3;
        const irisY = ey + gazeY * 0.2;
        ctx.beginPath();
        ctx.arc(irisX, irisY, 5, 0, Math.PI * 2);
        ctx.fillStyle = eyeColor;
        ctx.fill();
        ctx.beginPath();
        ctx.arc(irisX, irisY, 2.5, 0, Math.PI * 2);
        ctx.fillStyle = '#111';
        ctx.fill();
        // Eye highlight
        ctx.beginPath();
        ctx.arc(irisX + 1.5, irisY - 1.5, 1.2, 0, Math.PI * 2);
        ctx.fillStyle = 'rgba(255,255,255,0.6)';
        ctx.fill();
      }
    }

    // Nose
    const noseWrinkle = s.au9_nose_wrinkle || 0;
    ctx.beginPath();
    ctx.moveTo(0, -8);
    ctx.lineTo(-noseW - noseWrinkle * 2, 8);
    ctx.lineTo(noseW + noseWrinkle * 2, 8);
    ctx.strokeStyle = skinStroke;
    ctx.lineWidth = 1.2;
    ctx.stroke();

    // Mouth
    const smile = s.au12_lip_corner_pull || 0;
    const frown = s.au15_lip_corner_drop || 0;
    const jawDrop = s.au26_jaw_drop || 0;
    const lipsPart = s.au25_lips_part || 0;
    const mouthOpen = (s.mouth_open || 0) + jawDrop * 0.5;
    const lipRound = s.lip_round || 0;
    const lipStretch = s.au20_lip_stretch || 0;
    const mouthW = 24 + lipStretch * 12 - lipRound * 8;
    const mouthH = Math.max(2, mouthOpen * 20 + lipsPart * 8 + jawDrop * 12 + lipFull * 4);
    const cornerCurve = (smile - frown) * 12;
    const my = 30;

    ctx.beginPath();
    ctx.moveTo(-mouthW, my);
    ctx.quadraticCurveTo(-mouthW * 0.5, my - cornerCurve, 0, my - mouthH * 0.3);
    ctx.quadraticCurveTo(mouthW * 0.5, my - cornerCurve, mouthW, my);
    if (mouthH > 3) {
      ctx.quadraticCurveTo(mouthW * 0.5, my + mouthH, 0, my + mouthH);
      ctx.quadraticCurveTo(-mouthW * 0.5, my + mouthH, -mouthW, my);
    }
    ctx.fillStyle = mouthH > 5 ? lipDarkColor : lipColor;
    ctx.fill();
    ctx.strokeStyle = lipDarkColor;
    ctx.lineWidth = 1.2;
    ctx.stroke();

    if (mouthH > 8) {
      ctx.beginPath();
      ctx.rect(-mouthW * 0.6, my + 1, mouthW * 1.2, Math.min(5, mouthH * 0.3));
      ctx.fillStyle = '#f0f0f0';
      ctx.fill();
    }

    ctx.restore();

    requestAnimationFrame(draw);
  }, []);

  useEffect(() => {
    const id = requestAnimationFrame(draw);
    return () => cancelAnimationFrame(id);
  }, [draw]);

  const emotionName = avatar && avatar.emotion_id < EMOTIONS.length
    ? EMOTIONS[avatar.emotion_id] : 'neutral';
  const emotionPct = avatar?.emotion_intensity
    ? `${(avatar.emotion_intensity * 100).toFixed(0)}%` : '';

  return (
    <div className="avatar-face-panel">
      <canvas
        ref={canvasRef}
        width={width}
        height={height}
        className="avatar-face-canvas"
      />
      <div className="avatar-face-emotion">
        {emotionName}{emotionPct ? ` (${emotionPct})` : ''}
      </div>
      {avatar && (
        <div className="avatar-face-stats">
          <span>V: {avatar.valence?.toFixed(2) ?? '--'}</span>
          <span>A: {avatar.arousal?.toFixed(2) ?? '--'}</span>
          <span>Viseme: {avatar.current_viseme ?? 0}</span>
          {avatar.is_speaking && <span className="avatar-speaking-dot" />}
        </div>
      )}
    </div>
  );
}
