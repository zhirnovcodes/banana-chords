import type { Event } from '../core/event';

/** 3 - Frame view: the device frame. Its one control is the silicone SHUFFLE button. */
export interface FrameView {
  readonly shuffleClicked: Event<void>;
}
