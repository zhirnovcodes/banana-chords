import { Subscriptions } from '../core/event';
import type { EngineDataModel } from '../models/engineDataModel';
import type { FrameView } from '../views/frameView';

/** Connects the frame to the engine data model: SHUFFLE regenerates every sequence. */
export class FramePresenter {
  private readonly subs = new Subscriptions();

  constructor(engine: EngineDataModel, view: FrameView) {
    this.subs.add(view.shuffleClicked.on(() => engine.shuffle()));
  }

  dispose(): void {
    this.subs.dispose();
  }
}
