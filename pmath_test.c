/* pmath_test.c -- validation for pmath + ascii_io integration
 *
 * Build Q8.8 fixed:       gcc -std=c99 -Wall pmath_test.c pmath.c ascii_io.c -o t_fixed
 * Build float mode:       gcc -std=c99 -Wall -DPMATH_USE_FLOAT pmath_test.c pmath.c ascii_io.c -o t_float
 * Build no float at all:  gcc -std=c99 -Wall -DASCII_NO_FLOAT -DPMATH_NO_FP_CONST pmath_test.c pmath.c ascii_io.c -o t_nofp
 * Build no trig/sqrt:     gcc -std=c99 -Wall -DASCII_NO_FLOAT -DPMATH_NO_TRIG -DPMATH_NO_SQRT pmath_test.c pmath.c ascii_io.c -o t_bare
 * ERROR (conflict):       gcc -std=c99 -DPMATH_USE_FLOAT -DASCII_NO_FLOAT pmath_test.c pmath.c ascii_io.c
 */

#include "ascii_io.h"
#include "pmath.h"
#include <stdio.h>

void  ascii_putc(char c) { putchar((unsigned char)c); }
char  ascii_getc(void)   { return (char)getchar(); }

static void nl(void)            { ascii_nl(); }
static void label(const char *s){ ascii_puts(s); }

/* -----------------------------------------------------------------------
 * Test constants -- in PMATH_NO_FP_CONST mode all values must be supplied
 * as pre-scaled Q8.8 integers.  PM_F() is intentionally undefined.
 * In all other modes PM_F() handles the conversion.
 * ----------------------------------------------------------------------- */
#ifdef PMATH_NO_FP_CONST
  /* Pre-scaled Q8.8 raw values (value * 256, rounded) */
  #define N_35    PM_RAW( 896)   /* 3.5    */
  #define N_125   PM_RAW( 320)   /* 1.25   */
  #define N_15    PM_RAW( 384)   /* 1.5    */
  #define N_N15   PM_RAW(-384)   /* -1.5   */
  #define N_50    PM_RAW(1280)   /* 5.0    */
  #define N_10    PM_ONE         /* 1.0    */
  #define N_40    PM_RAW(1024)   /* 4.0    */
  #define N_37    PM_RAW( 947)   /* 3.7    */
  #define N_32    PM_RAW( 819)   /* 3.2    */
  #define N_26    PM_RAW( 666)   /* 2.6    */
  #define N_24    PM_RAW( 614)   /* 2.4    */
  #define N_39    PM_RAW( 998)   /* 3.9    */
  #define N_N19   PM_RAW(-486)   /* -1.9   */
  #define N_N35   PM_RAW(-896)   /* -3.5   */
  #define N_45    PM_RAW(11520)  /* 45.0   */
  #define N_90    PM_RAW(23040)  /* 90.0   */
  #define N_127   PM_RAW(32512)  /* 127.0  */
  #define N_20    PM_TWO         /* 2.0    */
  #define N_90Q   PM_RAW(23040)  /* 9.0 -- overflows Q8.8 int part; use sqrt test differently */
  #define N_12    PM_RAW( 307)   /* 1.2    */
  #define N_PI6   PM_RAW( 134)   /* pi/6   */
  #define N_PI4   PM_RAW( 201)   /* pi/4   */
#else
  #define N_35    PM_F( 3.5f)
  #define N_125   PM_F( 1.25f)
  #define N_15    PM_F( 1.5f)
  #define N_N15   PM_F(-1.5f)
  #define N_50    PM_F( 5.0f)
  #define N_10    PM_ONE
  #define N_40    PM_F( 4.0f)
  #define N_37    PM_F( 3.7f)
  #define N_32    PM_F( 3.2f)
  #define N_26    PM_F( 2.6f)
  #define N_24    PM_F( 2.4f)
  #define N_39    PM_F( 3.9f)
  #define N_N19   PM_F(-1.9f)
  #define N_N35   PM_F(-3.5f)
  #define N_45    PM_F(45.0f)
  #define N_90    PM_F(90.0f)
  #define N_127   PM_F(127.0f)
  #define N_20    PM_TWO
  #define N_90Q   PM_F( 9.0f)
  #define N_12    PM_F( 1.2f)
  #define N_PI6   PM_F(0.5235988f)
  #define N_PI4   PM_F(0.7853982f)
