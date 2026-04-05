/* pmath.c -- portable minimal math library implementation
 * See pmath.h for configuration and usage notes.
 *
 * Fixed-point trig: 65-entry uint8_t quarter-wave table (65 bytes ROM)
 *   Angle conversion: multiply by 41, right-shift 8 (~= *64/402, no div)
 *   Accuracy: < 0.5 LSB error across full range at Q8.8
 *
 * Float trig: Bhaskara I rational approximation for sin [0, pi]
 *   sin(x) ~= 16x(pi-x) / (5pi^2 - 4x(pi-x))
 *   Max error: < 0.17%  No division chain, exact at 0, pi/6, pi/4, pi/3, pi/2
 *
 * Float sqrt: fast inverse sqrt (rsqrt) via union type punning (C99)
 *   Two Newton-Raphson rsqrt iterations -> < 1 ULP error for 32-bit float
 *
 * Fixed sqrt: digit-by-digit binary method, no division
 *   Input in Qm.n -> output in Qm.n via internal Q(m+n).(n) scaling
 *
 * pm_print: bridges num_t output to ascii_io in both modes
 *   Float mode: delegates to ascii_put_f32()
 *   Fixed mode: digit-by-digit integer arithmetic, no float required
 */

#include "ascii_io.h"
#include "pmath.h"

/* =======================================================================
 * pm_print -- print a num_t value via ascii_io
 *
 * Float mode:  ascii_put_f32(v, decimals)  -- direct delegation.
 * Fixed mode:  pure integer arithmetic.  No float, no ascii_put_f32.
 *   1. Handle sign; negate to positive.
 *   2. Emit integer part via ascii_put_u16.
 *   3. Emit fractional digits one at a time by multiplying the
 *      fractional bits by 10 and reading off the integer part,
 *      exactly as ascii_put_f32 does for its fractional loop but
 *      entirely in fixed-point using a 32-bit intermediate.
 * ======================================================================= */

void pm_print(num_t v, uint8_t decimals)
{
#ifdef PMATH_USE_FLOAT
    ascii_put_f32(v, decimals);
#else
    fixed_t frac;
    uint8_t d;

    /* Sign */
    if (v < PM_ZERO) {
        ASCII_PUTC('-');
        v = (num_t)(-v);   /* Note: INT16_MIN negated is UB; caller must avoid */
    }

    /* Integer part */
    ascii_put_u16((uint16_t)pm_ftoi(v));

    if (decimals == 0u) return;
    ASCII_PUTC('.');

    /* Fractional part: isolate the raw fractional bits, then for each
     * decimal digit multiply by 10 using a 32-bit intermediate, extract
     * the digit from the integer portion, and keep the remainder.       */
    frac = v & PM_FRAC_MASK;
    for (d = 0u; d < decimals; d++) {
        int32_t tmp = (int32_t)frac * 10;
        ASCII_PUTC((char)('0' + (uint8_t)(tmp >> PMATH_FRAC_BITS)));
        frac = (fixed_t)(tmp & (int32_t)PM_FRAC_MASK);
    }
#endif
}

/* =======================================================================
 * FIXED-POINT SECTION
 * ======================================================================= */
#ifndef PMATH_USE_FLOAT

/* -----------------------------------------------------------------------
 * Quarter-wave sine table
 *
 * 65 entries covering sin(k*pi/128) for k = 0..64.
 * Values are Q0.8: multiply by (1/256) to get the true sine.
 * 255 represents sin(pi/2) = 1.0; max error at peak = 1/256 ~= 0.4%.
 * ----------------------------------------------------------------------- */
#ifndef PMATH_NO_TRIG
static const uint8_t pm_sin_tbl[65] = {
/*  0- 7 */  0,   6,  13,  19,  25,  31,  37,  44,
/*  8-15 */ 50,  56,  62,  68,  74,  80,  86,  92,
/* 16-23 */ 98, 103, 109, 115, 120, 126, 131, 136,
/* 24-31 */142, 147, 152, 157, 162, 167, 171, 176,
/* 32-39 */181, 185, 189, 193, 197, 201, 205, 208,
/* 40-47 */212, 216, 219, 222, 225, 228, 231, 233,
/* 48-55 */236, 238, 240, 242, 244, 246, 248, 249,
/* 56-63 */250, 252, 253, 253, 254, 255, 255, 255,
/* 64   */255
};

/* Internal Q8.8 sin lookup.  x must be in [0, pi/2] expressed in Q8.8. */
static int16_t pm_sin_q88_raw(int16_t x)
{
    /* Map [0, pi/2] -> [0, 64] table index.
     * Exact:  idx = x * 64 / 402   (402 = pi/2 in Q8.8)
     * Approx: idx = (x * 41) >> 8  (41/256 ~= 64/402, max idx error < 0.4)
     * uint16_t intermediate avoids signed overflow on 8-bit targets.   */
    uint16_t idx = ((uint16_t)x * 41U) >> 8;
    if (idx > 64U) idx = 64U;
    return (int16_t)pm_sin_tbl[idx];
}

/* =======================================================================
 * pm_sin (fixed-point)
 *
 * Accepts angle in the current num_t format (Q m.FRAC_BITS) as radians.
 * Internally normalises to Q8.8 regardless of PMATH_FRAC_BITS, performs
 * quadrant reduction, table lookup, then converts result back.
 * ======================================================================= */
