/** A continuous control moved (a slider handle). `value` is the control position 0..1. */
export interface Edit {
  value: number;
  /** 'begin' on grab, 'end' on release: the host records one automation gesture in between. */
  gesture?: 'begin' | 'end';
}
