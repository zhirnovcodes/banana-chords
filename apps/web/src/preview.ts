import { loadCore } from '../../../hosts/web/core';
import { INDEX, PARAMS, toNormalized, toPlain } from '../../../hosts/web/params';
import { mountSkin, type SkinBridge } from './ui/skin';

// Skin preview (pnpm dev → /preview.html): the UI alone on the real engine (wasm), no audio.
// Every rule (Min ≤ Max, scale notes) comes from the core, as in the plugin.
// Query: any param id = plain value (e.g. ?scale=13&loop=32); &open=1 opens the SCALE list; &down=1 holds SHUFFLE.

const query = new URLSearchParams(location.search);
const core = await loadCore(new URL('/banana-chords.wasm', location.href));
const engine = core.bc_create(48000, 1);
const plain = (id: string) => core.bc_param(engine, INDEX.get(id)!);
for (const p of PARAMS) if (query.has(p.id)) core.bc_set_param(engine, INDEX.get(p.id)!, Number(query.get(p.id)));

const listeners = new Set<(id: string, n: number) => void>();
const bridge: SkinBridge = {
  params: PARAMS,
  get: (id) => toNormalized(PARAMS[INDEX.get(id)!], plain(id)),
  set(id, n) {
    const index = INDEX.get(id)!;
    const before = PARAMS.map((p) => plain(p.id));
    core.bc_set_param(engine, index, toPlain(PARAMS[index], n));
    // The engine may move another value too (Min ≤ Max): tell the skin, like the plugin does.
    PARAMS.forEach((p, i) => {
      if (i !== index && plain(p.id) !== before[i]) for (const l of [...listeners]) l(p.id, bridge.get(p.id));
    });
  },
  onParam(cb) {
    listeners.add(cb);
    return () => listeners.delete(cb);
  },
  view: () => ({ loop: plain('loop'), chords: [] }),
  onView: () => () => {},
  playhead: () => null,
  scaleMask: () => core.bc_scale_mask(engine),
};

mountSkin(document.getElementById('skin')!, bridge);
if (query.get('open') === '1') (document.querySelector('.bc-field') as HTMLElement).click();
if (query.get('down') === '1') document.querySelector('.bc-device')!.classList.add('bc-shuffle-down');
