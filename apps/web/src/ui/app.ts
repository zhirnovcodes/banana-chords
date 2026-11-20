import { EngineDataModel } from './models/engineDataModel';
import { MainUiModel } from './models/mainUiModel';
import { FramePresenter } from './presenters/framePresenter';
import { MainScreenPresenter } from './presenters/mainScreenPresenter';
import { PianoPresenter } from './presenters/pianoPresenter';
import type { SkinBridge } from './skin';
import type { FrameView } from './views/frameView';
import type { MainScreenView } from './views/mainScreenView';
import type { PianoView } from './views/pianoView';

/** The device at Figma size (Frame 4: 298 x 169). manifest.ts ui size = this. */
export const DEVICE = { width: 298, height: 169 } as const;

export interface Views {
  main: MainScreenView;
  piano: PianoView;
  frame: FrameView;
}

export interface UiApp {
  engine: EngineDataModel;
  ui: MainUiModel;
  dispose(): void;
}

/** Composition root: builds the models and wires every view to its presenter. The views come from the skin. */
export function createUi(bridge: SkinBridge, views: Views): UiApp {
  const engine = new EngineDataModel(bridge);
  const ui = new MainUiModel();
  const presenters = [
    new MainScreenPresenter(engine, ui, views.main),
    new PianoPresenter(engine, views.piano),
    new FramePresenter(engine, views.frame),
  ];
  return {
    engine,
    ui,
    dispose() {
      for (const p of presenters) p.dispose();
      engine.dispose();
    },
  };
}
