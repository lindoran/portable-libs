/* portable_types.h -- integer type definitions for embedded targets
 *
 * Standard : C89/C90 compatible
 * Targets  : Z80, AVR, 6809, RP2040, and other 8/16/32-bit systems
 *
 * Include this header first in every translation unit, or let any of
 * the companion library headers (ascii_io.h, pmath.h, cbuf.h) pull it
 * in automatically.  Only the first inclusion has any effect.
 *
 * -----------------------------------------------------------------------
 * NORMAL USE
 * -----------------------------------------------------------------------
 *  On any toolchain with a conforming <stdint.h> (GCC, Clang, SDCC,
 *  Z88DK, avr-gcc, ia16-gcc, CMOC, MSVC 2010+) just include this header
 *  and all types are available.  No flags needed.
 *
 *    #include "portable_types.h"
 *
 * -----------------------------------------------------------------------
 * FLAG
 * -----------------------------------------------------------------------
 *  PORTABLE_NO_STDINT -- do not include <stdint.h>.  Instead, the manual
 *                        fallback typedefs below are used.  Define this
 *                        only when the toolchain genuinely lacks stdint.h.
 *                        Adjust the fallback typedefs for your platform
 *                        if the sizes below are wrong.
 *
 * -----------------------------------------------------------------------
 * TYPES PROVIDED
 * -----------------------------------------------------------------------
 *  uint8_t   uint16_t   uint32_t   -- unsigned integers, exact width
 *  int8_t    int16_t    int32_t    -- signed integers, exact width
 *
 *  int32_t and uint32_t are included here so that this single header
 *  covers every type needed across ascii_io, pmath, and cbuf.  ascii_io
 *  and cbuf use only 8/16-bit types at runtime; pmath uses 32-bit for
 *  fixed-point multiply/divide intermediates and the float sqrt bit trick.
 *
 * -----------------------------------------------------------------------
 * PORTING NOTE
 * -----------------------------------------------------------------------
 *  If PORTABLE_NO_STDINT is required on your platform, verify the sizes:
 *    uint8_t   must be exactly  8 bits (unsigned char  on all targets)
 *    uint16_t  must be exactly 16 bits (unsigned short on most targets)
 *    uint32_t  must be exactly 32 bits (unsigned long  on most 8/16-bit
 *              targets; unsigned int on 32-bit targets)
 *
 *  On targets where unsigned long is 64 bits (LP64 Linux, Win64) the
 *  manual fallback will be wrong -- but those targets have stdint.h, so
 *  PORTABLE_NO_STDINT should not be set there.
 *
 * -----------------------------------------------------------------------
 * NULL
 * -----------------------------------------------------------------------
 *  NULL is also defined here if not already present, avoiding a
 *  dependency on <stddef.h> across the library family.
 */

#ifndef PORTABLE_TYPES_H
#define PORTABLE_TYPES_H

#ifndef PORTABLE_NO_STDINT
#  include <stdint.h>
#else

/* -----------------------------------------------------------------------
 * Manual fallback typedefs.
 * Sizes assume a typical 8 or 16-bit target.
 * Adjust uint32_t / int32_t if unsigned long is not 32 bits.
 * ----------------------------------------------------------------------- */
typedef unsigned char   uint8_t;
typedef signed   char   int8_t;
typedef unsigned short  uint16_t;
typedef signed   short  int16_t;
typedef unsigned long   uint32_t;   /* must be exactly 32 bits */
typedef signed   long   int32_t;    /* must be exactly 32 bits */

#endif /* PORTABLE_NO_STDINT */

/* -----------------------------------------------------------------------
 * NULL -- defined here so no library header needs stddef.h
 * ----------------------------------------------------------------------- */
#ifndef NULL
#  ifdef __cplusplus
#    define NULL 0
#  else
#    define NULL ((void *)0)
#  endif
#endif

#endif /* PORTABLE_TYPES_H */
