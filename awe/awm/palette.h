#pragma once

#include "types.h"

struct Palette {
    const Colour borderColour{255, 255, 255};
    const Colour clientColour{128, 128, 128};
    const Colour backgroundColour{150, 150, 220};

    const Colour focusHeaderColour{0, 0, 255};
    const Colour noFocusHeaderColour{0, 0, 63};
};
