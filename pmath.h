/* pmath.h -- portable minimal math library
 *
 * Standard : C99 (int32_t/uint32_t required; union type-pun defined by C99)
 * Targets  : Z80, AVR, 6809, RP2040, and other 8/16-bit systems
 *
 * Companion to ascii_io.h.  Include ascii_io.h before this header so that
 * the PORTABLE_TYPES_DEFINED sentinel (see below) resolves correctly.
 *
 * -----------------------------------------------------------------------
 * FEATURE DROP FLAGS  (define before including to shrink the binary)
 * -----------------------------------------------------------------------
 *  PMATH_NO_TRIG    -- omit sin/cos (saves ~200 bytes ROM in fixed mode)
 *  PMATH_NO_SQRT    -- omit sqrt
 *  PMATH_NO_STDINT    -- omit <stdint.h>; you supply the fallback typedefs
 *                        (see PORTABLE_TYPES_DEFINED note below)
 *  PMATH_NO_FP_CONST -- replace all float literals in constant definitions
 *                        with pre-computed Q8.8 integer literals.
 *                        Use when the toolchain has no float type at all
 *                        (e.g. a bare 6809 compiler with no float subsystem).
 *                        Only valid in fixed-point mode (no PMATH_USE_FLOAT).
 *                        PM_F() becomes PM_RAW() -- caller supplies the
 *                        pre-scaled integer value directly.
 *                        Only correct for PMATH_FRAC_BITS=8 (Q8.8).
 *                        For other FRAC_BITS supply your own constants.
 *
 * -----------------------------------------------------------------------
 * MODE FLAG
 * -----------------------------------------------------------------------
 *  PMATH_USE_FLOAT  -- use 32-bit IEEE 754 float instead of fixed-point
 *
 *  FLOAT NOTE: unlike ascii_io.h where float is ON by default and dropped
 *  with ASCII_NO_FLOAT, here fixed-point is ON by default and float is
 *  opt-IN.  This is intentional: for 8-bit math, fixed-point is the right
 *  default.  Float mode is provided for targets (e.g. RP2040) where the
 *  hardware or ROM makes it cheap.
 *
 *  FLOAT INTERACTION WITH ascii_io.h:
 *    PMATH_USE_FLOAT requires ascii_put_f32() for pm_print() to work.
 *    Do NOT define ASCII_NO_FLOAT when PMATH_USE_FLOAT is active -- this
 *    header will emit a #error if both are set simultaneously.
 *
 *    In fixed-point mode (PMATH_USE_FLOAT not set), pm_print() uses pure
 *    integer arithmetic and ASCII_NO_FLOAT is completely safe to define.
 *
 * -----------------------------------------------------------------------
 * FIXED-POINT CONFIGURATION
 * -----------------------------------------------------------------------
 *  PMATH_FRAC_BITS n  -- fractional bits (default: 8)
 *                        8  -> Q8.8   range +-127.996  res ~0.0039
 *                        12 -> Q4.12  range +-7.9998   res ~0.00024
 *                        4  -> Q12.4  range +-2047.9   res ~0.0625
 *
 * -----------------------------------------------------------------------
 * NAMING NOTES
 * -----------------------------------------------------------------------
 *  pm_ftoi / pm_itof : "f" = current format (fixed or float), not float.
 *  All angle arguments are radians expressed in the current num_t format.
 *
 * -----------------------------------------------------------------------
 * K&R / C89 PORTING NOTE
 * -----------------------------------------------------------------------
 *  This library requires C99 minimum.  It uses int32_t / uint32_t from
 *  <stdint.h> in its inline arithmetic, and the float sqrt relies on
 *  union type-punning which is defined behaviour only in C99 (section
 *  6.5.2.3).  If your toolchain predates C99, define PMATH_NO_SQRT and
 *  supply your own sqrt, and avoid the pm_deg2rad / pm_rad2deg helpers.
 *  All other fixed-point arithmetic compiles cleanly under strict C89 if
 *  you also define PMATH_NO_STDINT and provide the typedefs manually.
 *
 * -----------------------------------------------------------------------
 * QUICK START
 * -----------------------------------------------------------------------
 *  #include "ascii_io.h"   // always first -- resolves shared sentinel
 *  #include "pmath.h"
 *
 *  num_t a = PM_F(1.5);            // float literal -> num_t
 *  num_t b = pm_mul(a, PM_PI);     // fixed: shifts correctly; float: *
 *  num_t s = pm_sin(PM_HALF_PI);   // 1.0 in current format
 *  pm_print(s, 4);                 // prints "1.0000" in either mode
 */

#ifndef PMATH_H
#define PMATH_H

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * FLOAT / STDINT CONFLICT GUARD
 * Catch the invalid combination before anything else is processed.
 * ----------------------------------------------------------------------- */
