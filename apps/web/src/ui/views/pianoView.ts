import type { Event } from '../core/event';

/**
 * 2 - Piano view: the small screen. Octave Min (- + and its number, left), the keyboard,
 * Octave Max (- + and its number, right). Red dots mark the notes of the selected scale.
 */
export interface PianoView {
  setOctaveMin(text: string): void;
  setOctaveMax(text: string): void;
  /** 12 entries, C..B: true = the key gets a red dot. */
  setScaleNotes(notes: boolean[]): void;

  readonly octaveMinUpClicked: Event<void>;
  readonly octaveMinDownClicked: Event<void>;
  readonly octaveMaxUpClicked: Event<void>;
  readonly octaveMaxDownClicked: Event<void>;
}
