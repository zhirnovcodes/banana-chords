import { INDEX } from '../../../../../hosts/web/params';
import { Subscriptions } from '../core/event';
import { maskToNotes, octaveText } from '../data/paramText';
import type { EngineDataModel } from '../models/engineDataModel';
import type { PianoView } from '../views/pianoView';
import { stepParam } from './paramAccess';

/**
 * Connects the piano screen to the engine data model. + / - step Octave Min and Octave Max and stop at 0 and 6;
 * the engine keeps Min ≤ Max (moving one past the other takes the other along).
 * Red dots: the notes of the selected scale on the selected root, as the core reports them.
 */
export class PianoPresenter {
  private readonly subs = new Subscriptions();

  constructor(
    private readonly engine: EngineDataModel,
    private readonly view: PianoView,
  ) {
    const mine = new Set(['octaveMin', 'octaveMax'].map((id) => INDEX.get(id)!));
    const scale = new Set(['root', 'scale'].map((id) => INDEX.get(id)!));
    this.showOctaves();
    this.showScale();
    this.subs.add(view.octaveMinUpClicked.on(() => stepParam(engine, 'octaveMin', +1)));
    this.subs.add(view.octaveMinDownClicked.on(() => stepParam(engine, 'octaveMin', -1)));
    this.subs.add(view.octaveMaxUpClicked.on(() => stepParam(engine, 'octaveMax', +1)));
    this.subs.add(view.octaveMaxDownClicked.on(() => stepParam(engine, 'octaveMax', -1)));
    this.subs.add(engine.changed.on((index) => mine.has(index) && this.showOctaves()));
    this.subs.add(engine.changed.on((index) => scale.has(index) && this.showScale()));
  }

  dispose(): void {
    this.subs.dispose();
  }

  private showScale(): void {
    this.view.setScaleNotes(maskToNotes(this.engine.getScaleMask()));
  }

  private showOctaves(): void {
    const { main } = this.engine.getData();
    this.view.setOctaveMin(octaveText(main.octaveMin));
    this.view.setOctaveMax(octaveText(main.octaveMax));
  }
}
