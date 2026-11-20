// The test synth for the dev page: one sine per note, short fades so nothing clicks.

import { noteToHz, type PluginEvent } from 'ivandaw-core';

const ATTACK = 0.01;
const RELEASE = 0.08;
const VOICE_LEVEL = 0.1;

type Voice = { osc: OscillatorNode; gain: GainNode };

export class SineSynth {
  private voices = new Map<number, Voice>();

  constructor(
    private ctx: AudioContext,
    private out: AudioNode,
  ) {}

  /** Plays a midi PluginEvent at its time. Other events are ignored. */
  handle(e: PluginEvent) {
    if (e.type !== 'midi') return;
    const [status, pitch, velocity] = e.bytes;
    const time = Math.max(e.time, this.ctx.currentTime);
    const type = status & 0xf0;
    if (type === 0x90 && velocity > 0) this.noteOn(pitch, velocity, time);
    else if (type === 0x80 || type === 0x90) this.noteOff(pitch, time);
    else if (type === 0xb0 && (pitch === 120 || pitch === 123)) this.allOff(time);
  }

  noteOn(pitch: number, velocity: number, time: number) {
    this.noteOff(pitch, time);
    const osc = this.ctx.createOscillator();
    osc.type = 'sine';
    osc.frequency.value = noteToHz(pitch);
    const gain = this.ctx.createGain();
    gain.gain.setValueAtTime(0, time);
    gain.gain.linearRampToValueAtTime(VOICE_LEVEL * (velocity / 127), time + ATTACK);
    osc.connect(gain).connect(this.out);
    osc.start(time);
    const voice = { osc, gain };
    osc.onended = () => {
      gain.disconnect();
      if (this.voices.get(pitch) === voice) this.voices.delete(pitch);
    };
    this.voices.set(pitch, voice);
  }

  noteOff(pitch: number, time: number) {
    const v = this.voices.get(pitch);
    if (!v) return;
    this.voices.delete(pitch);
    v.gain.gain.cancelScheduledValues(time);
    v.gain.gain.setTargetAtTime(0, time, RELEASE / 4);
    v.osc.stop(time + RELEASE * 2);
  }

  allOff(time = this.ctx.currentTime) {
    for (const pitch of [...this.voices.keys()]) this.noteOff(pitch, time);
  }

  get active() {
    return this.voices.size;
  }
}
