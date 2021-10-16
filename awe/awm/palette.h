#pragma once

#include "awe/types.h"

struct Palette {
    const awe::Colour borderColour{255, 255, 255};
    const awe::Colour clientColour{128, 128, 128};
    const awe::Colour backgroundColour{150, 150, 220};

    const awe::Colour focusHeaderColour{0, 0, 255};
    const awe::Colour noFocusHeaderColour{0, 0, 63};
};
