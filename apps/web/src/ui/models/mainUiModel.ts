import { Emitter, type Event } from '../core/event';
import { initialMainViewData, type MainViewData } from '../data/uiData';

export type MainUiChange = 'scaleList' | 'scaleScroll';

/** 2 - Main UI model: the decorator over MainViewData. */
export class MainUiModel {
  private readonly data: MainViewData = initialMainViewData();
  private readonly changedEmitter = new Emitter<MainUiChange>();

  readonly changed: Event<MainUiChange> = this.changedEmitter;

  isScaleListOpen(): boolean {
    return this.data.scaleListOpen;
  }

  openScaleList(): void {
    this.setOpen(true);
  }

  closeScaleList(): void {
    this.setOpen(false);
  }

  getScaleListScroll(): number {
    return this.data.scaleListScroll;
  }

  setScaleListScroll(row: number): void {
    const next = Math.max(0, Math.round(row));
    if (next === this.data.scaleListScroll) return;
    this.data.scaleListScroll = next;
    this.changedEmitter.emit('scaleScroll');
  }

  private setOpen(open: boolean): void {
    if (open === this.data.scaleListOpen) return;
    this.data.scaleListOpen = open;
    this.changedEmitter.emit('scaleList');
  }
}
