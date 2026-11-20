// The Banana Chords skin: plain DOM, used by every host through SkinBridge.
// It never knows which host runs it. All classes start with "bc-".

import type { ParameterInfo } from 'ivandaw-core';
import css from './skin.css?inline';

export interface SequenceView {
  loop: number; // beats
  chords: { start: number; length: number; notes: number[] }[]; // beats, MIDI notes
}

export interface SkinBridge {
  params: ParameterInfo[];
  /** normalized 0..1 */
  get(id: string): number;
  set(id: string, normalized: number, gesture?: 'begin' | 'end'): void;
  /** Changes from outside the skin (host automation, state load). */
  onParam(cb: (id: string, normalized: number) => void): () => void;
  view(): SequenceView;
  /** The sequences or anything shown in the timeline changed. */
  onView(cb: () => void): () => void;
  /** Beat now, or null when stopped. */
  playhead(): number | null;
}

const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

/** C3 = 60, as in IvanDAW and Ableton. */
export function noteName(pitch: number): string {
  return NOTE_NAMES[pitch % 12] + (Math.floor(pitch / 12) - 2);
}

// One style element per document, shared by every mounted skin.
const styles = new Map<Document, { element: HTMLStyleElement; users: number }>();
function holdStyles(doc: Document): () => void {
  let entry = styles.get(doc);
  if (!entry) {
    const element = doc.createElement('style');
    element.dataset.bananaChords = '';
    element.textContent = css;
    doc.head.append(element);
    entry = { element, users: 0 };
    styles.set(doc, entry);
  }
  entry.users++;
  const held = entry;
  return () => {
    held.users--;
    if (held.users === 0) {
      held.element.remove();
      styles.delete(doc);
    }
  };
}

const plainOf = (p: ParameterInfo, n: number) => {
  const v = p.min + Math.min(1, Math.max(0, n)) * (p.max - p.min);
  return p.kind === 'float' ? v : Math.round(v);
};
const normOf = (p: ParameterInfo, v: number) => (v - p.min) / (p.max - p.min);

function valueText(p: ParameterInfo, v: number): string {
  if (p.kind === 'choice' && p.choices) return p.choices[Math.round(v - p.min)] ?? String(v);
  if (p.kind === 'float') return (v >= 0 && p.min < 0 ? '+' : '') + v.toFixed(2);
  if (p.min < 0 && v > 0) return '+' + v;
  return p.unit ? `${v} ${p.unit === 'beats' ? (v === 1 ? 'beat' : 'beats') : p.unit}` : String(v);
}

function el<K extends keyof HTMLElementTagNameMap>(doc: Document, tag: K, cls?: string, text?: string) {
  const e = doc.createElement(tag);
  if (cls) e.className = cls;
  if (text !== undefined) e.textContent = text;
  return e;
}

