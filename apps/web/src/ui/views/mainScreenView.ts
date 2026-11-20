import type { Event } from '../core/event';
import type { Edit } from './edit';

/**
 * 1 - Main screen view: the big screen (Figma Frame 4, screens 1 and 2). Texts come in ready to show,
 * sliders take positions 0..1.
 */
export interface MainScreenView {
  /** NOTE < C > */
  setNote(text: string): void;
  readonly noteLeftClicked: Event<void>;
  readonly noteRightClicked: Event<void>;

  /** SCALE: the field, the ↓ button and the drop-down list (screen 2). */
  setScale(text: string): void;
  setScaleList(items: string[]): void;
  /** Mark the selected entry. */
  setScaleValue(index: number): void;
  openScaleList(): void;
  hideScaleList(): void;
  /** First visible row of the list (the scroll handle follows it). */
  setScaleListScroll(row: number): void;
  readonly scaleButtonClicked: Event<void>;
  /** Index into the scale list. */
  readonly scaleSelected: Event<number>;
  /** The list was scrolled to this first row. */
  readonly scaleListScrolled: Event<number>;

  /** LOOP (n) slider. */
  setLoop(label: string, position: number): void;
  readonly loopChanged: Event<Edit>;

  /** LEN(min-max): one slider, two handles. */
  setLength(label: string, minPosition: number, maxPosition: number): void;
  readonly lengthMinChanged: Event<Edit>;
  readonly lengthMaxChanged: Event<Edit>;

  /** DRIFT slider. */
  setDrift(position: number): void;
  readonly driftChanged: Event<Edit>;

  /** NOTES(n) slider. */
  setNotes(label: string, position: number): void;
  readonly notesChanged: Event<Edit>;

  /** DENSITY slider. */
  setDensity(position: number): void;
  readonly densityChanged: Event<Edit>;
}
