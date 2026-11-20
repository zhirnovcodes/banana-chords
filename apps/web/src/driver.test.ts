import fs from 'node:fs';
import type { PluginEvent, TransportState } from 'ivandaw-core';
import { beforeAll, describe, expect, it } from 'vitest';
import { type Core, instantiateCore } from '../../../hosts/web/core';
import { createDriver } from '../../../hosts/web/driver';
import { wasmCommand, wasmPath } from '../vite.shared.ts';

// The real engine (build-wasm/banana-chords.wasm) under a fake clock and a fake ticker.

const RATE = 48000;
let core: Core;

beforeAll(async () => {
  if (!fs.existsSync(wasmPath)) throw new Error(`build-wasm/banana-chords.wasm is missing. From the repo root:\n  ${wasmCommand}`);
  core = await instantiateCore(fs.readFileSync(wasmPath));
});

function rig(seed = 1) {
  let now = 0;
  let tick: (() => void) | null = null;
  const events: Extract<PluginEvent, { type: 'midi' }>[] = [];
  const engine = core.bc_create(RATE, seed);
  const driver = createDriver({
    core,
    engine,
    sampleRate: RATE,
    now: () => now,
    createTicker(_ms, fn) {
      tick = fn;
      return { stop: () => (tick = null) };
    },
    tickMs: 25,
    lookahead: 0.1,
    emit: (e) => {
      if (e.type === 'midi') events.push(e);
    },
  });
  return {
    engine,
    driver,
    events,
    /** Advance the clock in 25 ms ticks. */
    run(seconds: number) {
      const end = now + seconds;
      while (now < end - 1e-9) {
        now += 0.025;
        tick?.();
      }
    },
    get now() {
      return now;
    },
    get ticking() {
      return tick !== null;
    },
  };
}

const state = (playing: boolean, beat: number, bpm = 120): TransportState => ({
  playing,
  bpm,
  beat,
  numerator: 4,
  denominator: 4,
  loop: null,
});

function checkBalanced(events: Extract<PluginEvent, { type: 'midi' }>[]) {
  // Every on gets an off at or after it; none open at the end.
  const sorted = [...events].sort((a, b) => a.time - b.time);
  const open = new Map<number, number>();
  for (const e of sorted) {
    const [status, pitch, velocity] = e.bytes;
    if ((status & 0xf0) === 0x90 && velocity > 0) {
      expect(open.has(pitch), `pitch ${pitch} started twice`).toBe(false);
      open.set(pitch, e.time);
    } else {
      expect(open.has(pitch), `off without on for ${pitch}`).toBe(true);
      open.delete(pitch);
    }
  }
  expect([...open.keys()]).toEqual([]);
}

describe('driver', () => {
  it('plays chords on the beat grid and stops cleanly', () => {
    const r = rig(3);
    r.driver.schedule([{ type: 'transport', time: 0.05, state: state(true, 0) }]);
    r.run(6);
    const ons = r.events.filter((e) => (e.bytes[0] & 0xf0) === 0x90);
    expect(ons.length).toBeGreaterThan(0);
    expect(ons[0].time).toBeCloseTo(0.05, 4);
    for (const e of r.events) {
      // 120 BPM: a 1/4 beat grid every 0.125 s from 0.05.
      const q = (e.time - 0.05) / 0.125;
      expect(Math.abs(q - Math.round(q)) * 0.125).toBeLessThan(1 / RATE + 1e-9);
    }
    const stopAt = r.now;
    r.driver.schedule([{ type: 'transport', time: stopAt, state: state(false, 0) }]);
    r.run(1);
    checkBalanced(r.events);
    // Nothing starts after the stop.
    const late = r.events.filter((e) => (e.bytes[0] & 0xf0) === 0x90 && e.time > stopAt + 0.1);
    expect(late).toEqual([]);
  });

  it('releases every note on CC 123 and on dispose', () => {
    const r = rig(4);
    r.driver.schedule([{ type: 'transport', time: 0, state: state(true, 0) }]);
    r.run(1.3);
    r.driver.schedule([{ type: 'midi', time: r.now, bytes: [0xb0, 123, 0] }]);
    const afterCc = r.events.length;
    expect(afterCc).toBeGreaterThan(0);
    r.run(0.5);
    r.driver.dispose();
    expect(r.ticking).toBe(false);
    checkBalanced(r.events);
  });

  it('follows a late transport event without a burst', () => {
    const r = rig(5);
    r.run(1);
    // Play that says beat 0 was 0.3 s ago: the next notes still land on the grid of that anchor.
    r.driver.schedule([{ type: 'transport', time: r.now - 0.3, state: state(true, 0) }]);
    const anchor = r.now - 0.3;
    r.run(3);
    // The first chord starts at once (late); every change after it is on the grid.
    const ons = r.events.filter((x) => (x.bytes[0] & 0xf0) === 0x90);
    expect(ons.length).toBeGreaterThan(0);
    for (const e of ons.filter((x) => x.time > ons[0].time + 1e-9)) {
      const q = (e.time - anchor) / 0.125;
      expect(Math.abs(q - Math.round(q)) * 0.125).toBeLessThan(2 / RATE);
    }
    r.driver.dispose();
    checkBalanced(r.events);
  });

  it('reports the beat for the playhead', () => {
    const r = rig();
    expect(r.driver.beatAt(0)).toBeNull();
    r.driver.schedule([{ type: 'transport', time: 1, state: state(true, 4, 60) }]);
    expect(r.driver.beatAt(3)).toBeCloseTo(6, 9);
    r.driver.dispose();
  });
});