/** Renders the skin into `container`. Returns the unmount function. */
export function mountSkin(container: HTMLElement, bridge: SkinBridge): () => void {
  const doc = container.ownerDocument;
  const releaseStyles = holdStyles(doc);
  const cleanups: (() => void)[] = [];

  const root = el(doc, 'div', 'bc-skin');
  const header = el(doc, 'div', 'bc-header');
  const title = el(doc, 'div', 'bc-title');
  title.append(el(doc, 'span', 'bc-logo', 'BANANA'), el(doc, 'span', 'bc-logo2', 'CHORDS'));
  const shuffle = el(doc, 'button', 'bc-shuffle', 'Shuffle');
  shuffle.type = 'button';
  shuffle.dataset.param = 'shuffle';
  shuffle.title = 'Regenerate every sequence';
  shuffle.addEventListener('click', () => {
    bridge.set('shuffle', 1, 'begin');
    bridge.set('shuffle', 0, 'end');
  });
  header.append(title, shuffle);

  // Timeline: one block per chord, the playhead on top.
  const timeline = el(doc, 'div', 'bc-timeline');
  const blocks = el(doc, 'div', 'bc-blocks');
  const head = el(doc, 'div', 'bc-playhead');
  const ruler = el(doc, 'div', 'bc-ruler');
  timeline.append(blocks, head);
  let current: SequenceView = bridge.view();
  let blockEls: HTMLElement[] = [];

  const renderView = () => {
    current = bridge.view();
    blocks.replaceChildren();
    ruler.replaceChildren();
    blockEls = current.chords.map((c, i) => {
      const b = el(doc, 'div', 'bc-block' + (i % 2 ? ' bc-alt' : ''));
      b.style.left = `${(c.start / current.loop) * 100}%`;
      b.style.width = `${(c.length / current.loop) * 100}%`;
      const names = el(doc, 'div', 'bc-notes');
      for (const n of [...c.notes].reverse()) names.append(el(doc, 'div', 'bc-note', noteName(n)));
      b.append(names);
      b.title = `Chord ${i + 1}: ${c.notes.map(noteName).join(' ')} · ${c.length} beats from beat ${c.start}`;
      blocks.append(b);
      return b;
    });
    for (let beat = 0; beat <= current.loop; beat++) {
      const tick = el(doc, 'div', 'bc-tick', beat < current.loop && current.loop <= 16 ? String(beat + 1) : '');
      tick.style.left = `${(beat / current.loop) * 100}%`;
      ruler.append(tick);
    }
  };
  renderView();
  cleanups.push(bridge.onView(renderView));

  let frame = 0;
  let lastActive = -1;
  const animate = () => {
    const beat = bridge.playhead();
    let active = -1;
    if (beat === null || current.loop <= 0) {
      head.style.display = 'none';
    } else {
      const pos = ((beat % current.loop) + current.loop) % current.loop;
      head.style.display = '';
      head.style.left = `${(pos / current.loop) * 100}%`;
      active = current.chords.findIndex((c) => pos >= c.start && pos < c.start + c.length);
    }
    if (active !== lastActive) {
      blockEls[lastActive]?.classList.remove('bc-active');
      blockEls[active]?.classList.add('bc-active');
      lastActive = active;
    }
    frame = (doc.defaultView ?? window).requestAnimationFrame(animate);
  };
  frame = (doc.defaultView ?? window).requestAnimationFrame(animate);
  cleanups.push(() => (doc.defaultView ?? window).cancelAnimationFrame(frame));

  // Controls, by group.
  const groups = el(doc, 'div', 'bc-groups');
  const updaters = new Map<string, () => void>();
  const groupEls = new Map<string, HTMLElement>();
  for (const p of bridge.params) {
    if (p.kind === 'trigger') continue;
    const group = p.group ?? 'Main';
    let g = groupEls.get(group);
    if (!g) {
      g = el(doc, 'div', 'bc-group');
      g.append(el(doc, 'div', 'bc-group-title', group));
      groupEls.set(group, g);
      groups.append(g);
    }
    const row = el(doc, 'label', 'bc-row');
    const label = el(doc, 'span', 'bc-label', p.label);
    const value = el(doc, 'span', 'bc-value');
    let input: HTMLInputElement | HTMLSelectElement;
    if (p.kind === 'choice' && p.choices) {
      const select = el(doc, 'select', 'bc-select');
      p.choices.forEach((c, i) => {
        const o = el(doc, 'option', undefined, c);
        o.value = String(p.min + i);
        select.append(o);
      });
      select.addEventListener('change', () => bridge.set(p.id, normOf(p, Number(select.value))));
      input = select;
      updaters.set(p.id, () => {
        select.value = String(plainOf(p, bridge.get(p.id)));
      });
    } else {
      const range = el(doc, 'input', 'bc-range');
      range.type = 'range';
      range.min = String(p.min);
      range.max = String(p.max);
      range.step = p.kind === 'float' ? '0.01' : '1';
      let dragging = false;
      range.addEventListener('pointerdown', () => {
        dragging = true;
        bridge.set(p.id, normOf(p, Number(range.value)), 'begin');
      });
      range.addEventListener('input', () => {
        bridge.set(p.id, normOf(p, Number(range.value)));
        value.textContent = valueText(p, Number(range.value));
      });
      range.addEventListener('change', () => {
        bridge.set(p.id, normOf(p, Number(range.value)), dragging ? 'end' : undefined);
        dragging = false;
      });
      // Arrow keys move the slider; keep them from the DAW. Space still reaches it.
      range.addEventListener('keydown', (e) => {
        if (e.key.startsWith('Arrow') || e.key === 'Home' || e.key === 'End' || e.key.startsWith('Page')) e.stopPropagation();
      });
      input = range;
      updaters.set(p.id, () => {
        const v = plainOf(p, bridge.get(p.id));
        range.value = String(v);
        value.textContent = valueText(p, v);
      });
    }
    input.dataset.param = p.id;
    if (p.kind === 'choice') value.remove();
    row.append(label, input);
    if (p.kind !== 'choice') row.append(value);
    g.append(row);
  }
  const refresh = (id: string) => updaters.get(id)?.();
  for (const id of updaters.keys()) refresh(id);
  cleanups.push(bridge.onParam((id) => refresh(id)));

  root.append(header, timeline, ruler, groups);
  container.append(root);

  return () => {
    for (const c of cleanups) c();
    root.remove();
    releaseStyles();
  };
}
