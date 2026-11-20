import raw from '../../params.json';
import type { ParameterInfo } from 'ivandaw-core';

// params.json is the one parameter list. Array index = C ABI index = Engine::setParam index.
export const PARAMS = raw.params as ParameterInfo[];
export const INDEX = new Map(PARAMS.map((p, i) => [p.id, i]));

/** normalized 0..1 → plain min..max. Same function as the JUCE side. */
export function toPlain(p: ParameterInfo, n: number): number {
  const c = Math.min(1, Math.max(0, n));
  let v = p.scale === 'log' ? p.min * (p.max / p.min) ** c : p.min + c * (p.max - p.min);
  if (p.kind === 'int' || p.kind === 'choice' || p.kind === 'boolean') v = Math.round(v);
  return v;
}

export function toNormalized(p: ParameterInfo, v: number): number {
  const n = p.scale === 'log' ? Math.log(v / p.min) / Math.log(p.max / p.min) : (v - p.min) / (p.max - p.min);
  return Math.min(1, Math.max(0, n));
}

/** The label of a plain value, for UIs. */
export function formatValue(p: ParameterInfo, v: number): string {
  if (p.kind === 'choice' && p.choices) return p.choices[Math.round(v - p.min)] ?? String(v);
  if (p.kind === 'float') return v.toFixed(2);
  const text = String(Math.round(v));
  return p.unit ? `${text} ${p.unit}` : text;
}
