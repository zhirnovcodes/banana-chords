// Runs one engine on the main thread with a lookahead ticker and turns its output into PluginEvents.
// The clock and the ticker are injected, so Vitest drives it with a fake clock.

import type { PluginEvent, TransportState } from 'ivandaw-core';
import type { Core } from './core';

export type Ticker = { stop(): void };

export interface DriverDeps {
  core: Core;
  engine: number;
  sampleRate: number;
  /** AudioContext seconds. */
  now(): number;
  createTicker(ms: number, tick: () => void): Ticker;
  tickMs: number;
  /** Seconds rendered ahead of now. */
  lookahead: number;
  emit(e: PluginEvent): void;
}

export interface Driver {
  /** transport, tempo and midi events. Automation is the plugin's job. */
  schedule(events: PluginEvent[]): void;
  /** Beat at `time`, or null when stopped. From the last transport event. */
  beatAt(time: number): number | null;
  /** Note-offs for everything sounding, at `time` or just after the last note-on of that pitch. */
  allOff(time: number): void;
  suspend(): void;
  resume(): void;
  dispose(): void;
}

type Queued = { time: number; apply(frame: number, rendered: number): void };

// A note-off sent for a pitch whose note-on was already booked later than the off: just after the on.
const AFTER_ON = 0.001;

export function createDriver(d: DriverDeps): Driver {
  const { core, engine } = d;
  let rendered = d.now();
  let queue: Queued[] = [];
  let ticker: Ticker | null = null;
  let anchor: { time: number; state: TransportState } | null = null;
  /** The engine's play state, as of the end of the rendered window. */
  let playing = false;
  const lastOn = new Float64Array(128).fill(-Infinity);

  // Out events of the last engine call. Frame 0 is at `base`. Note-offs never go before the note-on of their pitch.
  const emitOut = (base: number) => {
    const count = core.bc_out_count(engine);
    for (let k = 0; k < count; k++) {
      if (core.bc_out_kind(engine, k) !== 0) continue;
      const b = core.bc_out_bytes(engine, k);
      const bytes: [number, number, number] = [b & 0xff, (b >> 8) & 0xff, (b >> 16) & 0xff];
      let time = base + core.bc_out_frame(engine, k) / d.sampleRate;
      const isOn = (bytes[0] & 0xf0) === 0x90 && bytes[2] > 0;
      if (isOn) lastOn[bytes[1]] = time;
      else time = Math.max(time, lastOn[bytes[1]] + AFTER_ON);
      d.emit({ type: 'midi', time, bytes, port: 'out' });
    }
  };

  const tick = () => {
    const now = d.now();
    const until = now + d.lookahead;
    if (rendered < now) rendered = now; // after a stall: skip, never burst
    // Stopped: render nothing ahead, so a Play booked in the next LOOKAHEAD lands on its exact frame.
    if (!playing) {
      rendered = now;
      if (!queue.some((q) => q.time < until)) return;
    }
    if (until <= rendered) return;
    const frames = Math.round((until - rendered) * d.sampleRate);
    if (frames <= 0) return;

    // Events due in this window, in time order. Late ones apply at frame 0.
    const end = rendered + frames / d.sampleRate;
    const due = queue.filter((q) => q.time < end).sort((a, b) => a.time - b.time);
    queue = queue.filter((q) => q.time >= end);
    for (const q of due) {
      const frame = Math.min(frames - 1, Math.max(0, Math.round((q.time - rendered) * d.sampleRate)));
      q.apply(frame, rendered);
    }
    core.bc_process(engine, frames, 0, 0);
    emitOut(rendered);
    rendered = end;
  };

  const onTransport = (time: number, state: TransportState) => {
    anchor = { time, state };
    queue.push({
      time,
      apply(frame, windowStart) {
        // The beat at the frame it is applied, which is later than `time` for a late event.
        const at = windowStart + frame / d.sampleRate;
        const beat = state.playing ? state.beat + ((at - time) * state.bpm) / 60 : state.beat;
        playing = state.playing;
        core.bc_transport(engine, state.playing ? 1 : 0, state.bpm, beat, state.numerator, state.denominator, frame);
      },
    });
    if (!state.playing) offNow(time);
  };

  // Stop and all-notes-off act at once: what sounds at the end of the rendered window stops at `time`
  // (or just after its note-on, when that was booked after `time`).
  const offNow = (time: number) => {
    core.bc_all_off(engine, 0);
    emitOut(Math.max(time, d.now()));
  };

  const start = () => {
    if (ticker === null) {
      ticker = d.createTicker(d.tickMs, tick);
      tick();
    }
  };
  start();

  return {
    schedule(events) {
      for (const e of events) {
        if (e.type === 'transport') {
          onTransport(e.time, e.state);
        } else if (e.type === 'tempo') {
          const base = anchor ?? { time: e.time, state: { playing: false, bpm: e.bpm, beat: 0, numerator: 4, denominator: 4, loop: null } };
          const beat = base.state.playing ? base.state.beat + ((e.time - base.time) * base.state.bpm) / 60 : base.state.beat;
          onTransport(e.time, { ...base.state, bpm: e.bpm, beat });
        } else if (e.type === 'midi') {
          const [status, d1] = e.bytes;
          if ((status & 0xf0) === 0xb0 && (d1 === 120 || d1 === 123)) offNow(e.time);
        }
      }
    },
    beatAt(time) {
      if (!anchor?.state.playing) return null;
      return anchor.state.beat + ((time - anchor.time) * anchor.state.bpm) / 60;
    },
    allOff(time) {
      offNow(time);
    },
    suspend() {
      ticker?.stop();
      ticker = null;
      offNow(d.now());
    },
    resume() {
      start();
    },
    dispose() {
      ticker?.stop();
      ticker = null;
      offNow(d.now());
      queue = [];
    },
  };
}
