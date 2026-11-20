import { describe, expect, it } from 'vitest';
import { PARAMS, toNormalized, toPlain } from '../../../../hosts/web/params';
import { createUi, DEVICE } from './app';
import { Emitter } from './core/event';
import { DATA_PARAMS, PARAM_PATHS } from './data/engineData';
import { SCALE_NAMES, SCALE_TEXT_LENGTH } from './data/paramText';
import type { SkinBridge } from './skin';
import type { Edit } from './views/edit';
import type { FrameView } from './views/frameView';
import type { MainScreenView } from './views/mainScreenView';
import type { PianoView } from './views/pianoView';
import { manifest } from '../manifest';

const param = (id: string) => PARAMS.find((p) => p.id === id)!;

/**
 * In-memory bridge that behaves like hosts/web/plugin.ts: no echo for the skin's own changes,
 * the engine keeps Octave and Chord Length Min ≤ Max and tells everyone about the one it moved.
 * The scale mask is whatever the test sets (the core computes it in the plugin; bc_tests checks that).
 */
function fakeBridge(overrides: Record<string, number> = {}) {
  const values = new Map(PARAMS.map((p) => [p.id, toNormalized(p, overrides[p.id] ?? p.default)]));
  const listeners = new Set<(id: string, n: number) => void>();
  const calls: string[] = [];
  const plain = (id: string) => toPlain(param(id), values.get(id)!);
  const push = (id: string, v: number) => {
    values.set(id, toNormalized(param(id), v));
    for (const l of [...listeners]) l(id, values.get(id)!);
  };
  const bridge: SkinBridge = {
    params: PARAMS,
    get: (id) => values.get(id) ?? 0,
    set(id, n, gesture) {
      calls.push(`${id}=${+n.toFixed(4)}${gesture ? ` ${gesture}` : ''}`);
      values.set(id, n);
      if (id === 'octaveMin' && plain('octaveMin') > plain('octaveMax')) push('octaveMax', plain('octaveMin'));
      if (id === 'octaveMax' && plain('octaveMax') < plain('octaveMin')) push('octaveMin', plain('octaveMax'));
      if (id === 'chordMin' && plain('chordMin') > plain('chordMax')) push('chordMax', plain('chordMin'));
      if (id === 'chordMax' && plain('chordMax') < plain('chordMin')) push('chordMin', plain('chordMax'));
    },
    onParam(cb) {
      listeners.add(cb);
      return () => listeners.delete(cb);
    },
    view: () => ({ loop: 8, chords: [] }),
    onView: () => () => {},
    playhead: () => null,
    scaleMask: () => state.mask,
  };
  const state = { mask: 0b101010110101 }; // C major
  return { bridge, calls, plain, push, state, listenerCount: () => listeners.size };
}

function fakeViews() {
  const main = {
    note: '',
    scale: '',
    scaleList: [] as string[],
    scaleValue: -1,
    listOpen: false,
    scroll: 0,
    loop: { label: '', pos: 0 },
    length: { label: '', min: 0, max: 0 },
    drift: 0,
    notes: '',
    density: 0,
    setNote(t: string) { this.note = t; },
    setScale(t: string) { this.scale = t; },
    setScaleList(items: string[]) { this.scaleList = items; },
    setScaleValue(i: number) { this.scaleValue = i; },
    openScaleList() { this.listOpen = true; },
    hideScaleList() { this.listOpen = false; },
    setScaleListScroll(r: number) { this.scroll = r; },
    setLoop(label: string, pos: number) { this.loop = { label, pos }; },
    setLength(label: string, min: number, max: number) { this.length = { label, min, max }; },
    setDrift(p: number) { this.drift = p; },
    setNotes(t: string) { this.notes = t; },
    setDensity(p: number) { this.density = p; },
    noteLeftClicked: new Emitter<void>(),
    noteRightClicked: new Emitter<void>(),
    scaleButtonClicked: new Emitter<void>(),
    scaleSelected: new Emitter<number>(),
    scaleListScrolled: new Emitter<number>(),
    loopChanged: new Emitter<Edit>(),
    lengthMinChanged: new Emitter<Edit>(),
    lengthMaxChanged: new Emitter<Edit>(),
    driftChanged: new Emitter<Edit>(),
    notesDownClicked: new Emitter<void>(),
    notesUpClicked: new Emitter<void>(),
    densityChanged: new Emitter<Edit>(),
  } satisfies MainScreenView & Record<string, unknown>;
  const piano = {
    min: '',
    max: '',
    dots: [] as boolean[],
    setOctaveMin(t: string) { this.min = t; },
    setOctaveMax(t: string) { this.max = t; },
    setScaleNotes(n: boolean[]) { this.dots = n; },
    octaveMinUpClicked: new Emitter<void>(),
    octaveMinDownClicked: new Emitter<void>(),
    octaveMaxUpClicked: new Emitter<void>(),
    octaveMaxDownClicked: new Emitter<void>(),
  } satisfies PianoView & Record<string, unknown>;
  const frame = { shuffleClicked: new Emitter<void>() } satisfies FrameView;
  return { main, piano, frame };
}

