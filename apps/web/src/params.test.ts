import { describe, expect, it } from 'vitest';
import { INDEX, PARAMS, toNormalized, toPlain } from '../../../hosts/web/params';

// params.json is the one parameter list. The core checks it against its own tables in bc_tests.

describe('params.json', () => {
  it('has unique ids', () => {
    const ids = PARAMS.map((p) => p.id);
    expect(new Set(ids).size).toBe(ids.length);
  });

  it('has well-formed ranges, defaults, and choices', () => {
    for (const p of PARAMS) {
      expect(p.min, p.id).toBeLessThan(p.max);
      expect(p.default, p.id).toBeGreaterThanOrEqual(p.min);
      expect(p.default, p.id).toBeLessThanOrEqual(p.max);
      if (p.kind === 'choice') expect(p.choices?.length, p.id).toBe(p.max - p.min + 1);
      else expect(p.choices, p.id).toBeUndefined();
      expect(typeof p.automatable, p.id).toBe('boolean');
    }
  });

  it('keeps the indexes the engine uses (append only)', () => {
    const order = ['root', 'scale', 'loop', 'chordMin', 'chordMax', 'octaveMin', 'octaveMax', 'shuffle', 'drift', 'density', 'notesCount'];
    expect(PARAMS.slice(0, order.length).map((p) => p.id)).toEqual(order);
    expect(INDEX.get('notesCount')).toBe(10);
  });

  it('round-trips normalized values', () => {
    for (const p of PARAMS) {
      for (const n of [0, 0.25, 0.5, 1]) {
        const v = toPlain(p, n);
        expect(v, p.id).toBeGreaterThanOrEqual(p.min);
        expect(v, p.id).toBeLessThanOrEqual(p.max);
        expect(toPlain(p, toNormalized(p, v)), p.id).toBeCloseTo(v, 9);
      }
    }
  });
});