#endif

/* =======================================================================
 * pm_print -- key synergy test
 * ======================================================================= */
static void test_pm_print(void)
{
    label("-- pm_print (key synergy) --------------------------------"); nl();
    label("  pm_print(PI, 4)    = "); pm_print(PM_PI,    4u); nl();
    label("  pm_print(SQRT2, 4) = "); pm_print(PM_SQRT2, 4u); nl();
    label("  pm_print(1.5, 3)   = "); pm_print(N_15,     3u); nl();
    label("  pm_print(-1.5, 3)  = "); pm_print(N_N15,    3u); nl();
    label("  pm_print(0, 2)     = "); pm_print(PM_ZERO,  2u); nl();
    label("  pm_print(127, 0)   = "); pm_print(N_127,    0u); nl();
#ifndef PMATH_NO_TRIG
    { num_t s = pm_sin(PM_HALF_PI);
      label("  sin(pi/2) = "); pm_print(s, 4u); nl(); }
#endif
#ifndef PMATH_NO_SQRT
    { num_t r = pm_sqrt(PM_TWO);
      label("  sqrt(2)   = "); pm_print(r, 4u); nl(); }
#endif
}

/* =======================================================================
 * Basic arithmetic
 * ======================================================================= */
static void test_basic(void)
{
    num_t a = N_35;
    num_t b = N_125;
    label("-- Basic arithmetic --------------------------------------"); nl();
    label("  mul(3.5, 1.25) = "); pm_print(pm_mul(a, b),                       4u); nl();
    label("  div(3.5, 1.25) = "); pm_print(pm_div(a, b),                       4u); nl();
    label("  abs(-3.5)      = "); pm_print(pm_abs(N_N35),                      4u); nl();
    label("  clamp(5, 1, 4) = "); pm_print(pm_clamp(N_50, PM_ONE, N_40),       4u); nl();
    label("  lerp(0,1,0.5)  = "); pm_print(pm_lerp(PM_ZERO, PM_ONE, PM_HALF),  4u); nl();
    label("  floor(3.7)     = "); pm_print(pm_floor(N_37),                     4u); nl();
    label("  floor(-1.5)    = "); pm_print(pm_floor(N_N15),                    4u); nl();
    label("  ceil(-1.5)     = "); pm_print(pm_ceil(N_N15),                     4u); nl();
    label("  round(2.6)     = "); pm_print(pm_round(N_26),                     4u); nl();
    label("  round(2.4)     = "); pm_print(pm_round(N_24),                     4u); nl();
}

/* =======================================================================
 * Conversions
 * ======================================================================= */
static void test_conv(void)
{
    label("-- Conversions -------------------------------------------"); nl();
    label("  itof(7)       = "); pm_print(pm_itof(7), 4u);               nl();
    label("  ftoi(3.9)     = "); ascii_put_i16(pm_ftoi(N_39));            nl();
    label("  ftoi(-1.9)    = "); ascii_put_i16(pm_ftoi(N_N19));           nl();
    label("  sign(PI)      = "); ascii_put_i16((int16_t)pm_sign(PM_PI));  nl();
    label("  deg2rad(45)   = "); pm_print(pm_deg2rad(N_45),  4u);         nl();
    label("  deg2rad(90)   = "); pm_print(pm_deg2rad(N_90),  4u);         nl();
    label("  rad2deg(pi/4) = "); pm_print(pm_rad2deg(PM_HALF_PI), 4u);    nl();
}

/* =======================================================================
 * Sqrt
 * ======================================================================= */
