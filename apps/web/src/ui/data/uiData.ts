// 2 - UI data: what the screens are showing. Plain data, no behaviour: MainUiModel owns it.
//
// Figma Frame 4 has two states of the main screen:
//   screen 1: NOTE, SCALE, LOOP, LEN, DRIFT, NOTES, DENSITY
//   screen 2: the SCALE list is open over the right column (LEN, NOTES hidden)

export interface MainViewData {
  /** The SCALE drop-down is open (screen 2). */
  scaleListOpen: boolean;
  /** First visible row of the SCALE list. */
  scaleListScroll: number;
}

export function initialMainViewData(): MainViewData {
  return { scaleListOpen: false, scaleListScroll: 0 };
}
