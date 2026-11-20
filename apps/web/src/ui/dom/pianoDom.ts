import { Emitter } from '../core/event';
import type { PianoView } from '../views/pianoView';
import { centredLabel, div, PixelButton } from './widgets';

// Figma Frame 6, px on the device: keyboard 77 x 28 stroked 1 px at (176.5, 125.5), i.e. 78 x 29 at (176, 125);
// 7 white keys 11 wide, black keys 7 x 17 at these x. Red dots 2 x 2: white keys at y 149, black keys at y 136.
const KEYS = { x: 176, y: 125, w: 78, h: 29 } as const;
const WHITE = [0, 2, 4, 5, 7, 9, 11]; // pitch classes of the white keys, C..B
const BLACK: Record<number, number> = { 1: 184, 3: 195, 6: 217, 8: 228, 10: 239 }; // pitch class → key x
const NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

/**
 * The small screen, Figma Frame 6: - + and Octave Min on the left, the keyboard with the scale's red dots,
 * - + and Octave Max on the right.
 */
export class PianoDomView implements PianoView {
  readonly el: HTMLDivElement;
  readonly octaveMinUpClicked = new Emitter<void>();
  readonly octaveMinDownClicked = new Emitter<void>();
  readonly octaveMaxUpClicked = new Emitter<void>();
  readonly octaveMaxDownClicked = new Emitter<void>();
  private readonly min: HTMLDivElement;
  private readonly max: HTMLDivElement;
  private readonly keys: HTMLDivElement;
  private readonly dots: HTMLDivElement[] = [];

  constructor(doc: Document) {
    this.el = div(doc, 'bc-piano');
    const button = (x: number, text: string, aria: string, out: Emitter<void>) => {
      const b = new PixelButton(doc, { x, y: 125, w: 9, h: 10 }, text, aria);
      b.clicked.on(() => out.emit());
      return b.el;
    };
    this.min = centredLabel(doc, '', 160, 144.7);
    this.max = centredLabel(doc, '', 270, 144.7);

    // Keys drawn as in Figma: 1 px black outlines, white keys share their borders.
    this.keys = div(doc, 'bc-keys', KEYS);
    this.keys.setAttribute('role', 'img');
    WHITE.forEach((_, i) => this.keys.append(div(doc, 'bc-white', { x: i * 11, y: 0, w: 12, h: 29 })));
    for (const x of Object.values(BLACK)) this.keys.append(div(doc, 'bc-black', { x: x - KEYS.x, y: 0, w: 7, h: 17 }));
    for (let pc = 0; pc < 12; pc++) {
      const black = pc in BLACK;
      const x = black ? BLACK[pc] + 3 : KEYS.x + WHITE.indexOf(pc) * 11 + 5;
      const dot = div(doc, 'bc-dot', { x: Math.round(x) - KEYS.x, y: (black ? 136 : 149) - KEYS.y, w: 2, h: 2 });
      dot.hidden = true;
      this.dots.push(dot);
      this.keys.append(dot);
    }

    this.el.append(
      button(147, '-', 'Octave min down', this.octaveMinDownClicked),
      button(162.6, '+', 'Octave min up', this.octaveMinUpClicked),
      this.min,
      this.keys,
      button(257, '-', 'Octave max down', this.octaveMaxDownClicked),
      button(272.6, '+', 'Octave max up', this.octaveMaxUpClicked),
      this.max,
    );
  }

  setOctaveMin(text: string): void {
    this.min.textContent = text;
  }

  setOctaveMax(text: string): void {
    this.max.textContent = text;
  }

  setScaleNotes(notes: boolean[]): void {
    this.dots.forEach((dot, pc) => (dot.hidden = !notes[pc]));
    this.keys.setAttribute('aria-label', `Scale notes: ${NAMES.filter((_, pc) => notes[pc]).join(' ')}`);
  }
}