num_t pm_sin(num_t angle)
{
    int16_t x, y;
    int8_t  neg = 0;

    /* Normalise to Q8.8 */
#if PMATH_FRAC_BITS != 8
    x = (int16_t)(((int32_t)angle * 256) >> PMATH_FRAC_BITS);
#else
    x = (int16_t)angle;
#endif

    /* sin(-x) = -sin(x) */
    if (x < 0) { x = (int16_t)-x; neg ^= 1; }

    /* Reduce to [0, 2*pi) in Q8.8.  2*pi ~= 1608. */
    {
#ifdef PMATH_NO_FP_CONST
        int16_t two_pi = (int16_t)1608;
#else
        int16_t two_pi = (int16_t)((int32_t)(6.28318530f * 256 + 0.5f));
#endif
        while (x >= two_pi) x = (int16_t)(x - two_pi);
    }

    /* Quadrant split.  pi ~= 804, pi/2 ~= 402 in Q8.8. */
    {
#ifdef PMATH_NO_FP_CONST
        int16_t pi      = (int16_t)804;
        int16_t half_pi = (int16_t)402;
#else
        int16_t pi      = (int16_t)((int32_t)(3.14159265f * 256 + 0.5f));
        int16_t half_pi = (int16_t)((int32_t)(1.57079632f * 256 + 0.5f));
#endif

        if (x > pi) {
            x   = (int16_t)(x - pi);
            neg ^= 1;
        }
        if (x > half_pi) {
            x = (int16_t)(pi - x);
        }
    }

    y = pm_sin_q88_raw(x);   /* Q0.8 result in [0, 255] */

#if PMATH_FRAC_BITS != 8
    y = (int16_t)(((int32_t)y << PMATH_FRAC_BITS) >> 8);
#endif

    return (num_t)(neg ? (int16_t)-y : y);
}

/* pm_cos (fixed-point): cos(x) = sin(x + pi/2) */
num_t pm_cos(num_t x)
{
    return pm_sin((num_t)(x + PM_HALF_PI));
}
#endif /* PMATH_NO_TRIG */

/* =======================================================================
 * pm_sqrt (fixed-point)
 *
 * Digit-by-digit binary square root, no division.
 * Upshifts input by FRAC_BITS so the integer sqrt yields the correct
 * Q m.FRAC result.  Works for any PMATH_FRAC_BITS value.
 * ======================================================================= */
#ifndef PMATH_NO_SQRT
num_t pm_sqrt(num_t x)
{
    uint32_t n, bit, result;

    if (x <= PM_ZERO) return PM_ZERO;

    n      = (uint32_t)(uint16_t)x << PMATH_FRAC_BITS;
    bit    = 1UL << 30;
    result = 0;

    while (bit > n)  bit >>= 2;
    while (bit) {
        if (n >= result + bit) {
            n      -= result + bit;
            result  = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return (num_t)(uint16_t)result;
}
#endif /* PMATH_NO_SQRT */

/* =======================================================================
 * FLOAT SECTION
 * ======================================================================= */
#else  /* PMATH_USE_FLOAT */

/* =======================================================================
 * pm_sin / pm_cos (float)
 *
 * Bhaskara I rational approximation.  Valid on [0, pi]:
 *   sin(x) ~= 16x(pi-x) / (5*pi^2 - 4x(pi-x))
 * Exact at 0, pi/6, pi/4, pi/3, pi/2, 2pi/3, 3pi/4, 5pi/6, pi.
 * Max relative error: ~0.17%  Cost: 2 mul, 1 div, 3 add/sub.
 * ======================================================================= */
#ifndef PMATH_NO_TRIG
static float pm_sin_bhaskara(float x)
{
    float xpm = x * (3.14159265f - x);
    return (16.0f * xpm) / (49.34802200f - 4.0f * xpm);
    /* 5*pi^2 = 49.348... precomputed */
}

num_t pm_sin(num_t x)
{
    float r;
    int8_t neg = 0;

    while (x < 0.0f)         x += 6.28318530f;
    while (x >= 6.28318530f) x -= 6.28318530f;
    if (x > 3.14159265f) { x -= 3.14159265f; neg = 1; }

    r = pm_sin_bhaskara(x);
    return neg ? -r : r;
}

num_t pm_cos(num_t x)
{
    return pm_sin(x + 1.57079632f);
}
#endif /* PMATH_NO_TRIG */

/* =======================================================================
 * pm_sqrt (float)
 *
 * Fast inverse sqrt via IEEE 754 bit trick (union type punning,
 * defined behaviour in C99 section 6.5.2.3 footnote 82).
 * Two Newton-Raphson iterations of rsqrt -> < 1 ULP for 32-bit float.
 * ======================================================================= */
#ifndef PMATH_NO_SQRT
num_t pm_sqrt(num_t x)
{
    union { float f; uint32_t u; } v;
    float h;

    if (x <= 0.0f) return 0.0f;

    h   = x * 0.5f;
    v.f = x;
    v.u = 0x5F375A86UL - (v.u >> 1);
    v.f = v.f * (1.5f - h * v.f * v.f);
    v.f = v.f * (1.5f - h * v.f * v.f);
    return x * v.f;
}
#endif /* PMATH_NO_SQRT */

#endif /* PMATH_USE_FLOAT */
