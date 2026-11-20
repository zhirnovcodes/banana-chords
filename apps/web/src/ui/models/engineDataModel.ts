import { INDEX, PARAMS, toNormalized, toPlain } from '../../../../../hosts/web/params';
import { Emitter, type Event } from '../core/event';
import {
  type AdditionalData,
  DATA_PARAMS,
  emptyEngineData,
  type EngineData,
  type GroupKey,
  type MainData,
  PARAM_PATHS,
  readPlain,
  writePlain,
} from '../data/engineData';
import type { SkinBridge } from '../skin';

/**
 * 1 - Engine data model: the decorator over EngineData. The engine (through SkinBridge) is the source of truth.
 * A setter writes to the bridge, then reads every value back: the engine may move others too
 * (Octave Min pushes Octave Max). Changes from outside (automation, state load) arrive through onParam.
 * `changed` fires once per value that really changed, with its params.json index.
 */
export class EngineDataModel {
  private readonly data: EngineData = emptyEngineData();
  private readonly changedEmitter = new Emitter<number>();
  private readonly offs: (() => void)[] = [];

  readonly changed: Event<number> = this.changedEmitter;

  constructor(private readonly bridge: SkinBridge) {
    for (const p of DATA_PARAMS) writePlain(this.data, p.id, toPlain(p, bridge.get(p.id)));
    this.offs.push(bridge.onParam((id) => this.pull(id)));
  }

  getData(): EngineData {
    return structuredClone(this.data);
  }

  getMainData(): MainData {
    return { ...this.data.main };
  }
  setMainData(value: MainData): void {
    this.apply('main', { ...this.data, main: value });
  }

  getAdditionalData(): AdditionalData {
    return { ...this.data.additional };
  }
  setAdditionalData(value: AdditionalData): void {
    this.apply('additional', { ...this.data, additional: value });
  }

  /** The SHUFFLE button: new seeds for every sequence. */
  shuffle(): void {
    this.bridge.set('shuffle', 1, 'begin');
    this.bridge.set('shuffle', 0, 'end');
    this.pullAll();
  }

  /** Slider drags: the host records one automation gesture between begin and end. */
  beginGesture(index: number): void {
    this.gesture(index, 'begin');
  }
  endGesture(index: number): void {
    this.gesture(index, 'end');
  }

  dispose(): void {
    for (const off of this.offs.splice(0)) off();
    this.changedEmitter.clear();
  }

  private gesture(index: number, gesture: 'begin' | 'end'): void {
    const p = PARAMS[index];
    if (p && p.kind !== 'trigger') this.bridge.set(p.id, this.bridge.get(p.id), gesture);
  }

  /**
   * Writes the params of `group` whose value in `next` differs, then reads everything back.
   * The changes are decided before the first write: a write can move another value of the same group
   * (Octave Min pushes Octave Max), and that move must not be written back.
   */
  private apply(group: GroupKey, next: EngineData): void {
    const changes = DATA_PARAMS.filter(
      (p) => PARAM_PATHS[p.id][0] === group && readPlain(next, p.id) !== readPlain(this.data, p.id),
    );
    for (const p of changes) this.bridge.set(p.id, toNormalized(p, readPlain(next, p.id)));
    this.pullAll();
  }

  private pullAll(): void {
    for (const p of DATA_PARAMS) this.pull(p.id);
  }

  private pull(id: string): void {
    const index = INDEX.get(id);
    if (index === undefined || !(id in PARAM_PATHS)) return;
    const plain = toPlain(PARAMS[index], this.bridge.get(id));
    if (plain === readPlain(this.data, id)) return;
    writePlain(this.data, id, plain);
    this.changedEmitter.emit(index);
  }
}
