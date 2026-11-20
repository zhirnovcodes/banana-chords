import { PARAMS } from '../../../../../hosts/web/params';

// 1 - Engine data: the value of every parameter in plain units (params.json min..max),
// nested by params.json "group". Plain data, no behaviour: EngineDataModel owns reading and writing it.

/** Group "Main". */
export interface MainData {
  root: number; // 0..11, C..B
  scale: number; // index into the scale choices
  loop: number; // 1..32 beats
  chordMin: number; // 1..32 beats
  chordMax: number; // 1..32 beats
  octaveMin: number; // 0..6
  octaveMax: number; // 0..6
}

/** Group "Additional". */
export interface AdditionalData {
  drift: number; // 0..1
  density: number; // 0..1
  notesCount: number; // 1..6
}

export interface EngineData {
  main: MainData;
  additional: AdditionalData;
}

export type GroupKey = keyof EngineData;

/** Where each parameter lives in EngineData. `shuffle` is a trigger, not data: EngineDataModel.shuffle(). */
export const PARAM_PATHS: Record<string, readonly [GroupKey, string]> = {
  root: ['main', 'root'],
  scale: ['main', 'scale'],
  loop: ['main', 'loop'],
  chordMin: ['main', 'chordMin'],
  chordMax: ['main', 'chordMax'],
  octaveMin: ['main', 'octaveMin'],
  octaveMax: ['main', 'octaveMax'],
  drift: ['additional', 'drift'],
  density: ['additional', 'density'],
  notesCount: ['additional', 'notesCount'],
};

/** Every parameter that is data (all but triggers), in params.json order. */
export const DATA_PARAMS = PARAMS.filter((p) => p.kind !== 'trigger');

export function readPlain(data: EngineData, id: string): number {
  const [group, key] = PARAM_PATHS[id];
  return (data[group] as unknown as Record<string, number>)[key];
}

export function writePlain(data: EngineData, id: string, plain: number): void {
  const [group, key] = PARAM_PATHS[id];
  (data[group] as unknown as Record<string, number>)[key] = plain;
}

export function emptyEngineData(): EngineData {
  return {
    main: { root: 0, scale: 0, loop: 0, chordMin: 0, chordMax: 0, octaveMin: 0, octaveMax: 0 },
    additional: { drift: 0, density: 0, notesCount: 0 },
  };
}