#if defined(PMATH_USE_FLOAT) && defined(ASCII_NO_FLOAT)
#  error "PMATH_USE_FLOAT requires ascii_put_f32(): do not define ASCII_NO_FLOAT."
#  error "In fixed-point mode, pm_print() needs no float support -- ASCII_NO_FLOAT is safe."
#endif

/* -----------------------------------------------------------------------
 * INTEGER TYPES
 *
 * PORTABLE_TYPES_DEFINED is a shared sentinel used by both ascii_io.h and
 * pmath.h.  Whichever header is included first defines the types; the
 * second skips its block entirely, preventing redefinition errors when
 * both ASCII_NO_STDINT and PMATH_NO_STDINT are set in the same build.
 *
 * Recommended include order: ascii_io.h first, then pmath.h.
 * pmath.h needs int32_t / uint32_t which ascii_io.h does not define in
 * its manual fallback (ascii_io is 8/16-bit only).  If pmath.h is
 * included first without stdint.h, the manual block here covers all six
 * types.  If ascii_io.h is first with its manual block, pmath.h will not
 * redefine types -- ensure int32_t / uint32_t are provided some other way.
 * ----------------------------------------------------------------------- */
#ifndef PORTABLE_TYPES_DEFINED
#  define PORTABLE_TYPES_DEFINED
#  ifndef PMATH_NO_STDINT
#    include <stdint.h>
#  else
     /* Adjust for your platform if the defaults are wrong.              */
     /* int32_t / uint32_t are required for fixed-point intermediates    */
     /* and for the float sqrt bit-manipulation.                         */
     typedef unsigned char   uint8_t;
     typedef signed   char   int8_t;
     typedef unsigned short  uint16_t;
     typedef signed   short  int16_t;
     typedef unsigned long   uint32_t;  /* must be exactly 32 bits       */
     typedef signed   long   int32_t;   /* must be exactly 32 bits       */
#  endif
#endif

/* -----------------------------------------------------------------------
 * INLINE PORTABILITY
 * ----------------------------------------------------------------------- */

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#  define PM_INLINE static inline
#else
#  define PM_INLINE static
#endif

/* -----------------------------------------------------------------------
 * FIXED-POINT CONFIGURATION
 * ----------------------------------------------------------------------- */

#ifndef PMATH_FRAC_BITS
#  define PMATH_FRAC_BITS  8
#endif

#define PM_SCALE      ((int16_t)(1 << PMATH_FRAC_BITS))
#define PM_FRAC_MASK  ((int16_t)(PM_SCALE - 1))

/* =======================================================================
 * TYPES
 * ======================================================================= */

typedef int16_t fixed_t;

#ifdef PMATH_USE_FLOAT
  typedef float num_t;
#else
  typedef fixed_t num_t;
#endif

/* =======================================================================
 * CONSTANTS  (in the current num_t format)
 * ======================================================================= */

#ifdef PMATH_USE_FLOAT
#  define PM_ZERO      0.0f
#  define PM_HALF      0.5f
#  define PM_ONE       1.0f
#  define PM_TWO       2.0f
#  define PM_PI        3.14159265f
#  define PM_TWO_PI    6.28318530f
#  define PM_HALF_PI   1.57079632f
#  define PM_SQRT2     1.41421356f
#  define PM_INV_SQRT2 0.70710678f
#elif defined(PMATH_NO_FP_CONST)
/* Pre-computed Q8.8 integer literals -- zero float tokens.
 * Use when the toolchain has no float type at all.
 * Values are only correct for PMATH_FRAC_BITS=8.                        */
#  define PM_ZERO      ((fixed_t)   0)   /* 0.0                          */
#  define PM_HALF      ((fixed_t) 128)   /* 0.5    * 256                 */
#  define PM_ONE       ((fixed_t) 256)   /* 1.0    * 256                 */
#  define PM_TWO       ((fixed_t) 512)   /* 2.0    * 256                 */
#  define PM_PI        ((fixed_t) 804)   /* 3.14159 * 256 = 804.25 -> 804 */
#  define PM_TWO_PI    ((fixed_t)1608)   /* 6.28318 * 256 = 1608.5 -> 1608 */
#  define PM_HALF_PI   ((fixed_t) 402)   /* 1.57079 * 256 = 402.12 -> 402  */
#  define PM_SQRT2     ((fixed_t) 362)   /* 1.41421 * 256 = 362.03 -> 362  */
#  define PM_INV_SQRT2 ((fixed_t) 181)   /* 0.70710 * 256 = 181.02 -> 181  */
#else
/* All constants resolve at compile time via float literals (C99).       */
#  define PM_ZERO      ((fixed_t)0)
#  define PM_HALF      ((fixed_t)(PM_SCALE >> 1))
#  define PM_ONE       ((fixed_t)(PM_SCALE))
#  define PM_TWO       ((fixed_t)(PM_SCALE << 1))
#  define PM_PI        ((fixed_t)((int32_t)(3.14159265f * PM_SCALE + 0.5f)))
#  define PM_TWO_PI    ((fixed_t)((int32_t)(6.28318530f * PM_SCALE + 0.5f)))
#  define PM_HALF_PI   ((fixed_t)((int32_t)(1.57079632f * PM_SCALE + 0.5f)))
#  define PM_SQRT2     ((fixed_t)((int32_t)(1.41421356f * PM_SCALE + 0.5f)))
#  define PM_INV_SQRT2 ((fixed_t)((int32_t)(0.70710678f * PM_SCALE + 0.5f)))
#endif

