import { PARAMS } from '../../../../../hosts/web/params';
import type { EngineData } from './engineData';

// Texts the screens show, in caps (8-bit Operator+ 8). Labels follow Figma Frame 4.

const choices = (id: string) => PARAMS.find((p) => p.id === id)?.choices ?? [];

export const NOTE_NAMES = choices('root').map((c) => c.toUpperCase());

/** The SCALE list fits 12 characters (7.3 px each), so long names are shortened on screen. */
export const SCALE_TEXT_LENGTH = 12;
const SHORT: Record<string, string> = {
  'Natural Minor': 'NAT. MINOR',
  'Harmonic Minor': 'HARM. MINOR',
  'Melodic Minor': 'MEL. MINOR',
  'Phrygian Dominant': 'PHRYG. DOM.',
  'Hungarian Minor': 'HUNG. MINOR',
  'Double Harmonic': 'DOUBLE HARM.',
  'Major Pentatonic': 'MAJ. PENTA',
  'Minor Pentatonic': 'MIN. PENTA',
};
export const SCALE_NAMES = choices('scale').map((c) => SHORT[c] ?? c.toUpperCase());

export const noteText = (data: EngineData) => NOTE_NAMES[data.main.root] ?? '';
export const scaleText = (data: EngineData) => SCALE_NAMES[data.main.scale] ?? '';
export const loopText = (data: EngineData) => `LOOP(${data.main.loop})`;
export const lengthText = (data: EngineData) => `LEN(${data.main.chordMin}-${data.main.chordMax})`;
export const notesText = (data: EngineData) => String(data.additional.notesCount);
export const octaveText = (octave: number) => String(octave);

/** 12 entries, C..B, from the core's scale mask (bit k = pitch class k). */
export const maskToNotes = (mask: number) => Array.from({ length: 12 }, (_, k) => ((mask >> k) & 1) === 1);
