import { PARAMS } from '../../../../../hosts/web/params';
import type { EngineData } from './engineData';

// Texts the screens show, in caps (8-bit Operator+ 8). Labels follow Figma Frame 4.

const choices = (id: string) => PARAMS.find((p) => p.id === id)?.choices ?? [];

export const NOTE_NAMES = choices('root').map((c) => c.toUpperCase());
export const SCALE_NAMES = choices('scale').map((c) => c.toUpperCase());

export const noteText = (data: EngineData) => NOTE_NAMES[data.main.root] ?? '';
export const scaleText = (data: EngineData) => SCALE_NAMES[data.main.scale] ?? '';
export const loopText = (data: EngineData) => `LOOP (${data.main.loop})`;
export const lengthText = (data: EngineData) => `LEN(${data.main.chordMin}-${data.main.chordMax})`;
export const notesText = (data: EngineData) => `NOTES(${data.additional.notesCount})`;
export const DRIFT_TEXT = 'DRIFT';
export const DENSITY_TEXT = 'DENSITY';
export const octaveText = (octave: number) => String(octave);
