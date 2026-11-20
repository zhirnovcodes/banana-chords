// The Banana Chords skin: the device (device/device.svg) with its two screens, plain DOM, used by every host
// through SkinBridge. It never knows which host runs it. All classes start with "bc-".
// Layers (AxolotlPad structure): data → models → presenters ← views (dom/). app.ts wires them.

import type { ParameterInfo } from 'ivandaw-core';
import { createUi, DEVICE } from './app';
import deviceSvg from './device/device.svg?raw';
import { FrameDomView } from './dom/frameDom';
import { MainScreenDomView } from './dom/mainScreenDom';
import { PianoDomView } from './dom/pianoDom';
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
  /** Changes from outside the skin (host automation, state load, the engine moving another value). */
  onParam(cb: (id: string, normalized: number) => void): () => void;
  view(): SequenceView;
  /** The sequences or anything shown in the timeline changed. */
  onView(cb: () => void): () => void;
  /** Beat now, or null when stopped. */
  playhead(): number | null;
  /** Pitch classes of the selected scale on the selected root, from the core: bit k = pitch class k (0 = C). */
  scaleMask(): number;
}

const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

/** C3 = 60, as in IvanDAW and Ableton. */
export function noteName(pitch: number): string {
  return NOTE_NAMES[pitch % 12] + (Math.floor(pitch / 12) - 2);
}

// Keys a focused control uses. Everything else (space = play) reaches the DAW.
const CONTROL_KEYS = new Set(['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown', 'Home', 'End', 'PageUp', 'PageDown', 'Enter']);

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

let instances = 0;

/** device.svg with its ids made unique: two plugin instances share a document. */
function deviceMarkup(): string {
  const prefix = `bc${++instances}-`;
  return deviceSvg
    .replace(/\bid="([^"]+)"/g, `id="${prefix}$1"`)
    .replace(/url\(#([^)]+)\)/g, `url(#${prefix}$1)`);
}

/** Renders the skin into `container` at Figma size (DEVICE, 298 x 169). Returns the unmount function. */
export function mountSkin(container: HTMLElement, bridge: SkinBridge): () => void {
  const doc = container.ownerDocument;
  const releaseStyles = holdStyles(doc);

  const root = doc.createElement('div');
  root.className = 'bc-root';
  root.style.width = `${DEVICE.width}px`;
  root.style.height = `${DEVICE.height}px`;
  root.addEventListener('keydown', (event) => {
    if (CONTROL_KEYS.has(event.key)) event.stopPropagation();
  });
  root.innerHTML = deviceMarkup();
  const svg = root.querySelector('svg')!;
  svg.classList.add('bc-device');

  const main = new MainScreenDomView(doc);
  const piano = new PianoDomView(doc);
  const frame = new FrameDomView(doc, svg);
  root.append(main.el, piano.el, frame.el);
  container.append(root);

  const app = createUi(bridge, { main, piano, frame });

  return () => {
    app.dispose();
    main.dispose();
    root.remove();
    releaseStyles();
  };
}
