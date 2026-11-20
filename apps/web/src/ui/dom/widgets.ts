import { Emitter } from '../core/event';
import type { Edit } from '../views/edit';

// The screens' building blocks at Figma size. Every coordinate is px on the 298 x 169 device (Figma Frame 4,
// device origin subtracted), so numbers here can be checked against assets/frame4-reference.svg.

export interface Box {
  x: number;
  y: number;
  w?: number;
  h?: number;
}

export function place<T extends HTMLElement | SVGElement>(e: T, b: Box): T {
  e.style.left = `${b.x}px`;
  e.style.top = `${b.y}px`;
  if (b.w !== undefined) e.style.width = `${b.w}px`;
  if (b.h !== undefined) e.style.height = `${b.h}px`;
  return e;
}

export function div(doc: Document, cls: string, b?: Box, text?: string): HTMLDivElement {
  const e = doc.createElement('div');
  e.className = cls;
  if (b) place(e, b);
  if (text !== undefined) e.textContent = text;
  return e;
}

// 8-bit Operator+ 8 at Figma's 12.5 px (one font pixel = 1.0417 px). In its 15 px line box a glyph starts
// 1 px right and 3.1 px down, so a label placed at a Figma glyph's top-left lands on it.
const GLYPH_DX = 1;
const GLYPH_DY = 3.1;

/** A text whose first glyph's top-left is at (x, y), as read from the Figma export. */
export function label(doc: Document, text: string, x: number, y: number): HTMLDivElement {
  return div(doc, 'bc-text', { x: x - GLYPH_DX, y: y - GLYPH_DY }, text);
}

/** A text centred on x; y is the glyph top. */
export function centredLabel(doc: Document, text: string, cx: number, y: number): HTMLDivElement {
  const e = label(doc, text, cx, y);
  e.classList.add('bc-center');
  e.style.left = `${cx}px`;
  return e;
}

/** A grey pixel button: #DCDDD9 face, 1 px #AEAFAB shadow down-right; pressed = sits on its shadow. */
export class PixelButton {
  readonly el: HTMLButtonElement;
  readonly clicked = new Emitter<void>();

  constructor(doc: Document, b: Box, content: string | Node, ariaLabel: string) {
    const e = doc.createElement('button');
    e.type = 'button';
    e.className = 'bc-btn';
    e.setAttribute('aria-label', ariaLabel);
    place(e, b);
    if (typeof content === 'string') e.textContent = content;
    else e.append(content);
    e.addEventListener('click', () => this.clicked.emit());
    this.el = e;
  }
}

/** The ↓ glyph of the SCALE button (Figma: a 2 px stem and a 3-row head), 12 x 13 button. */
export function arrowDown(doc: Document): SVGSVGElement {
  const svg = doc.createElementNS('http://www.w3.org/2000/svg', 'svg');
  svg.setAttribute('width', '12');
  svg.setAttribute('height', '13');
  svg.setAttribute('viewBox', '0 0 12 13');
  svg.classList.add('bc-arrow');
  svg.innerHTML =
    '<path d="M5 2h2v8H5z M3 9h6v1H3z M4 10h4v1H4z M5 11h2v1H5z" fill="#000"/>';
  return svg;
}

/** Slider handle width (Figma Frame 6). */
const HANDLE_W = 6;

export interface SliderOptions {
  label: string;
}

/**
 * A slider from Figma Frame 6: a 2 px black line and 6 x 10 handles (#DCDDD9, 1 px #AEAFAB shadow).
 * One handle, or two (LEN: min and max). Drag a handle, click the line (nearest handle moves), wheel, arrow keys.
 */
export class PixelSlider {
  readonly el: HTMLDivElement;
  /** One emitter per handle. */
  readonly edited: Emitter<Edit>[];
  private readonly handles: HTMLDivElement[];
  private readonly positions: number[];