/* =======================================================================
 * LITERAL HELPER
 *
 * PM_F(x) converts a float constant to num_t.
 *
 * Float mode:  plain cast, no overhead.
 * Fixed mode:  float multiply at compile time -- requires float literals.
 * No-float mode (PMATH_NO_FP_CONST): PM_F() is disabled.  Use PM_RAW(x)
 *   instead and supply the pre-scaled Q8.8 integer directly.
 *   e.g.  PM_F(3.5f)  -->  PM_RAW(896)   (3.5 * 256 = 896)
 *         PM_F(0.25f) -->  PM_RAW(64)    (0.25 * 256 = 64)
 *
 * Use for initialisation only -- not inside inner loops on 8-bit targets.
 * Rounding is half-away-from-zero for positive and negative values.
 * ======================================================================= */

#ifdef PMATH_USE_FLOAT
#  define PM_F(x)    ((num_t)(x))
#  define PM_RAW(x)  ((num_t)(x))
#elif defined(PMATH_NO_FP_CONST)
/* Float literals are not available -- PM_F() would silently truncate.
 * PM_RAW takes the pre-scaled integer value directly.                    */
#  define PM_RAW(x)  ((fixed_t)(x))
/* PM_F is intentionally not defined.  If your code uses PM_F() and
 * PMATH_NO_FP_CONST is set you will get a compile error, which is the
 * correct behaviour -- replace each PM_F() call with PM_RAW().           */
#else
#  define PM_F(x)    ((num_t)((int32_t)((x) * PM_SCALE + ((x) < 0.0f ? -0.5f : 0.5f))))
#  define PM_RAW(x)  ((fixed_t)(x))
#endif

/* =======================================================================
 * MULTIPLY / DIVIDE
 *
 * Fixed: 32-bit intermediate keeps the binary point correct.
 * Float: plain C operators.
 * Use these macros so the same source compiles in both modes.
 * ======================================================================= */

#ifdef PMATH_USE_FLOAT
#  define pm_mul(a, b)  ((num_t)((a) * (b)))
#  define pm_div(a, b)  ((num_t)((a) / (b)))
#else
#  define pm_mul(a, b)  ((fixed_t)(((int32_t)(a) * (int32_t)(b)) >> PMATH_FRAC_BITS))
#  define pm_div(a, b)  ((fixed_t)(((int32_t)(a) << PMATH_FRAC_BITS) / (int32_t)(b)))
#endif

/* =======================================================================
 * CONVERSIONS
 * ======================================================================= */

/* num_t -> int16_t  (truncates toward zero in float mode;
 *                    floors in fixed mode via arithmetic right-shift)     */
#ifdef PMATH_USE_FLOAT
#  define pm_ftoi(x)  ((int16_t)(x))
#  define pm_itof(x)  ((num_t)(int16_t)(x))
#else
#  define pm_ftoi(x)  ((int16_t)((fixed_t)(x) >> PMATH_FRAC_BITS))
#  define pm_itof(x)  ((num_t)((fixed_t)(int16_t)(x) << PMATH_FRAC_BITS))
#endif

/* num_t <-> float  (debug / init bridge; avoid in inner loops on 8-bit) */
#ifdef PMATH_USE_FLOAT
#  define pm_to_float(x)    ((float)(x))
#  define pm_from_float(x)  ((num_t)(x))
#else
#  define pm_to_float(x)    ((float)(x) / (float)PM_SCALE)
#  define pm_from_float(x)  ((fixed_t)((x) * PM_SCALE + 0.5f))
#endif

/* =======================================================================
 * BASIC ARITHMETIC
 * ======================================================================= */

PM_INLINE num_t pm_abs(num_t x)  { return x < PM_ZERO ? (num_t)-x : x; }
PM_INLINE num_t pm_min(num_t a, num_t b) { return a < b ? a : b; }
PM_INLINE num_t pm_max(num_t a, num_t b) { return a > b ? a : b; }
PM_INLINE int   pm_sign(num_t x) { return x > PM_ZERO ? 1 : (x < PM_ZERO ? -1 : 0); }

