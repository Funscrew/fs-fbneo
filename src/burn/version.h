// Version number, written as  vV.V.BB  or  vV.V.BBaa
// (0xVVBBaa, in BCD notation)

// YES!  You can actually crash the program if you don't pick the right combination of numbers here!  WOW!
// TODO: The version of the emulator should not be used for control flow internally.
// File/Format versions for config should, but NOT the version of the emulator seen here!
#define VER_MAJOR  0
#define VER_MINOR  4
#define VER_BETA  00
#define VER_ALPHA 0

#define BURN_VERSION (VER_MAJOR * 0x100000) + (VER_MINOR * 0x010000) + (((VER_BETA / 10) * 0x001000) + ((VER_BETA % 10) * 0x000100)) + (((VER_ALPHA / 10) * 0x000010) + (VER_ALPHA % 10))

#define FS_VERSION 03
