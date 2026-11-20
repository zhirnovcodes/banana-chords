import { INDEX, PARAMS, toNormalized, toPlain } from '../../../../../hosts/web/params';
import { type EngineData, PARAM_PATHS, readPlain, writePlain } from '../data/engineData';
import type { EngineDataModel } from '../models/engineDataModel';
import type { Edit } from '../views/edit';

// One-parameter reads and writes for presenters, done through the model's get/set[Group]Data.

const param = (id: string) => PARAMS[INDEX.get(id)!];

/** Plain value (params.json min..max). */
export function readParam(model: EngineDataModel, id: string): number {
  return readPlain(model.getData(), id);
}

export function writeParam(model: EngineDataModel, id: string, plain: number): void {
  const data: EngineData = model.getData();
  writePlain(data, id, plain);
  if (PARAM_PATHS[id][0] === 'main') model.setMainData(data.main);
  else model.setAdditionalData(data.additional);
}

/** Control position 0..1 for `id`. */
export function readPosition(model: EngineDataModel, id: string): number {
  return toNormalized(param(id), readParam(model, id));
}

/** A slider edit on `id`: gesture begin, value, gesture end. */
export function applyEdit(model: EngineDataModel, id: string, edit: Edit): void {
  const index = INDEX.get(id)!;
  if (edit.gesture === 'begin') model.beginGesture(index);
  writeParam(model, id, toPlain(param(id), edit.value));
  if (edit.gesture === 'end') model.endGesture(index);
}

/** Steps a choice / int parameter by `delta`: wraps (`wrap`) or stops at the ends. */
export function stepParam(model: EngineDataModel, id: string, delta: number, wrap = false): void {
  const p = param(id);
  const count = p.max - p.min + 1;
  const current = readParam(model, id);
  const next = wrap
    ? p.min + ((((current - p.min + delta) % count) + count) % count)
    : Math.min(p.max, Math.max(p.min, current + delta));
  writeParam(model, id, next);
}
