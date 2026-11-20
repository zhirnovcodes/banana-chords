import { INDEX } from '../../../../../hosts/web/params';
import { Subscriptions } from '../core/event';
import { octaveText } from '../data/paramText';
import type { EngineDataModel } from '../models/engineDataModel';
import type { PianoView } from '../views/pianoView';
import { stepParam } from './paramAccess';

/**
 * Connects the piano screen to the engine data model. + / - step Octave Min and Octave Max and stop at 0 and 6;
 * the engine keeps Min ≤ Max (moving one past the other takes the other along).
 * Scale notes (red dots) come in step 6.
 */
export class PianoPresenter {
  private readonly subs = new Subscriptions();

  constructor(
    private readonly engine: EngineDataModel,
    private readonly view: PianoView,
  ) {
    const mine = new Set(['octaveMin', 'octaveMax'].map((id) => INDEX.get(id)!));
    this.showOctaves();
    this.subs.add(view.octaveMinUpClicked.on(() => stepParam(engine, 'octaveMin', +1)));
    this.subs.add(view.octaveMinDownClicked.on(() => stepParam(engine, 'octaveMin', -1)));
    this.subs.add(view.octaveMaxUpClicked.on(() => stepParam(engine, 'octaveMax', +1)));
    this.subs.add(view.octaveMaxDownClicked.on(() => stepParam(engine, 'octaveMax', -1)));
    this.subs.add(engine.changed.on((index) => mine.has(index) && this.showOctaves()));
  }

  dispose(): void {
    this.subs.dispose();
  }

  private showOctaves(): void {
    const { main } = this.engine.getData();
    this.view.setOctaveMin(octaveText(main.octaveMin));
    this.view.setOctaveMax(octaveText(main.octaveMax));
  }
}
