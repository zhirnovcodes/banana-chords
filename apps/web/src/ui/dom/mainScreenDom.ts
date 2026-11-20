import { Emitter } from '../core/event';
import type { Edit } from '../views/edit';
import type { MainScreenView } from '../views/mainScreenView';
import { arrowDown, centredLabel, div, label, PixelButton, PixelSlider } from './widgets';

/** Rows the SCALE list shows at once (Figma: 109 x 76 box, rows every 15 px). */
export const SCALE_ROWS = 5;

/**
 * The big screen, Figma Frame 4 (screen 1, and screen 2 with the SCALE list open).
 * Coordinates: px on the device (Frame 4 minus the device origin).
 */
export class MainScreenDomView implements MainScreenView {
  readonly el: HTMLDivElement;

  readonly noteLeftClicked = new Emitter<void>();
  readonly noteRightClicked = new Emitter<void>();
  readonly scaleButtonClicked = new Emitter<void>();
  readonly scaleSelected = new Emitter<number>();
  readonly scaleListScrolled = new Emitter<number>();
  readonly loopChanged: Emitter<Edit>;
  readonly lengthMinChanged: Emitter<Edit>;
  readonly lengthMaxChanged: Emitter<Edit>;
  readonly driftChanged: Emitter<Edit>;
  readonly notesChanged: Emitter<Edit>;
  readonly densityChanged: Emitter<Edit>;

  private readonly note: HTMLDivElement;
  private readonly scale: HTMLDivElement;
  private readonly loopLabel: HTMLDivElement;
  private readonly lengthLabel: HTMLDivElement;
  private readonly notesLabel: HTMLDivElement;
  private readonly loop: PixelSlider;
  private readonly length: PixelSlider;
  private readonly drift: PixelSlider;
  private readonly notes: PixelSlider;
  private readonly density: PixelSlider;
  private readonly scaleButton: PixelButton;
  private readonly list: HTMLDivElement;
  private readonly rows: HTMLDivElement;
  private readonly thumb: HTMLDivElement;
  private items: string[] = [];
  private selected = -1;
  private scroll = 0;
  private open = false;
  private readonly onOutside: (event: Event) => void;

  constructor(private readonly doc: Document) {
    const el = div(doc, 'bc-main');
    this.el = el;

    // Row 1: NOTE < C >   SCALE [ MAJOR      ] ↓
    const prev = new PixelButton(doc, { x: 54, y: 23, w: 11, h: 10 }, '<', 'Previous note');
    const next = new PixelButton(doc, { x: 88, y: 23, w: 11, h: 10 }, '>', 'Next note');
    prev.clicked.on(() => this.noteLeftClicked.emit());
    next.clicked.on(() => this.noteRightClicked.emit());
    // Centred between < (54..65) and > (88..99): C# is wider than the mock's Cb.
    this.note = centredLabel(doc, '', 77, 24.7);
    const field = div(doc, 'bc-field', { x: 154, y: 22, w: 109, h: 13 });
    this.scale = label(doc, '', 6, 2.7);
    field.append(this.scale);
    this.scaleButton = new PixelButton(doc, { x: 269, y: 22, w: 12, h: 13 }, arrowDown(doc), 'Scale list');
    this.scaleButton.el.setAttribute('aria-expanded', 'false');
    this.scaleButton.clicked.on(() => this.scaleButtonClicked.emit());
    field.addEventListener('click', () => this.scaleButtonClicked.emit());

    // Left column: LOOP, DRIFT, DENSITY
    this.loopLabel = label(doc, '', 19, 46.7);
    this.loop = new PixelSlider(doc, 75, 137, 50, { label: 'Loop' });
    this.drift = new PixelSlider(doc, 63, 137, 72, { label: 'Drift' });
    this.density = new PixelSlider(doc, 70, 137, 94, { label: 'Density' });

    // Right column: LEN (two handles), NOTES
    this.lengthLabel = label(doc, '', 146, 46.7);
    this.length = new PixelSlider(doc, 217, 281, 50, { label: 'Chord length', handles: 2 });
    this.notesLabel = label(doc, '', 146, 68.7);
    this.notes = new PixelSlider(doc, 205, 281, 72, { label: 'Notes' });

    this.loopChanged = this.loop.edited[0];
    this.driftChanged = this.drift.edited[0];
    this.densityChanged = this.density.edited[0];
    this.lengthMinChanged = this.length.edited[0];
    this.lengthMaxChanged = this.length.edited[1];
    this.notesChanged = this.notes.edited[0];

    // Screen 2: the SCALE list over the right column
    this.list = div(doc, 'bc-list', { x: 154, y: 21, w: 109, h: 76 });
    this.list.setAttribute('role', 'listbox');
    this.list.setAttribute('aria-label', 'Scale');
    this.list.hidden = true;
    this.rows = div(doc, 'bc-list-rows', { x: 0, y: 0, w: 97, h: 76 });
    const line = div(doc, 'bc-scroll-line', { x: 102, y: 4, w: 2, h: 69 });
    this.thumb = div(doc, 'bc-thumb', { x: 99, y: 6, w: 6, h: 10 });
    this.list.append(this.rows, line, this.thumb);
    this.list.addEventListener('wheel', (event) => {
      event.preventDefault();
      this.scrollTo(this.scroll + (event.deltaY > 0 ? 1 : -1));
    });
    let dragging = false;
    const scrollFromPointer = (clientY: number) => {
      const rect = this.list.getBoundingClientRect();
      const t = ((clientY - rect.top) / rect.height) * 76;
      this.scrollTo(Math.round(((t - 11) / 57) * this.maxScroll()));
    };
    this.thumb.addEventListener('pointerdown', (event) => {
      event.preventDefault();
      this.thumb.setPointerCapture(event.pointerId);
      dragging = true;
    });
    this.thumb.addEventListener('pointermove', (event) => dragging && scrollFromPointer(event.clientY));
    this.thumb.addEventListener('pointerup', () => (dragging = false));
    line.addEventListener('pointerdown', (event) => scrollFromPointer(event.clientY));
    this.onOutside = (event: Event) => {
      const target = event.target as Node;
      if (!this.open || this.list.contains(target) || this.scaleButton.el.contains(target) || field.contains(target)) return;
      this.scaleButtonClicked.emit();
    };

    el.append(
      label(doc, 'NOTE', 19, 24.7),
      prev.el,
      this.note,
      next.el,
      label(doc, 'SCALE', 111, 24.7),
      field,
      this.scaleButton.el,
      this.loopLabel,
      this.loop.el,
      label(doc, 'DRIFT', 19, 68.7),
      this.drift.el,
      label(doc, 'DENSITY', 19, 90.7),
      this.density.el,
      this.lengthLabel,
      this.length.el,
      this.notesLabel,
      this.notes.el,
      this.list,
    );
  }

