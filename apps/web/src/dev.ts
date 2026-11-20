// Dev page: the plugin from source, a host stand-in with Play / Stop / BPM, a sine synth and an event log.

import {
  type Anchor,
  beatAt,
  beatsIn,
  createTicker,
  type HostContext,
  lookaheadWindow,
  type PluginEvent,
  type PluginInstance,
  TICK_MS,
  type TransportState,
} from 'ivandaw-core';
import factory, { setWasmUrl } from '../../../hosts/web/plugin';
import { noteName } from './ui/skin';
import { SineSynth } from './sine';

const $ = <T extends HTMLElement>(id: string) => document.getElementById(id) as T;
const startBtn = $<HTMLButtonElement>('start');
const playBtn = $<HTMLButtonElement>('play');
const stopBtn = $<HTMLButtonElement>('stop');
const saveBtn = $<HTMLButtonElement>('save');
const bpmInput = $<HTMLInputElement>('bpm');
const volume = $<HTMLInputElement>('volume');
const clickBox = $<HTMLInputElement>('click');
const logEl = $<HTMLDivElement>('log');
const stats = $<HTMLSpanElement>('stats');

setWasmUrl(new URL('/banana-chords.wasm', location.href));

function log(text: string, cls = 'sys') {
  const line = document.createElement('div');
  line.className = cls;
  line.textContent = text;
  logEl.append(line);
  while (logEl.childElementCount > 300) logEl.firstElementChild?.remove();
  logEl.scrollTop = logEl.scrollHeight;
}

let ctx: AudioContext;
let plugin: PluginInstance;
let synth: SineSynth;
let anchor: Anchor;

const bpm = () => Math.min(300, Math.max(20, Number(bpmInput.value) || 120));
const stateAt = (time: number): TransportState => ({ ...anchor.state, beat: beatAt(time, anchor) });

function send(state: TransportState, time: number) {
  anchor = { state, time };
  plugin.schedule([{ type: 'transport', time, state }]);
}

function play() {
  if (anchor.state.playing) return;
  const time = ctx.currentTime + 0.05;
  lastClick = -1;
  send({ playing: true, bpm: bpm(), beat: 0, numerator: 4, denominator: 4, loop: null }, time);
  playBtn.disabled = true;
  stopBtn.disabled = false;
  log(`▶ play at ${time.toFixed(3)} · ${bpm()} BPM`);
}

function stop() {
  if (!anchor.state.playing) return;
  const time = ctx.currentTime;
  send({ ...anchor.state, playing: false, beat: 0 }, time);
  // As IvanDAW does: transport stop, then CC 123.
  plugin.schedule([{ type: 'midi', time, bytes: [0xb0, 123, 0] }]);
  playBtn.disabled = false;
  stopBtn.disabled = true;
  log(`■ stop at ${time.toFixed(3)}`);
}

// Metronome click, booked ahead like every musical event (never setTimeout).
let lastClick = -1;
function clickTick() {
  if (!clickBox.checked || !anchor.state.playing) return;
  for (const n of beatsIn(lookaheadWindow(ctx.currentTime), anchor)) {
    if (n <= lastClick) continue;
    lastClick = n;
    const t = anchor.time + ((n - anchor.state.beat) * 60) / anchor.state.bpm;
    const osc = ctx.createOscillator();
    const g = ctx.createGain();
    osc.frequency.value = n % 4 === 0 ? 1760 : 1320;
    g.gain.setValueAtTime(0, t);
    g.gain.linearRampToValueAtTime(0.15, t + 0.002);
    g.gain.exponentialRampToValueAtTime(0.0001, t + 0.05);
    osc.connect(g).connect(ctx.destination);
    osc.start(t);
    osc.stop(t + 0.06);
  }
}

async function start() {
  startBtn.disabled = true;
  ctx = new AudioContext();
  await ctx.resume();
  const master = ctx.createGain();
  master.gain.value = Number(volume.value);
  master.connect(ctx.destination);
  volume.oninput = () => master.gain.setTargetAtTime(Number(volume.value), ctx.currentTime, 0.02);
  synth = new SineSynth(ctx, master);
  anchor = { state: { playing: false, bpm: bpm(), beat: 0, numerator: 4, denominator: 4, loop: null }, time: ctx.currentTime };

  const host: HostContext = {
    audioContext: ctx,
    apiVersion: 2,
    slotId: 'dev',
    transport: () => stateAt(ctx.currentTime),
    notify: (level, message) => log(`[${level}] ${message}`),
    markDirty: () => {},
    assetUrl: (path) => new URL(path, location.href).href,
  };

  try {
    plugin = await factory.create(host, $('plugin'));
  } catch (error) {
    log(`could not create the plugin: ${error}`);
    startBtn.disabled = false;
    return;
  }
  plugin.listen((e: PluginEvent) => {
    synth.handle(e);
    if (e.type === 'midi') {
      const [s, p, v] = e.bytes;
      const on = (s & 0xf0) === 0x90 && v > 0;
      log(`${e.time.toFixed(3)}  ${on ? 'on ' : 'off'} ${noteName(p).padEnd(4)} (${p})${on ? ' v' + v : ''}`, on ? 'on' : 'off');
    }
  });
  plugin.subscribe((c) => log(`param ${c.id} = ${c.value.toFixed(3)} (${c.source}${c.gesture ? ', ' + c.gesture : ''})`));

  createTicker(TICK_MS, clickTick);
  const showStats = () => {
    const beat = anchor.state.playing ? beatAt(ctx.currentTime, anchor) : null;
    stats.textContent = `${beat === null ? 'stopped' : 'beat ' + (beat + 1).toFixed(2)} · ${synth.active} voices`;
    requestAnimationFrame(showStats);
  };
  showStats();

  playBtn.disabled = false;
  saveBtn.disabled = false;
  startBtn.textContent = `Audio on · ${ctx.sampleRate} Hz`;
  log(`plugin ready · ${factory.manifest.name} ${factory.manifest.version}`);
}

// ?auto starts audio and plays at once (for headless smoke checks; browsers may block audio without a click).
if (new URLSearchParams(location.search).has('auto')) {
  start().then(() => plugin && play());
}

startBtn.onclick = start;
playBtn.onclick = play;
stopBtn.onclick = stop;
bpmInput.onchange = () => {
  if (!plugin || !anchor.state.playing) return;
  const time = ctx.currentTime + 0.05;
  send({ ...stateAt(time), bpm: bpm() }, time);
  log(`tempo ${bpm()} BPM at ${time.toFixed(3)}`);
};
saveBtn.onclick = async () => {
  const state = await plugin.getState();
  log(`getState: ${JSON.stringify(state.data)}`);
  await plugin.setState(JSON.parse(JSON.stringify(state)));
  log('setState done: the timeline and sound should not change');
};
window.addEventListener('keydown', (e) => {
  if (e.code !== 'Space' || !plugin || (e.target as HTMLElement)?.tagName === 'INPUT') return;
  e.preventDefault();
  if (anchor.state.playing) stop();
  else play();
});
