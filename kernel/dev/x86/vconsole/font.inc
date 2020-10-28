#pragma once

namespace unscii16 {
// http://pelulamu.net/unscii/, converted from .hex to array
// unscii-16 is licensed as Public Domain

constexpr inline int charHeight = 16;
constexpr inline int charWidth = 8;
constexpr inline int bytesPerChar = (charHeight * charWidth) / 8;

constexpr inline uint8_t fontData[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 00000
  0xe0, 0xe0, 0x80, 0x80, 0xea, 0xea, 0x2a, 0x2a, 0xee, 0xee, 0x0a, 0x0a, 0x0a, 0x0a, 0x00, 0x00, // 00001
  0xe0, 0xe0, 0x80, 0x80, 0xea, 0xea, 0x2a, 0x2a, 0xe4, 0xe4, 0x0a, 0x0a, 0x0a, 0x0a, 0x00, 0x00, // 00002
  0xe0, 0xe0, 0x80, 0x80, 0xca, 0xca, 0x8a, 0x8a, 0xe4, 0xe4, 0x0a, 0x0a, 0x0a, 0x0a, 0x00, 0x00, // 00003
  0xe0, 0xe0, 0x80, 0x80, 0xce, 0xce, 0x84, 0x84, 0xe4, 0xe4, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00, // 00004
  0xe0, 0xe0, 0x80, 0x80, 0xce, 0xce, 0x8a, 0x8a, 0xea, 0xea, 0x0e, 0x0e, 0x04, 0x04, 0x00, 0x00, // 00005
  0xe0, 0xe0, 0xa0, 0xa0, 0xea, 0xea, 0xaa, 0xaa, 0xac, 0xac, 0x0a, 0x0a, 0x0a, 0x0a, 0x00, 0x00, // 00006
  0xc0, 0xc0, 0xa0, 0xa0, 0xc8, 0xc8, 0xa8, 0xa8, 0xc8, 0xc8, 0x08, 0x08, 0x0e, 0x0e, 0x00, 0x00, // 00007
  0xc0, 0xc0, 0xa0, 0xa0, 0xce, 0xce, 0xa8, 0xa8, 0xce, 0xce, 0x02, 0x02, 0x0e, 0x0e, 0x00, 0x00, // 00008
  0xa0, 0xa0, 0xa0, 0xa0, 0xee, 0xee, 0xa4, 0xa4, 0xa4, 0xa4, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00, // 00009
  0x80, 0x80, 0x80, 0x80, 0x8e, 0x8e, 0x88, 0x88, 0xec, 0xec, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, // 0000A
  0xa0, 0xa0, 0xa0, 0xa0, 0xae, 0xae, 0xa4, 0xa4, 0x44, 0x44, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00, // 0000B
  0xe0, 0xe0, 0x80, 0x80, 0xce, 0xce, 0x88, 0x88, 0x8c, 0x8c, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, // 0000C
  0xe0, 0xe0, 0x80, 0x80, 0x8e, 0x8e, 0x8a, 0x8a, 0xee, 0xee, 0x0c, 0x0c, 0x0a, 0x0a, 0x00, 0x00, // 0000D
  0xe0, 0xe0, 0x80, 0x80, 0xee, 0xee, 0x2a, 0x2a, 0xea, 0xea, 0x0a, 0x0a, 0x0e, 0x0e, 0x00, 0x00, // 0000E
  0xe0, 0xe0, 0x80, 0x80, 0xee, 0xee, 0x24, 0x24, 0xe4, 0xe4, 0x04, 0x04, 0x0e, 0x0e, 0x00, 0x00, // 0000F
  0xc0, 0xc0, 0xa0, 0xa0, 0xa8, 0xa8, 0xa8, 0xa8, 0xc8, 0xc8, 0x08, 0x08, 0x0e, 0x0e, 0x00, 0x00, // 00010
  0xc0, 0xc0, 0xa0, 0xa0, 0xa4, 0xa4, 0xac, 0xac, 0xc4, 0xc4, 0x04, 0x04, 0x0e, 0x0e, 0x00, 0x00, // 00011
  0xc0, 0xc0, 0xa0, 0xa0, 0xae, 0xae, 0xa2, 0xa2, 0xce, 0xce, 0x08, 0x08, 0x0e, 0x0e, 0x00, 0x00, // 00012
  0xc0, 0xc0, 0xa0, 0xa0, 0xae, 0xae, 0xa2, 0xa2, 0xc6, 0xc6, 0x02, 0x02, 0x0e, 0x0e, 0x00, 0x00, // 00013
  0xc0, 0xc0, 0xa0, 0xa0, 0xaa, 0xaa, 0xaa, 0xaa, 0xce, 0xce, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, // 00014
  0xe0, 0xe0, 0xa0, 0xa0, 0xaa, 0xaa, 0xaa, 0xaa, 0xac, 0xac, 0x0a, 0x0a, 0x0a, 0x0a, 0x00, 0x00, // 00015
  0xe0, 0xe0, 0x80, 0x80, 0xea, 0xea, 0x2a, 0x2a, 0xee, 0xee, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00, // 00016
  0xe0, 0xe0, 0x80, 0x80, 0xcc, 0xcc, 0x8a, 0x8a, 0xec, 0xec, 0x0a, 0x0a, 0x0c, 0x0c, 0x00, 0x00, // 00017
  0xe0, 0xe0, 0x80, 0x80, 0x8e, 0x8e, 0x8a, 0x8a, 0xea, 0xea, 0x0a, 0x0a, 0x0a, 0x0a, 0x00, 0x00, // 00018
  0xe0, 0xe0, 0x80, 0x80, 0xca, 0xca, 0x8e, 0x8e, 0xea, 0xea, 0x0a, 0x0a, 0x0a, 0x0a, 0x00, 0x00, // 00019
  0x3c, 0x3c, 0x66, 0x66, 0x66, 0x66, 0x30, 0x30, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, // 0001A
  0xe0, 0xe0, 0x80, 0x80, 0xce, 0xce, 0x88, 0x88, 0xe8, 0xe8, 0x08, 0x08, 0x0e, 0x0e, 0x00, 0x00, // 0001B
  0xe0, 0xe0, 0x80, 0x80, 0xce, 0xce, 0x88, 0x88, 0x8e, 0x8e, 0x02, 0x02, 0x0e, 0x0e, 0x00, 0x00, // 0001C
  0xe0, 0xe0, 0x80, 0x80, 0xae, 0xae, 0xa8, 0xa8, 0xee, 0xee, 0x02, 0x02, 0x0e, 0x0e, 0x00, 0x00, // 0001D
  0xe0, 0xe0, 0xa0, 0xa0, 0xee, 0xee, 0xc8, 0xc8, 0xae, 0xae, 0x02, 0x02, 0x0e, 0x0e, 0x00, 0x00, // 0001E
  0xa0, 0xa0, 0xa0, 0xa0, 0xae, 0xae, 0xa8, 0xa8, 0xee, 0xee, 0x02, 0x02, 0x0e, 0x0e, 0x00, 0x00, // 0001F
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 00020
  0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, // 00021
  0x00, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 00022
  0x00, 0x00, 0x6c, 0x6c, 0x6c, 0xfe, 0x6c, 0x6c, 0x6c, 0xfe, 0x6c, 0x6c, 0x6c, 0x00, 0x00, 0x00, // 00023
  0x00, 0x18, 0x18, 0x3c, 0x66, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x66, 0x3c, 0x18, 0x18, 0x00, 0x00, // 00024
  0x00, 0x00, 0x06, 0xc6, 0xcc, 0xcc, 0x18, 0x18, 0x30, 0x30, 0x66, 0x66, 0xc6, 0xc0, 0x00, 0x00, // 00025
  0x00, 0x00, 0x38, 0x6c, 0x6c, 0x38, 0x30, 0x7a, 0xde, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, // 00026
  0x00, 0x18, 0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 00027
  0x00, 0x0c, 0x18, 0x18, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x18, 0x18, 0x0c, 0x00, 0x00, // 00028
  0x00, 0x30, 0x18, 0x18, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x18, 0x18, 0x30, 0x00, 0x00, // 00029
  0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x3c, 0xff, 0x3c, 0x66, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, // 0002A
  0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, // 0002B
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x18, 0x18, 0x30, 0x60, 0x00, // 0002C
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0002D
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, // 0002E
  0x03, 0x03, 0x06, 0x06, 0x0c, 0x0c, 0x18, 0x18, 0x30, 0x30, 0x60, 0x60, 0xc0, 0xc0, 0x00, 0x00, // 0002F
  0x00, 0x00, 0x38, 0x6c, 0xc6, 0xc6, 0xce, 0xd6, 0xe6, 0xc6, 0xc6, 0x6c, 0x38, 0x00, 0x00, 0x00, // 00030
  0x00, 0x00, 0x18, 0x38, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00, 0x00, 0x00, // 00031
  0x00, 0x00, 0x3c, 0x66, 0x66, 0x06, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x60, 0x7e, 0x00, 0x00, 0x00, // 00032
  0x00, 0x00, 0x3c, 0x66, 0x66, 0x06, 0x06, 0x1c, 0x06, 0x06, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, // 00033
  0x00, 0x00, 0x0c, 0x1c, 0x3c, 0x6c, 0xcc, 0xcc, 0xfe, 0x0c, 0x0c, 0x0c, 0x0c, 0x00, 0x00, 0x00, // 00034
  0x00, 0x00, 0x7e, 0x60, 0x60, 0x60, 0x7c, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, // 00035
  0x00, 0x00, 0x1c, 0x30, 0x60, 0x60, 0x7c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, // 00036
  0x00, 0x00, 0x7e, 0x06, 0x06, 0x06, 0x0c, 0x0c, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, // 00037
  0x00, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x76, 0x3c, 0x6e, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, // 00038
  0x00, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x66, 0x3e, 0x06, 0x06, 0x06, 0x0c, 0x38, 0x00, 0x00, 0x00, // 00039
  0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, // 0003A
  0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x38, 0x18, 0x18, 0x30, 0x60, 0x00, // 0003B
  0x00, 0x00, 0x00, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x00, 0x00, 0x00, 0x00, // 0003C
  0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0003D
  0x00, 0x00, 0x00, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x00, 0x00, 0x00, 0x00, // 0003E
  0x00, 0x3c, 0x66, 0x66, 0x06, 0x0c, 0x18, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, // 0003F
  0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xde, 0xde, 0xde, 0xdc, 0xc0, 0xc0, 0x7c, 0x00, 0x00, 0x00, // 00040
  0x00, 0x00, 0x18, 0x3c, 0x66, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, // 00041
  0x00, 0x00, 0x7c, 0x66, 0x66, 0x66, 0x6c, 0x78, 0x6c, 0x66, 0x66, 0x66, 0x7c, 0x00, 0x00, 0x00, // 00042
  0x00, 0x00, 0x3c, 0x66, 0x66, 0x60, 0x60, 0x60, 0x60, 0x60, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, // 00043
  0x00, 0x00, 0x78, 0x6c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x6c, 0x78, 0x00, 0x00, 0x00, // 00044
  0x00, 0x00, 0x7e, 0x60, 0x60, 0x60, 0x60, 0x7c, 0x60, 0x60, 0x60, 0x60, 0x7e, 0x00, 0x00, 0x00, // 00045
  0x00, 0x00, 0x7e, 0x60, 0x60, 0x60, 0x7c, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x00, 0x00, 0x00, // 00046
  0x00, 0x00, 0x3c, 0x66, 0x66, 0x60, 0x60, 0x6e, 0x66, 0x66, 0x66, 0x66, 0x3e, 0x00, 0x00, 0x00, // 00047
  0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, // 00048
  0x00, 0x00, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00, 0x00, 0x00, // 00049
  0x00, 0x00, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, // 0004A
  0x00, 0x00, 0xc6, 0xc6, 0xcc, 0xcc, 0xd8, 0xf0, 0xd8, 0xcc, 0xcc, 0xc6, 0xc6, 0x00, 0x00, 0x00, // 0004B
  0x00, 0x00, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7e, 0x00, 0x00, 0x00, // 0004C
  0x00, 0x00, 0xc6, 0xee, 0xee, 0xfe, 0xd6, 0xd6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, // 0004D
  0x00, 0x00, 0xc6, 0xc6, 0xe6, 0xe6, 0xf6, 0xfe, 0xde, 0xce, 0xce, 0xc6, 0xc6, 0x00, 0x00, 0x00, // 0004E
  0x00, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, // 0004F
  0x00, 0x00, 0x7c, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x60, 0x60, 0x60, 0x60, 0x60, 0x00, 0x00, 0x00, // 00050
  0x00, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x0c, 0x06, 0x00, // 00051
  0x00, 0x00, 0x7c, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x6c, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, // 00052
  0x00, 0x00, 0x3c, 0x66, 0x66, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, // 00053
  0x00, 0x00, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, // 00054
  0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, // 00055
  0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x3c, 0x18, 0x18, 0x00, 0x00, 0x00, // 00056
  0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xd6, 0xd6, 0xfe, 0xee, 0xee, 0xc6, 0x00, 0x00, 0x00, // 00057
  0x00, 0x00, 0xc3, 0xc3, 0x66, 0x3c, 0x18, 0x18, 0x18, 0x3c, 0x66, 0xc3, 0xc3, 0x00, 0x00, 0x00, // 00058
  0x00, 0x00, 0xc3, 0xc3, 0x66, 0x66, 0x3c, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, // 00059
  0x00, 0x00, 0x7e, 0x06, 0x06, 0x0c, 0x0c, 0x18, 0x30, 0x30, 0x60, 0x60, 0x7e, 0x00, 0x00, 0x00, // 0005A
  0x00, 0x3c, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3c, 0x00, 0x00, // 0005B
  0xc0, 0xc0, 0x60, 0x60, 0x30, 0x30, 0x18, 0x18, 0x0c, 0x0c, 0x06, 0x06, 0x03, 0x03, 0x00, 0x00, // 0005C
  0x00, 0x3c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x3c, 0x00, 0x00, // 0005D
  0x00, 0x10, 0x38, 0x6c, 0x6c, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0005E
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, // 0005F
  0x00, 0x18, 0x18, 0x0c, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 00060
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x06, 0x3e, 0x66, 0x66, 0x66, 0x3e, 0x00, 0x00, 0x00, // 00061
  0x00, 0x00, 0x60, 0x60, 0x60, 0x60, 0x7c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x00, 0x00, 0x00, // 00062
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3c, 0x00, 0x00, 0x00, // 00063
  0x00, 0x00, 0x06, 0x06, 0x06, 0x06, 0x3e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3e, 0x00, 0x00, 0x00, // 00064
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x66, 0x66, 0x7e, 0x60, 0x60, 0x3c, 0x00, 0x00, 0x00, // 00065
  0x00, 0x00, 0x1e, 0x30, 0x30, 0x30, 0x7e, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00, 0x00, // 00066
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3e, 0x06, 0x06, 0x7c, // 00067
  0x00, 0x00, 0x60, 0x60, 0x60, 0x60, 0x7c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, // 00068
  0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1e, 0x00, 0x00, 0x00, // 00069
  0x00, 0x00, 0x0c, 0x0c, 0x00, 0x00, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x78, // 0006A
  0x00, 0x00, 0x60, 0x60, 0x60, 0x60, 0x66, 0x66, 0x6c, 0x78, 0x6c, 0x66, 0x66, 0x00, 0x00, 0x00, // 0006B
  0x00, 0x00, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1e, 0x00, 0x00, 0x00, // 0006C
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xcc, 0xfe, 0xd6, 0xd6, 0xd6, 0xd6, 0xc6, 0x00, 0x00, 0x00, // 0006D
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, // 0006E
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, // 0006F
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x60, 0x60, 0x60, // 00070
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3e, 0x06, 0x06, 0x06, // 00071
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0x66, 0x66, 0x60, 0x60, 0x60, 0x60, 0x00, 0x00, 0x00, // 00072
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x60, 0x60, 0x3c, 0x06, 0x06, 0x7c, 0x00, 0x00, 0x00, // 00073
  0x00, 0x00, 0x00, 0x30, 0x30, 0x30, 0x7e, 0x30, 0x30, 0x30, 0x30, 0x30, 0x1e, 0x00, 0x00, 0x00, // 00074
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3e, 0x00, 0x00, 0x00, // 00075
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x00, 0x00, 0x00, // 00076
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc6, 0xc6, 0xd6, 0xd6, 0xd6, 0x7c, 0x6c, 0x00, 0x00, 0x00, // 00077
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc6, 0xc6, 0x6c, 0x38, 0x6c, 0xc6, 0xc6, 0x00, 0x00, 0x00, // 00078
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3e, 0x06, 0x06, 0x3c, // 00079
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x7e, 0x00, 0x00, 0x00, // 0007A
  0x00, 0x0e, 0x18, 0x18, 0x18, 0x18, 0x18, 0xf0, 0x18, 0x18, 0x18, 0x18, 0x18, 0x0e, 0x00, 0x00, // 0007B
  0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, // 0007C
  0x00, 0xe0, 0x30, 0x30, 0x30, 0x30, 0x30, 0x1e, 0x30, 0x30, 0x30, 0x30, 0x30, 0xe0, 0x00, 0x00, // 0007D
  0x00, 0x72, 0xd6, 0x9c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0007E
  0xc0, 0xc0, 0xa0, 0xa0, 0xae, 0xae, 0xa4, 0xa4, 0xc4, 0xc4, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00, // 0007F
  0xe0, 0xe0, 0xa0, 0xa0, 0xee, 0xee, 0xaa, 0xaa, 0xea, 0xea, 0x0a, 0x0a, 0x0e, 0x0e, 0x00, 0x00, // 00080
  0xe0, 0xe0, 0xa0, 0xa0, 0xe4, 0xe4, 0xac, 0xac, 0xe4, 0xe4, 0x04, 0x04, 0x0e, 0x0e, 0x00, 0x00, // 00081
  0xe0, 0xe0, 0xa0, 0xa0, 0xee, 0xee, 0xa2, 0xa2, 0xee, 0xee, 0x08, 0x08, 0x0e, 0x0e, 0x00, 0x00, // 00082
  0xe0, 0xe0, 0xa0, 0xa0, 0xee, 0xee, 0xa2, 0xa2, 0xee, 0xee, 0x02, 0x02, 0x0e, 0x0e, 0x00, 0x00, // 00083
  0xe0, 0xe0, 0x40, 0x40, 0x4e, 0x4e, 0x4a, 0x4a, 0xea, 0xea, 0x0a, 0x0a, 0x0a, 0x0a, 0x00, 0x00, // 00084
  0xe0, 0xe0, 0xa0, 0xa0, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0x08, 0x08, 0x0e, 0x0e, 0x00, 0x00, // 00085
  0xe0, 0xe0, 0x80, 0x80, 0xee, 0xee, 0x28, 0x28, 0xee, 0xee, 0x02, 0x02, 0x0e, 0x0e, 0x00, 0x00, // 00086
  0xe0, 0xe0, 0x80, 0x80, 0xce, 0xce, 0x88, 0x88, 0xee, 0xee, 0x02, 0x02, 0x0e, 0x0e, 0x00, 0x00, // 00087
  0xa0, 0xa0, 0xa0, 0xa0, 0xee, 0xee, 0xa8, 0xa8, 0xae, 0xae, 0x02, 0x02, 0x0e, 0x0e, 0x00, 0x00, // 00088
  0xa0, 0xa0, 0xa0, 0xa0, 0xe2, 0xe2, 0xa2, 0xa2, 0xa2, 0xa2, 0x0a, 0x0a, 0x0e, 0x0e, 0x00, 0x00, // 00089
  0xa0, 0xa0, 0xa0, 0xa0, 0xae, 0xae, 0xa8, 0xa8, 0x4e, 0x4e, 0x02, 0x02, 0x0e, 0x0e, 0x00, 0x00, // 0008A
  0xe0, 0xe0, 0xa0, 0xa0, 0xec, 0xec, 0x8a, 0x8a, 0x8a, 0x8a, 0x0a, 0x0a, 0x0c, 0x0c, 0x00, 0x00, // 0008B
  0xe0, 0xe0, 0xa0, 0xa0, 0xea, 0xea, 0x8a, 0x8a, 0x8a, 0x8a, 0x0a, 0x0a, 0x0e, 0x0e, 0x00, 0x00, // 0008C
  0xc0, 0xc0, 0xa0, 0xa0, 0xce, 0xce, 0xa4, 0xa4, 0xa4, 0xa4, 0x04, 0x04, 0x0e, 0x0e, 0x00, 0x00, // 0008D
  0xe0, 0xe0, 0x80, 0x80, 0xee, 0xee, 0x22, 0x22, 0xee, 0xee, 0x08, 0x08, 0x0e, 0x0e, 0x00, 0x00, // 0008E
  0xe0, 0xe0, 0x80, 0x80, 0xee, 0xee, 0x22, 0x22, 0xe6, 0xe6, 0x02, 0x02, 0x0e, 0x0e, 0x00, 0x00, // 0008F
  0xc0, 0xc0, 0xa0, 0xa0, 0xae, 0xae, 0xa8, 0xa8, 0xc8, 0xc8, 0x08, 0x08, 0x0e, 0x0e, 0x00, 0x00, // 00090
  0xe0, 0xe0, 0xa0, 0xa0, 0xe4, 0xe4, 0x8c, 0x8c, 0x84, 0x84, 0x04, 0x04, 0x0e, 0x0e, 0x00, 0x00, // 00091
  0xe0, 0xe0, 0xa0, 0xa0, 0xee, 0xee, 0x82, 0x82, 0x8e, 0x8e, 0x08, 0x08, 0x0e, 0x0e, 0x00, 0x00, // 00092
  0xe0, 0xe0, 0x80, 0x80, 0xee, 0xee, 0x28, 0x28, 0xec, 0xec, 0x08, 0x08, 0x0e, 0x0e, 0x00, 0x00, // 00093
  0xe0, 0xe0, 0x80, 0x80, 0x8e, 0x8e, 0x88, 0x88, 0xe8, 0xe8, 0x08, 0x08, 0x0e, 0x0e, 0x00, 0x00, // 00094
  0xa0, 0xa0, 0xe0, 0xe0, 0xea, 0xea, 0xaa, 0xaa, 0xae, 0xae, 0x0e, 0x0e, 0x0a, 0x0a, 0x00, 0x00, // 00095
  0xe0, 0xe0, 0x80, 0x80, 0xee, 0xee, 0x2a, 0x2a, 0xee, 0xee, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, // 00096
  0xe0, 0xe0, 0x80, 0x80, 0xce, 0xce, 0x8a, 0x8a, 0xee, 0xee, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, // 00097
  0xe0, 0xe0, 0xa0, 0xa0, 0xee, 0xee, 0x2a, 0x2a, 0xee, 0xee, 0x0a, 0x0a, 0x0e, 0x0e, 0x00, 0x00, // 00098
  0xe0, 0xe0, 0xa0, 0xa0, 0xee, 0xee, 0x2a, 0x2a, 0xee, 0xee, 0x02, 0x02, 0x0e, 0x0e, 0x00, 0x00, // 00099
  0xe0, 0xe0, 0xa0, 0xa0, 0xee, 0xee, 0x2a, 0x2a, 0xee, 0xee, 0x0a, 0x0a, 0x0a, 0x0a, 0x00, 0x00, // 0009A
  0xe0, 0xe0, 0x80, 0x80, 0x8e, 0x8e, 0x88, 0x88, 0xee, 0xee, 0x02, 0x02, 0x0e, 0x0e, 0x00, 0x00, // 0009B
  0xe0, 0xe0, 0x80, 0x80, 0xee, 0xee, 0x24, 0x24, 0xe4, 0xe4, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00, // 0009C
  0xe0, 0xe0, 0xa0, 0xa0, 0xae, 0xae, 0xa8, 0xa8, 0xee, 0xee, 0x02, 0x02, 0x0e, 0x0e, 0x00, 0x00, // 0009D
  0xe0, 0xe0, 0xa0, 0xa0, 0xea, 0xea, 0x8e, 0x8e, 0x8e, 0x8e, 0x0a, 0x0a, 0x0a, 0x0a, 0x00, 0x00, // 0009E
  0xe0, 0xe0, 0xa0, 0xa0, 0xee, 0xee, 0xaa, 0xaa, 0xae, 0xae, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, // 0009F
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 000A0
  0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, // 000A1
  0x00, 0x18, 0x18, 0x18, 0x3c, 0x66, 0x60, 0x60, 0x66, 0x3c, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, // 000A2
  0x00, 0x00, 0x38, 0x6c, 0x6c, 0x60, 0x60, 0xf0, 0x60, 0x60, 0x66, 0x66, 0xfc, 0x00, 0x00, 0x00, // 000A3
  0x00, 0x00, 0x66, 0x3c, 0x66, 0x66, 0x66, 0x3c, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 000A4
  0x00, 0x00, 0xc3, 0xc3, 0x66, 0x66, 0x3c, 0x18, 0x7e, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x00, 0x00, // 000A5
  0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, // 000A6
  0x00, 0x3c, 0x66, 0x60, 0x30, 0x38, 0x6c, 0x66, 0x36, 0x1c, 0x0c, 0x06, 0x66, 0x3c, 0x00, 0x00, // 000A7
  0x66, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 000A8
  0x00, 0x00, 0x3c, 0x42, 0x81, 0x9d, 0xb1, 0xb1, 0xb1, 0x9d, 0x81, 0x42, 0x3c, 0x00, 0x00, 0x00, // 000A9
  0x00, 0x3c, 0x06, 0x06, 0x3e, 0x66, 0x66, 0x3e, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 000AA
  0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0x66, 0xcc, 0x66, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 000AB
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x06, 0x06, 0x06, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, // 000AC
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 000AD
  0x00, 0x00, 0x3c, 0x42, 0x81, 0xb9, 0xa5, 0xb9, 0xa5, 0xa5, 0x81, 0x42, 0x3c, 0x00, 0x00, 0x00, // 000AE
  0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 000AF
  0x00, 0x3c, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 000B0
  0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x18, 0x00, 0x7e, 0x00, 0x00, 0x00, // 000B1
  0x00, 0x38, 0x6c, 0x0c, 0x18, 0x30, 0x60, 0x7c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 000B2
  0x00, 0x38, 0x6c, 0x0c, 0x38, 0x0c, 0x6c, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 000B3
  0x0c, 0x0c, 0x18, 0x18, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 000B4
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x60, 0x60, 0xc0, // 000B5
  0x00, 0x00, 0x3e, 0x7a, 0x7a, 0x7a, 0x7a, 0x3a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x00, 0x00, 0x00, // 000B6
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 000B7
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x06, 0x1c, // 000B8
  0x00, 0x30, 0x70, 0x30, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 000B9
  0x00, 0x00, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 000BA
  0x00, 0x00, 0x00, 0x00, 0x00, 0xcc, 0x66, 0x33, 0x66, 0xcc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 000BB
  0x00, 0x40, 0xc6, 0x46, 0x4c, 0x4c, 0x18, 0x18, 0x30, 0x30, 0x62, 0x66, 0xca, 0xcf, 0x02, 0x00, // 000BC
  0x00, 0x40, 0xc6, 0x46, 0x4c, 0x4c, 0x18, 0x18, 0x30, 0x30, 0x6c, 0x62, 0xc4, 0xc8, 0x0e, 0x00, // 000BD
  0x00, 0xc0, 0x26, 0x66, 0x2c, 0xcc, 0x18, 0x18, 0x30, 0x30, 0x62, 0x66, 0xca, 0xcf, 0x02, 0x00, // 000BE
  0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x30, 0x30, 0x60, 0x60, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, // 000BF
  0x30, 0x18, 0x00, 0x18, 0x3c, 0x66, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, // 000C0
  0x0c, 0x18, 0x00, 0x18, 0x3c, 0x66, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, // 000C1
  0x18, 0x66, 0x00, 0x18, 0x3c, 0x66, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, // 000C2
  0x76, 0xdc, 0x00, 0x18, 0x3c, 0x66, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, // 000C3
  0x66, 0x66, 0x00, 0x18, 0x3c, 0x66, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, // 000C4
  0x3c, 0x66, 0x3c, 0x00, 0x18, 0x3c, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, // 000C5
  0x00, 0x00, 0x3f, 0x7c, 0xfc, 0xcc, 0xcc, 0xfe, 0xcc, 0xcc, 0xcc, 0xcc, 0xcf, 0x00, 0x00, 0x00, // 000C6
  0x00, 0x00, 0x3c, 0x66, 0x66, 0x60, 0x60, 0x60, 0x60, 0x60, 0x66, 0x66, 0x3c, 0x0c, 0x06, 0x1c, // 000C7
  0x30, 0x18, 0x00, 0x7e, 0x60, 0x60, 0x60, 0x7c, 0x60, 0x60, 0x60, 0x60, 0x7e, 0x00, 0x00, 0x00, // 000C8
  0x0c, 0x18, 0x00, 0x7e, 0x60, 0x60, 0x60, 0x7c, 0x60, 0x60, 0x60, 0x60, 0x7e, 0x00, 0x00, 0x00, // 000C9
  0x18, 0x66, 0x00, 0x7e, 0x60, 0x60, 0x60, 0x7c, 0x60, 0x60, 0x60, 0x60, 0x7e, 0x00, 0x00, 0x00, // 000CA
  0x66, 0x66, 0x00, 0x7e, 0x60, 0x60, 0x60, 0x7c, 0x60, 0x60, 0x60, 0x60, 0x7e, 0x00, 0x00, 0x00, // 000CB
  0x30, 0x18, 0x00, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00, 0x00, 0x00, // 000CC
  0x0c, 0x18, 0x00, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00, 0x00, 0x00, // 000CD
  0x18, 0x66, 0x00, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00, 0x00, 0x00, // 000CE
  0x66, 0x66, 0x00, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00, 0x00, 0x00, // 000CF
  0x00, 0x00, 0x78, 0x6c, 0x66, 0x66, 0x66, 0xf6, 0x66, 0x66, 0x66, 0x6c, 0x78, 0x00, 0x00, 0x00, // 000D0
  0x76, 0xdc, 0x00, 0xc6, 0xc6, 0xe6, 0xf6, 0xde, 0xce, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, // 000D1
  0x30, 0x18, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, // 000D2
  0x0c, 0x18, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, // 000D3
  0x18, 0x66, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, // 000D4
  0x76, 0xdc, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, // 000D5
  0x66, 0x66, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, // 000D6
  0x00, 0x00, 0x00, 0x00, 0x82, 0xc6, 0x6c, 0x38, 0x6c, 0xc6, 0x82, 0x00, 0x00, 0x00, 0x00, 0x00, // 000D7
  0x00, 0x00, 0x3e, 0x66, 0x6e, 0x6e, 0x6e, 0x7e, 0x76, 0x76, 0x76, 0x66, 0x7c, 0x00, 0x00, 0x00, // 000D8
  0x30, 0x18, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, // 000D9
  0x0c, 0x18, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, // 000DA
  0x18, 0x66, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, // 000DB
  0x66, 0x66, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, // 000DC
  0x0c, 0x18, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, // 000DD
  0x00, 0x00, 0xc0, 0xc0, 0xfc, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xfc, 0xc0, 0xc0, 0x00, 0x00, 0x00, // 000DE
  0x00, 0x00, 0x78, 0xcc, 0xcc, 0xcc, 0xd8, 0xcc, 0xc6, 0xc6, 0xc6, 0xc6, 0xcc, 0x00, 0x00, 0x00, // 000DF
  0x00, 0x00, 0x30, 0x18, 0x00, 0x00, 0x3c, 0x06, 0x3e, 0x66, 0x66, 0x66, 0x3e, 0x00, 0x00, 0x00, // 000E0
  0x00, 0x00, 0x0c, 0x18, 0x00, 0x00, 0x3c, 0x06, 0x3e, 0x66, 0x66, 0x66, 0x3e, 0x00, 0x00, 0x00, // 000E1
  0x00, 0x18, 0x3c, 0x66, 0x00, 0x00, 0x3c, 0x06, 0x3e, 0x66, 0x66, 0x66, 0x3e, 0x00, 0x00, 0x00, // 000E2
  0x00, 0x00, 0x76, 0xdc, 0x00, 0x00, 0x3c, 0x06, 0x3e, 0x66, 0x66, 0x66, 0x3e, 0x00, 0x00, 0x00, // 000E3
  0x00, 0x00, 0x66, 0x66, 0x00, 0x00, 0x3c, 0x06, 0x3e, 0x66, 0x66, 0x66, 0x3e, 0x00, 0x00, 0x00, // 000E4
  0x00, 0x00, 0x3c, 0x66, 0x3c, 0x00, 0x3c, 0x06, 0x3e, 0x66, 0x66, 0x66, 0x3e, 0x00, 0x00, 0x00, // 000E5
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x1b, 0x1b, 0x7f, 0xd8, 0xd8, 0x77, 0x00, 0x00, 0x00, // 000E6
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3c, 0x0c, 0x06, 0x1c, // 000E7
  0x00, 0x00, 0x30, 0x18, 0x00, 0x00, 0x3c, 0x66, 0x66, 0x7e, 0x60, 0x60, 0x3c, 0x00, 0x00, 0x00, // 000E8
  0x00, 0x00, 0x0c, 0x18, 0x00, 0x00, 0x3c, 0x66, 0x66, 0x7e, 0x60, 0x60, 0x3c, 0x00, 0x00, 0x00, // 000E9
  0x00, 0x18, 0x3c, 0x66, 0x00, 0x00, 0x3c, 0x66, 0x66, 0x7e, 0x60, 0x60, 0x3c, 0x00, 0x00, 0x00, // 000EA
  0x00, 0x00, 0x66, 0x66, 0x00, 0x00, 0x3c, 0x66, 0x66, 0x7e, 0x60, 0x60, 0x3c, 0x00, 0x00, 0x00, // 000EB
  0x00, 0x00, 0x30, 0x18, 0x00, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, // 000EC
  0x00, 0x00, 0x0c, 0x18, 0x00, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, // 000ED
  0x00, 0x18, 0x3c, 0x66, 0x00, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, // 000EE
  0x00, 0x00, 0x66, 0x66, 0x00, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, // 000EF
  0x00, 0x36, 0x1c, 0x1c, 0x36, 0x06, 0x3e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, // 000F0
  0x00, 0x00, 0x76, 0xdc, 0x00, 0x00, 0x7c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, // 000F1
  0x00, 0x00, 0x30, 0x18, 0x00, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, // 000F2
  0x00, 0x00, 0x0c, 0x18, 0x00, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, // 000F3
  0x00, 0x18, 0x3c, 0x66, 0x00, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, // 000F4
  0x00, 0x00, 0x76, 0xdc, 0x00, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, // 000F5
  0x00, 0x00, 0x66, 0x66, 0x00, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, // 000F6
  0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x7e, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, // 000F7
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3d, 0x66, 0x6e, 0x7e, 0x76, 0x66, 0xbc, 0x00, 0x00, 0x00, // 000F8
  0x00, 0x00, 0x30, 0x18, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3e, 0x00, 0x00, 0x00, // 000F9
  0x00, 0x00, 0x0c, 0x18, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3e, 0x00, 0x00, 0x00, // 000FA
  0x00, 0x18, 0x3c, 0x66, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3e, 0x00, 0x00, 0x00, // 000FB
  0x00, 0x00, 0x66, 0x66, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3e, 0x00, 0x00, 0x00, // 000FC
  0x00, 0x00, 0x0c, 0x18, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3e, 0x06, 0x06, 0x3c, // 000FD
  0x00, 0x00, 0x00, 0x60, 0x60, 0x60, 0x7c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x60, 0x60, 0x60, // 000FE
  0x00, 0x00, 0x66, 0x66, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3e, 0x06, 0x06, 0x3c, // 000FF
};

}