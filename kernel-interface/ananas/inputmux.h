/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __ANANAS_INPUTMUX_H__
#define __ANANAS_INPUTMUX_H__

#include <ananas/types.h>

#define AIMX_KEY_NONE       0
#define AIMX_KEY_TAB        '\t' // 9
#define AIMX_KEY_ENTER      '\r' // 13
#define AIMX_KEY_ESCAPE     27
#define AIMX_KEY_SPACE      ' ' // 32
#define AIMX_KEY_0          '0' // 48
#define AIMX_KEY_1          '1'
#define AIMX_KEY_2          '2'
#define AIMX_KEY_3          '3'
#define AIMX_KEY_4          '4'
#define AIMX_KEY_5          '5'
#define AIMX_KEY_6          '6'
#define AIMX_KEY_7          '7'
#define AIMX_KEY_8          '8'
#define AIMX_KEY_9          '9' // 57
#define AIMX_KEY_A          'A' // 65
#define AIMX_KEY_B          'B'
#define AIMX_KEY_C          'C'
#define AIMX_KEY_D          'D'
#define AIMX_KEY_E          'E'
#define AIMX_KEY_F          'F'
#define AIMX_KEY_G          'G'
#define AIMX_KEY_H          'H'
#define AIMX_KEY_I          'I'
#define AIMX_KEY_J          'J'
#define AIMX_KEY_K          'K'
#define AIMX_KEY_L          'L'
#define AIMX_KEY_M          'M'
#define AIMX_KEY_N          'N'
#define AIMX_KEY_O          'O'
#define AIMX_KEY_P          'P'
#define AIMX_KEY_Q          'Q'
#define AIMX_KEY_R          'R'
#define AIMX_KEY_S          'S'
#define AIMX_KEY_T          'T'
#define AIMX_KEY_U          'U'
#define AIMX_KEY_V          'V'
#define AIMX_KEY_W          'W'
#define AIMX_KEY_X          'X'
#define AIMX_KEY_Y          'Y'
#define AIMX_KEY_Z          'Z' // 90

#define AIMX_KEY_F1         128
#define AIMX_KEY_F2         129
#define AIMX_KEY_F3         130
#define AIMX_KEY_F4         131
#define AIMX_KEY_F5         132
#define AIMX_KEY_F6         133
#define AIMX_KEY_F7         134
#define AIMX_KEY_F8         135
#define AIMX_KEY_F9         136
#define AIMX_KEY_F10        137
#define AIMX_KEY_F11        138
#define AIMX_KEY_F12        139

#define AIMX_KEY_LEFT_ARROW     140
#define AIMX_KEY_RIGHT_ARROW    141
#define AIMX_KEY_UP_ARROW       142
#define AIMX_KEY_DOWN_ARROW     143
#define AIMX_KEY_LEFT_CONTROL   150
#define AIMX_KEY_LEFT_SHIFT     151
#define AIMX_KEY_LEFT_ALT       152
#define AIMX_KEY_LEFT_GUI       153

#define AIMX_KEY_RIGHT_CONTROL  160
#define AIMX_KEY_RIGHT_SHIFT    161
#define AIMX_KEY_RIGHT_ALT      162
#define AIMX_KEY_RIGHT_GUI      163

#define AIMX_KEY_HOME           164
#define AIMX_KEY_END            165
#define AIMX_KEY_INSERT         166
#define AIMX_KEY_DELETE         167
#define AIMX_KEY_PAGE_UP        168
#define AIMX_KEY_PAGE_DOWN      169

typedef int AIMX_KEY_CODE;

#define AIMX_MOD_LEFT_CONTROL   (1 << 0)
#define AIMX_MOD_LEFT_SHIFT     (1 << 1)
#define AIMX_MOD_LEFT_ALT       (1 << 2)
#define AIMX_MOD_LEFT_GUI       (1 << 3)
#define AIMX_MOD_RIGHT_CONTROL  (1 << 4)
#define AIMX_MOD_RIGHT_SHIFT    (1 << 5)
#define AIMX_MOD_RIGHT_ALT      (1 << 6)
#define AIMX_MOD_RIGHT_GUI      (1 << 7)

typedef int AIMX_MODIFIERS;

enum AIMX_EVENT_TYPE {
    AIMX_EVENT_KEY_DOWN,
    AIMX_EVENT_KEY_UP,
    AIMX_EVENT_MOUSE,
};

struct AIMX_EVENT_KEY {
    AIMX_KEY_CODE key;
    AIMX_MODIFIERS mods;
};

#define AIMX_BUTTON_LEFT (1 << 0)
#define AIMX_BUTTON_RIGHT (1 << 1)
#define AIMX_BUTTON_MIDDLE (1 << 2)

struct AIMX_EVENT_MOUSE_INFO {
    int button;
    int delta_x;
    int delta_y;
};

struct AIMX_EVENT {
    AIMX_EVENT_TYPE type;
    union {
        struct AIMX_EVENT_KEY key_down;
        struct AIMX_EVENT_KEY key_up;
        struct AIMX_EVENT_MOUSE_INFO mouse;
    } u;
};

#endif /* __ANANAS_KBDMUX_H__ */
