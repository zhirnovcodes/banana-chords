import { PARAMS, toNormalized, toPlain } from '../../../hosts/web/params';
import { mountSkin, type SkinBridge } from './ui/skin';

// Skin preview (pnpm dev → /preview.html): the UI alone, on an in-memory bridge. No audio, no wasm.
// Query: any param id = plain value (e.g. ?scale=13&loop=32); &open=1 opens the SCALE list; &down=1 holds SHUFFLE.

const query = new URLSearchParams(location.search);
const values = new Map(PARAMS.map((p) => [p.id, toNormalized(p, Number(query.get(p.id) ?? p.default))]));
const listeners = new Set<(id: string, n: number) => void>();
const plain = (id: string) => toPlain(PARAMS.find((p) => p.id === id)!, values.get(id)!);
const push = (id: string, v: number) => {
  values.set(id, toNormalized(PARAMS.find((p) => p.id === id)!, v));
  for (const l of [...listeners]) l(id, values.get(id)!);
};

const bridge: SkinBridge = {
  params: PARAMS,
  get: (id) => values.get(id) ?? 0,
  set(id, n) {
    values.set(id, n);
    // The engine's rule: Octave Min ≤ Max, moving one past the other takes the other along.
    if (id === 'octaveMin' && plain('octaveMin') > plain('octaveMax')) push('octaveMax', plain('octaveMin'));
    if (id === 'octaveMax' && plain('octaveMax') < plain('octaveMin')) push('octaveMin', plain('octaveMax'));
  },
  onParam(cb) {
    listeners.add(cb);
    return () => listeners.delete(cb);
  },
  view: () => ({ loop: plain('loop'), chords: [] }),
  onView: () => () => {},
  playhead: () => null,
};

mountSkin(document.getElementById('skin')!, bridge);
if (query.get('open') === '1') (document.querySelector('.bc-field') as HTMLElement).click();
if (query.get('down') === '1') document.querySelector('.bc-device')!.classList.add('bc-shuffle-down');