  setNote(text: string): void {
    this.note.textContent = text;
  }

  setScale(text: string): void {
    this.scale.textContent = text;
  }

  setScaleList(items: string[]): void {
    this.items = items;
    this.rows.replaceChildren(
      ...items.map((text, i) => {
        const row = div(this.doc, 'bc-text bc-list-row', { x: 0, y: 0, w: 97, h: 15 }, text);
        row.style.paddingLeft = '5px'; // glyphs at x 6 in the list, as in Figma
        row.setAttribute('role', 'option');
        row.addEventListener('click', () => this.scaleSelected.emit(i));
        return row;
      }),
    );
    this.setScaleValue(this.selected);
    this.layout();
  }

  setScaleValue(index: number): void {
    this.selected = index;
    this.rows.querySelectorAll('.bc-list-row').forEach((row, i) => row.setAttribute('aria-selected', String(i === index)));
  }

  openScaleList(): void {
    if (this.open) return;
    this.open = true;
    this.list.hidden = false;
    this.scaleButton.el.setAttribute('aria-expanded', 'true');
    this.el.classList.add('bc-list-open');
    this.scrollTo(this.selected - 1);
    this.doc.addEventListener('pointerdown', this.onOutside, true);
  }

  hideScaleList(): void {
    if (!this.open) return;
    this.open = false;
    this.list.hidden = true;
    this.scaleButton.el.setAttribute('aria-expanded', 'false');
    this.el.classList.remove('bc-list-open');
    this.doc.removeEventListener('pointerdown', this.onOutside, true);
  }

  setScaleListScroll(row: number): void {
    const next = Math.min(this.maxScroll(), Math.max(0, row));
    if (next === this.scroll) return;
    this.scroll = next;
    this.layout();
  }

  setLoop(text: string, position: number): void {
    this.loopLabel.textContent = text;
    this.loop.set(0, position);
  }

  setLength(text: string, minPosition: number, maxPosition: number): void {
    this.lengthLabel.textContent = text;
    this.length.set(0, minPosition);
    this.length.set(1, maxPosition);
  }

  setDrift(position: number): void {
    this.drift.set(0, position);
  }

  setNotes(text: string, position: number): void {
    this.notesLabel.textContent = text;
    this.notes.set(0, position);
  }

  setDensity(position: number): void {
    this.density.set(0, position);
  }

  dispose(): void {
    this.hideScaleList();
  }

  private maxScroll(): number {
    return Math.max(0, this.items.length - SCALE_ROWS);
  }

  /** The view scrolls itself, and tells the presenter (the UI model keeps the row). */
  private scrollTo(row: number): void {
    const next = Math.min(this.maxScroll(), Math.max(0, row));
    if (next !== this.scroll) {
      this.scroll = next;
      this.layout();
    }
    this.scaleListScrolled.emit(next);
  }

  private layout(): void {
    this.rows.querySelectorAll<HTMLElement>('.bc-list-row').forEach((row, i) => {
      const r = i - this.scroll;
      row.hidden = r < 0 || r >= SCALE_ROWS;
      // Row glyphs at (6, 2.7 + 15 r) in the list, as in Figma.
      row.style.top = `${2.7 - 3.1 + r * 15}px`;
    });
    const t = this.maxScroll() === 0 ? 0 : this.scroll / this.maxScroll();
    this.thumb.style.top = `${Math.round(6 + t * 57)}px`;
  }
}
