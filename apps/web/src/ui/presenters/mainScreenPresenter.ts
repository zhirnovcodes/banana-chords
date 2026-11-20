import { INDEX } from '../../../../../hosts/web/params';
import { Subscriptions } from '../core/event';
import { lengthText, loopText, noteText, notesText, SCALE_NAMES, scaleText } from '../data/paramText';
import type { EngineDataModel } from '../models/engineDataModel';
import type { MainUiModel } from '../models/mainUiModel';
import type { MainScreenView } from '../views/mainScreenView';
import { applyEdit, readPosition, stepParam, writeParam } from './paramAccess';

const MINE = ['root', 'scale', 'loop', 'chordMin', 'chordMax', 'drift', 'notesCount', 'density'];

/**
 * Connects the main screen to the models. NOTE < > wraps around (B → C). The ↓ button opens / closes the
 * SCALE list; picking an entry selects the scale and closes the list.
 */
export class MainScreenPresenter {
  private readonly subs = new Subscriptions();

  constructor(
    private readonly engine: EngineDataModel,
    private readonly ui: MainUiModel,
    private readonly view: MainScreenView,
  ) {
    const mine = new Set(MINE.map((id) => INDEX.get(id)!));
    view.setScaleList(SCALE_NAMES);
    this.showAll();
    this.showList();

    this.subs.add(view.noteLeftClicked.on(() => stepParam(engine, 'root', -1, true)));
    this.subs.add(view.noteRightClicked.on(() => stepParam(engine, 'root', +1, true)));
    this.subs.add(view.scaleButtonClicked.on(() => (ui.isScaleListOpen() ? ui.closeScaleList() : ui.openScaleList())));
    this.subs.add(
      view.scaleSelected.on((index) => {
        writeParam(engine, 'scale', index);
        ui.closeScaleList();
      }),
    );
    this.subs.add(view.scaleListScrolled.on((row) => ui.setScaleListScroll(row)));
    this.subs.add(view.loopChanged.on((edit) => applyEdit(engine, 'loop', edit)));
    this.subs.add(view.lengthMinChanged.on((edit) => applyEdit(engine, 'chordMin', edit)));
    this.subs.add(view.lengthMaxChanged.on((edit) => applyEdit(engine, 'chordMax', edit)));
    this.subs.add(view.driftChanged.on((edit) => applyEdit(engine, 'drift', edit)));
    this.subs.add(view.notesChanged.on((edit) => applyEdit(engine, 'notesCount', edit)));
    this.subs.add(view.densityChanged.on((edit) => applyEdit(engine, 'density', edit)));

    this.subs.add(engine.changed.on((index) => mine.has(index) && this.showAll()));
    this.subs.add(ui.changed.on(() => this.showList()));
  }

  dispose(): void {
    this.subs.dispose();
  }

  private showAll(): void {
    const { engine, view } = this;
    const data = engine.getData();
    view.setNote(noteText(data));
    view.setScale(scaleText(data));
    view.setScaleValue(data.main.scale);
    view.setLoop(loopText(data), readPosition(engine, 'loop'));
    view.setLength(lengthText(data), readPosition(engine, 'chordMin'), readPosition(engine, 'chordMax'));
    view.setDrift(readPosition(engine, 'drift'));
    view.setNotes(notesText(data), readPosition(engine, 'notesCount'));
    view.setDensity(readPosition(engine, 'density'));
  }

  private showList(): void {
    if (this.ui.isScaleListOpen()) this.view.openScaleList();
    else this.view.hideScaleList();
    this.view.setScaleListScroll(this.ui.getScaleListScroll());
  }
}