PM_INLINE num_t pm_clamp(num_t x, num_t lo, num_t hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

/* =======================================================================
 * FLOOR / CEIL / ROUND
 *
 * Fixed: bit-masking, correct for negative two's-complement values.
 * Float: integer cast method, no libm dependency.
 * ======================================================================= */

#ifdef PMATH_USE_FLOAT
PM_INLINE num_t pm_floor(num_t x) {
    int16_t i = (int16_t)x;
    return (num_t)(i - (x < (num_t)i ? 1 : 0));
}
PM_INLINE num_t pm_ceil(num_t x) {
    int16_t i = (int16_t)x;
    return (num_t)(i + (x > (num_t)i ? 1 : 0));
}
#else
/* Mask off fractional bits -> floor. */
PM_INLINE num_t pm_floor(num_t x) {
    return (fixed_t)(x & (fixed_t)~PM_FRAC_MASK);
}
/* Add (SCALE-1) before masking: carries into the integer part if frac != 0. */
PM_INLINE num_t pm_ceil(num_t x) {
    return (fixed_t)((x + PM_FRAC_MASK) & (fixed_t)~PM_FRAC_MASK);
}
#endif

PM_INLINE num_t pm_round(num_t x) { return pm_floor(x + PM_HALF); }

/* =======================================================================
 * LINEAR INTERPOLATION
 *
 * pm_lerp(a, b, t) = a + t*(b-a),  t in [0, 1].
 * Fixed: one 32-bit multiply per call.
 * ======================================================================= */

PM_INLINE num_t pm_lerp(num_t a, num_t b, num_t t) {
    return (num_t)(a + pm_mul(b - a, t));
}

/* =======================================================================
 * ANGLE HELPERS
 * ======================================================================= */

/* Wrap radians to [0, 2*pi).
 * Subtraction loop avoids division -- suitable when angles are already
 * near-range, as is typical in animation / rotation on small systems.   */
PM_INLINE num_t pm_wrap_angle(num_t x) {
    while (x < PM_ZERO)    x = (num_t)(x + PM_TWO_PI);
    while (x >= PM_TWO_PI) x = (num_t)(x - PM_TWO_PI);
    return x;
}

/* Degrees <-> radians.
 *
 * Fixed-point range note:
 *   deg2rad: input limited by Q format (Q8.8 max ~127 degrees).
 *   rad2deg: output limited; results above the format max will wrap.
 *            For Q8.8 this means angles above ~127 degrees overflow.
 *            Use PMATH_USE_FLOAT for full 0-360 degree arithmetic.
 *
 * Multiplying by the small constant pi/180 directly is avoided: in Q8.8
 * it rounds to 4/256 = 0.01562, a 10% error.  Instead both functions
 * compute via PM_PI which is already rounded correctly for the format.  */

#ifdef PMATH_USE_FLOAT
#  define pm_deg2rad(d)  ((d) * 0.01745329f)
#  define pm_rad2deg(r)  ((r) * 57.2957795f)
#else
PM_INLINE num_t pm_deg2rad(num_t d) {
    return (fixed_t)(((int32_t)d * (int32_t)PM_PI)
                     / (int32_t)(180 * (uint32_t)PM_SCALE));
}
PM_INLINE num_t pm_rad2deg(num_t r) {
    return (fixed_t)(((int32_t)r * (int32_t)((uint32_t)180 * PM_SCALE))
                     / (int32_t)PM_PI);
}
#endif

/* =======================================================================
 * FUNCTION DECLARATIONS
 * ======================================================================= */

/*
 * pm_print(v, decimals) -- print a num_t value via ascii_io.
 *
 * Float mode:  delegates to ascii_put_f32(v, decimals).
 *              ASCII_NO_FLOAT must not be set (enforced by #error above).
 *
 * Fixed mode:  digit-by-digit integer arithmetic only.
 *              Does NOT use ascii_put_f32; ASCII_NO_FLOAT is safe.
 *              Precision is limited by the Q format (~2.4 significant
 *              decimal digits for Q8.8; requesting more prints correctly
 *              but the last digits will be zero-noise below the LSB).
 *              Range note: INT16_MIN (-32768) negated is UB; values at
 *              the format minimum should be avoided or clamped by caller.
 */
void pm_print(num_t v, uint8_t decimals);

#ifndef PMATH_NO_SQRT
num_t pm_sqrt(num_t x);
#endif

#ifndef PMATH_NO_TRIG
num_t pm_sin(num_t x);
num_t pm_cos(num_t x);
#endif

#ifdef __cplusplus
}
#endif

#endif /* PMATH_H */