#ifndef PMATH_NO_SQRT
static void test_sqrt(void)
{
    label("-- pm_sqrt -----------------------------------------------"); nl();
    label("  sqrt(1.0)  = "); pm_print(pm_sqrt(PM_ONE),  4u); nl();
    label("  sqrt(2.0)  = "); pm_print(pm_sqrt(PM_TWO),  4u); nl();
    label("  sqrt(4.0)  = "); pm_print(pm_sqrt(N_40),    4u); nl();
    label("  sqrt(0.25) = "); pm_print(pm_sqrt(PM_HALF), 4u); nl();
    /* sqrt(9.0) skipped in NO_FP_CONST mode: 9.0 overflows Q8.8 integer part */
#ifndef PMATH_NO_FP_CONST
    label("  sqrt(9.0)  = "); pm_print(pm_sqrt(N_90Q),   4u); nl();
#endif
}
#endif

/* =======================================================================
 * Trig
 * ======================================================================= */
#ifndef PMATH_NO_TRIG
static void test_trig(void)
{
    label("-- pm_sin/cos --------------------------------------------"); nl();
    label("  sin(0)     = "); pm_print(pm_sin(PM_ZERO),    4u); nl();
    label("  sin(pi/6)  = "); pm_print(pm_sin(N_PI6),      4u); nl();
    label("  sin(pi/4)  = "); pm_print(pm_sin(N_PI4),      4u); nl();
    label("  sin(pi/2)  = "); pm_print(pm_sin(PM_HALF_PI), 4u); nl();
    label("  sin(pi)    = "); pm_print(pm_sin(PM_PI),      4u); nl();
    label("  sin(-pi/2) = "); pm_print(pm_sin((num_t)-PM_HALF_PI), 4u); nl();
    label("  cos(0)     = "); pm_print(pm_cos(PM_ZERO),    4u); nl();
    label("  cos(pi/2)  = "); pm_print(pm_cos(PM_HALF_PI), 4u); nl();
    label("  cos(pi)    = "); pm_print(pm_cos(PM_PI),      4u); nl();
    { num_t s = pm_sin(N_12);
      num_t c = pm_cos(N_12);
      num_t id = (num_t)(pm_mul(s,s) + pm_mul(c,c));
      label("  sin^2+cos^2(1.2) = "); pm_print(id, 4u);
      label("  (expect ~1.0)"); nl(); }
}
#endif

/* =======================================================================
 * Hex output -- ascii_io standalone
 * ======================================================================= */
static void test_hex(void)
{
    label("-- ascii_io hex ------------------------------------------"); nl();
    label("  put_x8(0xAB)  = "); ascii_put_x8(0xABu);   nl();
    label("  put_x16p(val) = "); ascii_put_x16p(0x1234u); nl();
}

/* =======================================================================
 * Banner
 * ======================================================================= */
static void print_banner(void)
{
    ascii_puts("pmath + ascii_io integration test"); nl();
#ifdef PMATH_USE_FLOAT
    ascii_puts("  Mode       : FLOAT (32-bit IEEE 754)"); nl();
#else
    ascii_puts("  Mode       : FIXED Q");
    ascii_put_u8((uint8_t)(16 - PMATH_FRAC_BITS));
    ascii_putc('.');
    ascii_put_u8((uint8_t)PMATH_FRAC_BITS);
    ascii_puts("  (int16_t)"); nl();
#endif
#ifdef PMATH_NO_FP_CONST
    ascii_puts("  FP consts  : integer literals (PMATH_NO_FP_CONST)"); nl();
#endif
#ifdef ASCII_NO_FLOAT
    ascii_puts("  ascii_put_f32: omitted"); nl();
#else
    ascii_puts("  ascii_put_f32: present"); nl();
#endif
    nl();
}

int main(void)
{
    print_banner();
    test_pm_print();   nl();
    test_basic();      nl();
    test_conv();       nl();
#ifndef PMATH_NO_SQRT
    test_sqrt();       nl();
#endif
#ifndef PMATH_NO_TRIG
    test_trig();       nl();
#endif
    test_hex();        nl();
    ascii_puts("Done."); nl();
    return 0;
}