function setup(overrides: Record<string, number> = {}) {
  const b = fakeBridge(overrides);
  const views = fakeViews();
  const app = createUi(b.bridge, views);
  return { ...b, ...views, app };
}

describe('engine data', () => {
  it('has a path for every parameter except triggers', () => {
    expect(Object.keys(PARAM_PATHS).sort()).toEqual(DATA_PARAMS.map((p) => p.id).sort());
    expect(PARAMS.filter((p) => !(p.id in PARAM_PATHS)).map((p) => p.id)).toEqual(['shuffle']);
  });

  it('starts from the bridge values in plain units', () => {
    const { app } = setup();
    for (const p of DATA_PARAMS) {
      const [group, key] = PARAM_PATHS[p.id];
      expect((app.engine.getData()[group] as unknown as Record<string, number>)[key], p.id).toBe(p.default);
    }
  });
});

describe('plugin size', () => {
  it('is the Figma device size, 298 x 169', () => {
    expect(DEVICE).toEqual({ width: 298, height: 169 });
  });
  it('manifest ui size = DEVICE', () => {
    expect([manifest.ui?.width, manifest.ui?.height]).toEqual([DEVICE.width, DEVICE.height]);
  });
});

describe('main screen', () => {
  it('shows every value on start', () => {
    const { main } = setup();
    expect(main.note).toBe('C');
    expect(main.scale).toBe('MAJOR');
    expect(main.scaleList).toEqual(SCALE_NAMES);
    expect(main.loop.label).toBe('LOOP(8)');
    expect(main.length.label).toBe('LEN(1-3)');
    expect(main.notes).toBe('3');
    expect(main.density).toBeCloseTo(0.8);
    expect(main.listOpen).toBe(false);
  });

  it('every scale name fits the SCALE list (12 characters)', () => {
    for (const name of SCALE_NAMES) expect(name.length, name).toBeLessThanOrEqual(SCALE_TEXT_LENGTH);
  });

  it('NOTE < > wraps around', () => {
    const { main, plain } = setup({ root: 11 });
    expect(main.note).toBe('B');
    main.noteRightClicked.emit();
    expect(plain('root')).toBe(0);
    expect(main.note).toBe('C');
    main.noteLeftClicked.emit();
    expect(main.note).toBe('B');
  });

  it('the ↓ button opens and closes the scale list; picking a scale selects it and closes the list', () => {
    const { main, plain } = setup();
    main.scaleButtonClicked.emit();
    expect(main.listOpen).toBe(true);
    main.scaleButtonClicked.emit();
    expect(main.listOpen).toBe(false);
    main.scaleButtonClicked.emit();
    main.scaleSelected.emit(13);
    expect(plain('scale')).toBe(13);
    expect(main.scale).toBe('MIN. PENTA');
    expect(main.scaleValue).toBe(13);
    expect(main.listOpen).toBe(false);
  });

  it('a slider drag is one gesture: begin, values, end', () => {
    const { main, calls, plain } = setup();
    main.loopChanged.emit({ value: 0.5, gesture: 'begin' });
    main.loopChanged.emit({ value: 1 });
    main.loopChanged.emit({ value: 1, gesture: 'end' });
    expect(calls[0]).toMatch(/^loop=.* begin$/);
    expect(calls.at(-1)).toMatch(/^loop=1 end$/);
    expect(plain('loop')).toBe(32);
    expect(main.loop.label).toBe('LOOP(32)');
  });

  it('LEN has two handles: chordMin and chordMax', () => {
    const { main, plain } = setup();
    main.lengthMaxChanged.emit({ value: 1 });
    main.lengthMinChanged.emit({ value: toNormalized(param('chordMin'), 16) });
    expect([plain('chordMin'), plain('chordMax')]).toEqual([16, 32]);
    expect(main.length.label).toBe('LEN(16-32)');
  });

  it('LEN min past max takes max along (engine rule), and the label follows', () => {
    const { main, plain } = setup({ chordMin: 2, chordMax: 4 });
    main.lengthMinChanged.emit({ value: toNormalized(param('chordMin'), 8) });
    expect([plain('chordMin'), plain('chordMax')]).toEqual([8, 8]);
    expect(main.length.label).toBe('LEN(8-8)');
    expect(main.length.min).toBeCloseTo(main.length.max);
  });

  it('NOTES - + step the notes count and stop at 1 and 6', () => {
    const { main, plain } = setup({ notesCount: 6 });
    main.notesUpClicked.emit();
    expect(plain('notesCount')).toBe(6);
    main.notesDownClicked.emit();
    expect(main.notes).toBe('5');
  });

  it('follows changes from outside (automation, state load)', () => {
    const { main, push } = setup();
    push('notesCount', 4);
    expect(main.notes).toBe('4');
  });
});

