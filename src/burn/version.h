// Version number, written as  vV.V.BB  or  vV.V.BBaa
// (0xVVBBaa, in BCD notation)

#define VER_MAJOR  0
#define VER_MINOR  4
#define VER_REVISION 12345

#define VER_BETA  00
#define VER_ALPHA 0

static const int VER_GGPO = 2;
constexpr unsigned int FS_VER = VER_MAJOR << 24 | VER_MINOR << 16 | VER_REVISION;

// OBSOLETE:  This will be phased out at some point.
// The issue is that some of the program uses BURN_VERSION to version data files as well.  Data format versions + program versions are not the same thing, so I want to disambiguate them at some point.
#define BURN_VERSION (VER_MAJOR * 0x100000) + (VER_MINOR * 0x010000) + (((VER_BETA / 10) * 0x001000) + ((VER_BETA % 10) * 0x000100)) + (((VER_ALPHA / 10) * 0x000010) + (VER_ALPHA % 10))
