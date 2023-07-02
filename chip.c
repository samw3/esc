
#include "tracker.h"
#include "chip.h"

extern ChipInterface chip_p1xl;
extern ChipInterface chip_lft;
extern ChipInterface chip_bv;
ChipInterface *chips[] = {&chip_p1xl, &chip_lft, &chip_bv, NULL};

#define DELAY_SIZE (512)
#define FEEDBACK (0.6f)
#define EXPAND

ChipSample chip_expandSample(ChipSample _sample) {
    static s16 delay[DELAY_SIZE];
    static int pos = 0;
    ChipSample out;
    out.left = (_sample.left >> 1) + (delay[(pos + (DELAY_SIZE >> 1)) & (DELAY_SIZE - 1)] >> 1);
    out.right = (_sample.right >> 1) + (delay[pos & (DELAY_SIZE - 1)] >> 1);
    int oldPos = (pos + DELAY_SIZE - 1) & (DELAY_SIZE - 1);
    delay[oldPos] = (delay[oldPos] * (FEEDBACK / 2.0f)) + (((_sample.left + _sample.right) >> 1) * FEEDBACK);
    pos++;
    return out;
}

