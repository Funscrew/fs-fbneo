// Version number, written as  vV.V.BB  or  vV.V.BBaa
// (0xVVBBaa, in BCD notation)

// YES!  You can actually cause an access violation if you don't pick the right combination of numbers here!  WOW!
#define VER_MAJOR  0
#define VER_MINOR  3
#define VER_BETA  00
#define VER_ALPHA 3

#define BURN_VERSION (VER_MAJOR * 0x100000) + (VER_MINOR * 0x010000) + (((VER_BETA / 10) * 0x001000) + ((VER_BETA % 10) * 0x000100)) + (((VER_ALPHA / 10) * 0x000010) + (VER_ALPHA % 10))

#define FS_VERSION 02
