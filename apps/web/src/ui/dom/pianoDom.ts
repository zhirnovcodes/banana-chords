import { Emitter } from '../core/event';
import type { PianoView } from '../views/pianoView';
import { centredLabel, div, PixelButton } from './widgets';

/**
 * The small screen, Figma Frame 4: + - and Octave Min on the left, + - and Octave Max on the right.
 * The keyboard between them, with the scale's red dots, is step 6.
 */
export class PianoDomView implements PianoView {
  readonly el: HTMLDivElement;
  readonly octaveMinUpClicked = new Emitter<void>();
  readonly octaveMinDownClicked = new Emitter<void>();
  readonly octaveMaxUpClicked = new Emitter<void>();
  readonly octaveMaxDownClicked = new Emitter<void>();
  private readonly min: HTMLDivElement;
  private readonly max: HTMLDivElement;
  private notes: boolean[] = [];

  constructor(doc: Document) {
    this.el = div(doc, 'bc-piano');
    const button = (x: number, text: string, aria: string, out: Emitter<void>) => {
      const b = new PixelButton(doc, { x, y: 125, w: 9, h: 10 }, text, aria);
      b.clicked.on(() => out.emit());
      return b.el;
    };
    this.min = centredLabel(doc, '', 160, 144.7);
    this.max = centredLabel(doc, '', 270, 144.7);
    this.el.append(
      button(147, '+', 'Octave min up', this.octaveMinUpClicked),
      button(162.6, '-', 'Octave min down', this.octaveMinDownClicked),
      this.min,
      button(257, '+', 'Octave max up', this.octaveMaxUpClicked),
      button(272.6, '-', 'Octave max down', this.octaveMaxDownClicked),
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
    this.notes = [...notes];
  }
}
