/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _TYPES_H
#define _TYPES_H
/*
 * Keep the compiler happy
 */

/*
 * Disable specific compiler warnings
 *   4018 - '<' : signed/unsigned mismatch
 *   4100 - 'xxx' : unreferenced formal parameter
 *   4127 - conditional expression is constant
 *   4201 - nonstandard extension used : nameless struct/union
 *   4389 - '!=' : signed/unsigned mismatch
 *   4800 - 'int' : forcing value to bool 'true' or 'false' (performance warning)
 */
#pragma warning(disable: 4018 4100 4127 4201 4389 4800)

/*
 * Simple types
 */
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned char byte;
typedef unsigned long ulong;
typedef char int8;
typedef short int16;
typedef int int32;

/*
 * Additional headers
 */
#if defined(_WINDOWS)
#  include "platform_windows.h"
#elif defined(__GNUC__)
#  include "platform_linux.h"
#else
#  error Unsupported platform
#endif

#include "log.h"



/*
 * Macros
 */
// NOTE: This assert will always 'abort' and is not meant to be disabled in Release builds.
#define ASSERT(condition)                                   \
   do {                                                     \
      if (!(condition)) {                                   \
         char assert_buf[1024];                             \
         snprintf(assert_buf, sizeof(assert_buf) - 1, "Assertion: %s @ %s:%d (pid:%d)", #condition, __FILE__, __LINE__, Platform::GetProcessID()); \
         Utils::LogIt(CATEGORY_ERROR, assert_buf);          \
         Platform::AssertFailed(assert_buf);                \
         exit(0);                                           \
      }                                                     \
   } while (false)

#ifndef ARRAY_SIZE
#  define ARRAY_SIZE(a)    (sizeof(a) / sizeof((a)[0]))
#endif

#ifndef MAX_INT
#  define MAX_INT          0xEFFFFFF
#endif

#ifndef MAX
#  define MAX(x, y)        (((x) > (y)) ? (x) : (y))
#endif

#ifndef MIN
#  define MIN(x, y)        (((x) < (y)) ? (x) : (y))
#endif


// Useful consts:
static const int UDP_HEADER_SIZE = 28;     /* Size of IP + UDP headers */


#endif // _TYPES_H
