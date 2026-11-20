import { Emitter } from '../core/event';
import type { FrameView } from '../views/frameView';
import { place } from './widgets';

/**
 * The device frame: the SHUFFLE silicone button. A transparent <button> sits on the drawn pad
 * (face 107 x 20 at 17,136); while held, the pad's top (face + label) sinks onto its side.
 */
export class FrameDomView implements FrameView {
  readonly shuffleClicked = new Emitter<void>();
  readonly el: HTMLButtonElement;

  constructor(doc: Document, svg: SVGSVGElement) {
    const e = doc.createElement('button');
    e.type = 'button';
    e.className = 'bc-shuffle';
    e.setAttribute('aria-label', 'Shuffle');
    place(e, { x: 17, y: 136, w: 107, h: 23 });
    const down = (on: boolean) => svg.classList.toggle('bc-shuffle-down', on);
    e.addEventListener('pointerdown', () => down(true));
    for (const type of ['pointerup', 'pointerleave', 'pointercancel', 'blur']) e.addEventListener(type, () => down(false));
    e.addEventListener('keydown', (event) => (event.key === ' ' || event.key === 'Enter') && down(true));
    e.addEventListener('keyup', () => down(false));
    e.addEventListener('click', () => this.shuffleClicked.emit());
    this.el = e;
  }
}