  constructor(
    doc: Document,
    /** Line from x0 to x1 (px on the device), its centre at y. */
    private readonly x0: number,
    private readonly x1: number,
    y: number,
    options: SliderOptions & { handles?: 1 | 2 },
  ) {
    const count = options.handles ?? 1;
    const width = x1 - x0;
    const e = div(doc, 'bc-slider', { x: x0, y: y - 5, w: width, h: 10 });
    e.append(div(doc, 'bc-track', { x: 0, y: 4, w: width, h: 2 }));
    this.edited = Array.from({ length: count }, () => new Emitter<Edit>());
    this.positions = Array.from({ length: count }, () => 0);
    this.handles = Array.from({ length: count }, (_, i) => {
      const h = div(doc, 'bc-handle', { x: 0, y: 0, w: HANDLE_W, h: 10 });
      h.tabIndex = 0;
      h.setAttribute('role', 'slider');
      h.setAttribute('aria-label', count === 2 ? `${options.label} ${i === 0 ? 'min' : 'max'}` : options.label);
      h.setAttribute('aria-valuemin', '0');
      h.setAttribute('aria-valuemax', '1');
      h.addEventListener('keydown', (event) => {
        const step = { ArrowRight: 0.01, ArrowUp: 0.01, ArrowLeft: -0.01, ArrowDown: -0.01, PageUp: 0.1, PageDown: -0.1 }[event.key];
        const to = event.key === 'Home' ? 0 : event.key === 'End' ? 1 : step === undefined ? null : clamp(this.positions[i] + step);
        if (to === null) return;
        event.preventDefault();
        this.edited[i].emit({ value: to });
      });
      e.append(h);
      return h;
    });
    this.el = e;

    let dragging = -1;
    e.addEventListener('pointerdown', (event) => {
      if (event.button !== 0) return;
      event.preventDefault();
      const v = this.fromPointer(event.clientX);
      dragging = this.nearest(v);
      this.handles[dragging].focus();
      e.setPointerCapture(event.pointerId);
      this.edited[dragging].emit({ value: v, gesture: 'begin' });
    });
    e.addEventListener('pointermove', (event) => {
      if (dragging >= 0) this.edited[dragging].emit({ value: this.fromPointer(event.clientX) });
    });
    const end = (event: PointerEvent) => {
      if (dragging < 0) return;
      const i = dragging;
      dragging = -1;
      this.edited[i].emit({ value: this.fromPointer(event.clientX), gesture: 'end' });
    };
    e.addEventListener('pointerup', end);
    e.addEventListener('pointercancel', end);
    e.addEventListener('wheel', (event) => {
      event.preventDefault();
      const i = count === 2 && (event.target as Element) === this.handles[0] ? 0 : count - 1;
      this.edited[i].emit({ value: clamp(this.positions[i] + (event.deltaY < 0 ? 0.03 : -0.03)) });
    });
  }

  /** Handle `i` to position 0..1. */
  set(i: number, v: number): void {
    this.positions[i] = clamp(v);
    this.handles[i].style.left = `${Math.round(this.positions[i] * (this.x1 - this.x0 - HANDLE_W))}px`;
    this.handles[i].setAttribute('aria-valuenow', this.positions[i].toFixed(3));
  }

  get(i = 0): number {
    return this.positions[i];
  }

  private nearest(v: number): number {
    if (this.handles.length === 1) return 0;
    const [a, b] = this.positions;
    if (a === b) return v < a ? 0 : 1;
    return Math.abs(v - a) <= Math.abs(v - b) ? 0 : 1;
  }

  private fromPointer(clientX: number): number {
    const rect = this.el.getBoundingClientRect();
    const width = this.x1 - this.x0;
    const x = ((clientX - rect.left) / rect.width) * width;
    return clamp((x - HANDLE_W / 2) / (width - HANDLE_W));
  }
}

export function clamp(v: number): number {
  return Math.min(1, Math.max(0, Number.isFinite(v) ? v : 0));
}