describe('piano screen', () => {
  it('+ / - step the octaves and stop at the ends', () => {
    const { piano, plain } = setup({ octaveMin: 0, octaveMax: 6 });
    piano.octaveMinDownClicked.emit();
    piano.octaveMaxUpClicked.emit();
    expect([plain('octaveMin'), plain('octaveMax')]).toEqual([0, 6]);
    piano.octaveMaxDownClicked.emit();
    expect(piano.max).toBe('5');
  });

  it('Octave Min past Max takes Max along (engine rule), and both numbers update', () => {
    const { piano, plain } = setup({ octaveMin: 3, octaveMax: 3 });
    piano.octaveMinUpClicked.emit();
    expect([plain('octaveMin'), plain('octaveMax')]).toEqual([4, 4]);
    expect([piano.min, piano.max]).toEqual(['4', '4']);
  });
});

describe('piano dots', () => {
  it('shows the core scale mask, C..B, and follows root and scale changes', () => {
    const { piano, main, state } = setup();
    expect(piano.dots).toEqual([true, false, true, false, true, true, false, true, false, true, false, true]);
    state.mask = (1 << 9) | (1 << 0) | (1 << 2) | (1 << 4) | (1 << 7); // A minor pentatonic
    main.noteRightClicked.emit(); // root changed → dots read again
    expect(piano.dots.map((on, pc) => (on ? pc : -1)).filter((pc) => pc >= 0)).toEqual([0, 2, 4, 7, 9]);
  });
});

describe('frame', () => {
  it('SHUFFLE fires the trigger once', () => {
    const { frame, calls } = setup();
    frame.shuffleClicked.emit();
    expect(calls).toEqual(['shuffle=1 begin', 'shuffle=0 end']);
  });
});

describe('dispose', () => {
  it('drops every subscription', () => {
    const { app, main, calls, listenerCount } = setup();
    app.dispose();
    expect(listenerCount()).toBe(0);
    main.noteRightClicked.emit();
    expect(calls).toEqual([]);
  });
});
