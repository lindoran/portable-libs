# Portable Embedded Libraries Reference

Two companion libraries for 8/16-bit targets with no stdlib dependency.
Include `ascii_io.h` before `pmath.h` in every translation unit.

```c
#include "ascii_io.h"
#include "pmath.h"
```

---

## Table of Contents

- [ascii\_io — ASCII Console I/O](#ascii_io)
  - [Platform Hooks](#platform-hooks)
  - [Compile-time Flags](#ascii-flags)
  - [String Output](#string-output)
  - [Integer Output](#integer-output)
  - [Float Output](#float-output)
  - [BCD Output](#bcd-output)
  - [Input and Parse](#input-and-parse)
- [pmath — Portable Math](#pmath)
  - [Compile-time Flags](#pmath-flags)
  - [Number Format](#number-format)
  - [Constants](#constants)
  - [Literal Conversion](#literal-conversion)
  - [Multiply and Divide](#multiply-and-divide)
  - [Type Conversions](#type-conversions)
  - [Arithmetic](#arithmetic)
  - [Floor / Ceil / Round](#floor-ceil-round)
  - [Linear Interpolation](#linear-interpolation)
  - [Angle Helpers](#angle-helpers)
  - [Square Root](#square-root)
  - [Trigonometry](#trigonometry)
  - [Printing — pm\_print](#pm_print)
- [Using Both Libraries Together](#together)
  - [Valid Flag Combinations](#valid-combinations)
  - [The Illegal Combination](#illegal-combination)

---

## ascii_io — ASCII Console I/O {#ascii_io}

**File:** `ascii_io.h`, `ascii_io.c`  
**Standard:** ANSI C89/C90  
**Targets:** GCC, ia16-gcc, MinGW, SDCC, Z88DK, Arduino  
**Dependency:** `<stdint.h>` (or manual typedefs via `ASCII_NO_STDINT`)

Provides string, integer, float, and hex output plus line input and
parsing. All output goes through a single `ASCII_PUTC` hook; all input
goes through `ASCII_GETC`. No heap, no stdio, no stdlib.

---

### Platform Hooks {#platform-hooks}

You must provide an output character function. Input is only needed
when `ASCII_NO_INPUT` is not defined.

**Option A — function pointers (default):**

```c
/* platform.c */
#include "ascii_io.h"

void ascii_putc(char c) { UART_TX = c; }   /* write one byte */
char ascii_getc(void)   { return UART_RX;  /* blocking read */ }
```

**Option B — inline macros (zero call overhead):**

Define the macros *before* including the header and the function
declarations are suppressed entirely.

```c
#define ASCII_PUTC(c)  uart_tx(c)
#define ASCII_GETC()   uart_rx()
#include "ascii_io.h"
```

---

### Compile-time Flags {#ascii-flags}

Define any of these before including `ascii_io.h` to shrink the binary.

| Flag | Effect |
|------|--------|
| `ASCII_NO_FLOAT` | Omit `ascii_put_f32`. Safe in fixed-point pmath builds. **Do not set** when `PMATH_USE_FLOAT` is active. |
| `ASCII_NO_BCD` | Omit packed-BCD output functions. |
| `ASCII_NO_INPUT` | Omit all input, `ascii_gets`, and all parse/read functions. |
| `ASCII_NO_STDINT` | Skip `<stdint.h>`. Provide typedefs manually or rely on `PORTABLE_TYPES_DEFINED`. |
| `ASCII_CRLF` | `ascii_nl()` emits `\r\n` instead of `\n`. |
| `ASCII_GETS_ECHO` | `ascii_gets()` echoes each typed character back to the terminal. |

---

### String Output {#string-output}

```c
void ascii_puts(const char *s);           /* NUL-terminated, NULL-safe */
void ascii_putn(const char *s, uint8_t n); /* exactly n bytes */
void ascii_nl(void);                       /* newline (LF or CR+LF) */
```

```c
ascii_puts("Hello, world");
ascii_nl();

ascii_putn("ABCDEF", 3);   /* prints "ABC" */
ascii_nl();
```

Output:
```
Hello, world
ABC
```

---

### Integer Output {#integer-output}

```c
/* Unsigned decimal */
void ascii_put_u8 (uint8_t  v);    /*   0..255   */
void ascii_put_u16(uint16_t v);    /*   0..65535 */

/* Signed decimal */
void ascii_put_i8 (int8_t  v);     /* -128..127     */
void ascii_put_i16(int16_t v);     /* -32768..32767 */

/* Hex, zero-padded, uppercase */
void ascii_put_x8 (uint8_t  v);    /* "AB"     */
void ascii_put_x16(uint16_t v);    /* "ABCD"   */
void ascii_put_x8p (uint8_t  v);   /* "0xAB"   */
void ascii_put_x16p(uint16_t v);   /* "0xABCD" */
```

```c
ascii_put_u16(12345);   ascii_nl();   /* 12345  */
ascii_put_i16(-500);    ascii_nl();   /* -500   */
ascii_put_x8(0xBE);     ascii_nl();   /* BE     */
ascii_put_x16p(0xDEAD); ascii_nl();   /* 0xDEAD */
```

INT_MIN is handled correctly — negating `INT16_MIN` (-32768) as a
signed type is undefined behaviour in C; the implementation promotes
through unsigned arithmetic before negating.

---

### Float Output {#float-output}

```c
/* Omitted when ASCII_NO_FLOAT is defined */
void ascii_put_f32(float v, uint8_t decimals);
```

`decimals` is silently capped at 7 (the precision limit of 32-bit float).
Rounding is half-up, applied before splitting whole and fractional parts
so a carry correctly rolls over (e.g. 1.9995 at 3dp prints `2.000`).
NaN and Inf are not handled.

```c
ascii_put_f32(3.14159f, 4);   ascii_nl();   /* 3.1416  */
ascii_put_f32(-0.5f,    2);   ascii_nl();   /* -0.50   */
ascii_put_f32(1.9995f,  3);   ascii_nl();   /* 2.000   */
ascii_put_f32(1234.0f,  0);   ascii_nl();   /* 1234    */
```

On SDCC and Z88DK, software float is large (~1–2 KB). Define
`ASCII_NO_FLOAT` and use `pm_print()` from pmath (which uses integer
arithmetic in fixed-point mode) to avoid it entirely.

---

### BCD Output {#bcd-output}

```c
/* Omitted when ASCII_NO_BCD is defined */
void ascii_put_bcd8 (uint8_t  v);   /* two nibbles -> two digits */
void ascii_put_bcd16(uint16_t v);   /* four nibbles -> four digits */
```

Each nibble is treated as one decimal digit 0–9. No validity check.
Useful for RTC or hardware BCD registers.

```c
ascii_put_bcd8(0x42);     ascii_nl();   /* 42   */
ascii_put_bcd16(0x1999);  ascii_nl();   /* 1999 */
```

---

### Input and Parse {#input-and-parse}

All input functions are omitted when `ASCII_NO_INPUT` is defined.

#### Line input

```c
uint8_t ascii_gets(char *buf, uint8_t maxlen);
/* Returns number of characters read (not counting NUL).
   Stops on CR or LF. Handles backspace and DEL.
   Define ASCII_GETS_ECHO to echo characters as typed. */
```

```c
char buf[16];
uint8_t n = ascii_gets(buf, sizeof(buf));
ascii_puts("You typed: ");
ascii_puts(buf);
ascii_nl();
```

#### Parse (string → number)

Returns `1` on success, `0` on bad format or overflow.
`*out` is unchanged on failure. Leading whitespace is **not** skipped.

```c
uint8_t ascii_parse_u8 (const char *s, uint8_t  *out);
uint8_t ascii_parse_u16(const char *s, uint16_t *out);
uint8_t ascii_parse_i8 (const char *s, int8_t   *out);
uint8_t ascii_parse_i16(const char *s, int16_t  *out);
uint8_t ascii_parse_x8 (const char *s, uint8_t  *out);   /* accepts "0x" prefix */
uint8_t ascii_parse_x16(const char *s, uint16_t *out);
```

```c
uint16_t v;
if (ascii_parse_u16("1234", &v)) {
    ascii_put_u16(v);   /* 1234 */
}

uint8_t b;
if (ascii_parse_x8("0xFF", &b)) {
    ascii_put_x8(b);    /* FF */
}

int16_t i;
if (!ascii_parse_i16("99999", &i)) {
    ascii_puts("overflow");   /* overflow -- 99999 > INT16_MAX */
}
```

#### Stream read (directly from ASCII_GETC)

Skips leading spaces and tabs. Reads until a non-number character,
which is consumed (usually CR/LF — harmless on a line terminal).

```c
uint8_t ascii_read_u16(uint16_t *out);
uint8_t ascii_read_i16(int16_t  *out);
uint8_t ascii_read_x16(uint16_t *out);
```

```c
uint16_t x;
ascii_puts("Enter a number: ");
if (ascii_read_u16(&x)) {
    ascii_puts("Got: ");
    ascii_put_u16(x);
    ascii_nl();
}
```

---

## pmath — Portable Math {#pmath}

**File:** `pmath.h`, `pmath.c`  
**Standard:** C99  
**Targets:** Z80, AVR, 6809, RP2040, and other 8/16-bit systems  
**Dependency:** `ascii_io.h` (for `pm_print`), `<stdint.h>`

Provides fixed-point or float arithmetic, trig, sqrt, and output via
`pm_print`. All operations are expressed in terms of `num_t`, which
resolves to either `fixed_t` (int16_t) or `float` depending on the
build flags. The same application source compiles unchanged in both modes.

---

### Compile-time Flags {#pmath-flags}

| Flag | Effect |
|------|--------|
| `PMATH_USE_FLOAT` | Use 32-bit IEEE 754 `float` as `num_t`. Default is Q8.8 fixed-point. |
| `PMATH_FRAC_BITS n` | Set fractional bit count (default 8). See table below. |
| `PMATH_NO_TRIG` | Omit `pm_sin` and `pm_cos`. Saves ~200 bytes ROM in fixed mode (65-byte table + reduction code). |
| `PMATH_NO_SQRT` | Omit `pm_sqrt`. |
| `PMATH_NO_STDINT` | Skip `<stdint.h>`. Provide typedefs manually or rely on `PORTABLE_TYPES_DEFINED`. |
| `PMATH_NO_FP_CONST` | Replace all float literals in constant definitions with pre-computed Q8.8 integers. Use when the toolchain has no float type at all (e.g. a bare 6809 compiler). Only valid in fixed-point mode. Only correct for `PMATH_FRAC_BITS=8`. `PM_F()` is undefined under this flag — use `PM_RAW()` instead. |

#### Fixed-point format options

| `PMATH_FRAC_BITS` | Format | Integer range | Resolution | Best for |
|-------------------|--------|--------------|------------|----------|
| 4 | Q12.4 | ±2047 | ~0.0625 | Large integer ranges, coarse fractions |
| **8** (default) | **Q8.8** | **±127** | **~0.0039** | **General 8-bit use** |
| 12 | Q4.12 | ±7 | ~0.00024 | High-precision fractions, small range |

---

### Number Format {#number-format}

`num_t` is the universal numeric type. All pmath functions accept and
return `num_t`. You never need to know whether it is fixed or float
internally — the macros and functions handle the format transparently.

```c
typedef int16_t fixed_t;   /* always available */

#ifdef PMATH_USE_FLOAT
  typedef float   num_t;   /* float mode */
#else
  typedef fixed_t num_t;   /* fixed-point mode */
#endif
```

---

### Constants {#constants}

All constants are in `num_t` format and resolve at compile time.

| Constant | Value |
|----------|-------|
| `PM_ZERO` | 0 |
| `PM_HALF` | 0.5 |
| `PM_ONE` | 1.0 |
| `PM_TWO` | 2.0 |
| `PM_PI` | π ≈ 3.14159 |
| `PM_TWO_PI` | 2π ≈ 6.28318 |
| `PM_HALF_PI` | π/2 ≈ 1.57079 |
| `PM_SQRT2` | √2 ≈ 1.41421 |
| `PM_INV_SQRT2` | 1/√2 ≈ 0.70710 |

```c
num_t half_turn = PM_PI;
num_t unit      = PM_ONE;
```

---

### Literal Conversion {#literal-conversion}

`PM_F(x)` converts a compile-time float constant to `num_t`. Use it
for initialisers and configuration — not inside inner loops on 8-bit
targets, because it involves a float multiply at compile time which
some toolchains emit as a runtime call.

```c
num_t speed     = PM_F(2.5f);
num_t threshold = PM_F(-0.75f);
```

In fixed mode `PM_F(x)` rounds half-away-from-zero for both positive
and negative values. In float mode it is a plain cast with no overhead.

For integer values, prefer `pm_itof` which is a shift with no float
literals at all:

```c
num_t seven = pm_itof(7);   /* no float literal involved */
```

**When the toolchain has no float type — `PM_RAW(x)`**

`PM_F()` requires float literals, which some toolchains (bare 6809,
certain CMOC configurations) simply do not support. Define
`PMATH_NO_FP_CONST` and use `PM_RAW()` instead. You supply the
pre-scaled Q8.8 integer directly:

```c
#define PMATH_NO_FP_CONST
#define ASCII_NO_FLOAT

/* PM_F(3.5f) becomes PM_RAW(896)  -- 3.5 * 256 = 896  */
/* PM_F(1.25f) becomes PM_RAW(320) -- 1.25 * 256 = 320  */

num_t x = PM_RAW(896);   /* 3.5  */
num_t y = PM_RAW(320);   /* 1.25 */

/* + and - are plain integer ops -- no macro needed */
num_t sum  = x + y;          /* 4.75  */
num_t diff = x - y;          /* 2.25  */

/* * and / must go through pm_mul / pm_div */
num_t prod = pm_mul(x, y);   /* 4.375 */
num_t quot = pm_div(x, y);   /* 2.796 */

pm_print(sum,  3);  ascii_nl();   /* 4.750 */
pm_print(prod, 3);  ascii_nl();   /* 4.375 */
```

Under `PMATH_NO_FP_CONST`, `PM_F()` is intentionally left undefined.
Any remaining `PM_F()` calls in your code will produce a compile error,
which is the correct behaviour — replace them with `PM_RAW()` and the
pre-scaled value.

All the named constants (`PM_PI`, `PM_HALF_PI`, `PM_SQRT2` etc.) are
also pre-computed as integer literals under this flag, so `pm_sin`,
`pm_cos`, and `pm_sqrt` work without any float tokens in the build.

---

### Multiply and Divide {#multiply-and-divide}

Use `pm_mul` and `pm_div` for mixed-mode portable code. In fixed mode
they use a 32-bit intermediate to keep the binary point correct. In
float mode they expand to plain `*` and `/`.

```c
num_t pm_mul(num_t a, num_t b);   /* macro */
num_t pm_div(num_t a, num_t b);   /* macro */
```

```c
num_t a = PM_F(3.5f);
num_t b = PM_F(1.25f);

num_t product = pm_mul(a, b);   /* 4.375 */
num_t quotient = pm_div(a, b);  /* 2.800 (float) / 2.796 (Q8.8) */

pm_print(product,  4);  ascii_nl();
pm_print(quotient, 4);  ascii_nl();
```

Do **not** use plain `*` on two `fixed_t` values — the result will be
in the wrong Q format. Always go through `pm_mul`.

---

### Type Conversions {#type-conversions}

```c
int16_t pm_ftoi(num_t x);    /* num_t -> int16_t  (macro) */
num_t   pm_itof(int16_t x);  /* int16_t -> num_t  (macro) */

float   pm_to_float(num_t x);    /* num_t -> float  (debug bridge) */
num_t   pm_from_float(float x);  /* float -> num_t  (debug bridge) */
```

`pm_ftoi` truncates toward zero in float mode and floors (arithmetic
right-shift) in fixed mode. For symmetric truncation in fixed mode,
call `pm_floor(pm_abs(x))` and restore the sign.

`pm_to_float` and `pm_from_float` are intended for debug output and
initialisation. In fixed mode they involve a float divide/multiply —
avoid them in inner loops and in production code when `ASCII_NO_FLOAT`
is defined, as they may pull in a soft-float library on SDCC.

```c
num_t  x = PM_F(3.9f);
int16_t i = pm_ftoi(x);           /* 3  (truncates) */
num_t  back = pm_itof(i);         /* 3.0 */

/* Debug use only: */
float f = pm_to_float(PM_PI);     /* ~3.1406 */
num_t p = pm_from_float(3.14f);   /* PM_PI equivalent */
```

---

### Arithmetic {#arithmetic}

All inline, no function call overhead.

```c
num_t pm_abs  (num_t x);
num_t pm_min  (num_t a, num_t b);
num_t pm_max  (num_t a, num_t b);
int   pm_sign (num_t x);           /* returns -1, 0, or 1 */
num_t pm_clamp(num_t x, num_t lo, num_t hi);
```

```c
pm_print(pm_abs(PM_F(-2.5f)),                   4); /* 2.5000 */
pm_print(pm_min(PM_F(3.0f), PM_F(5.0f)),        4); /* 3.0000 */
pm_print(pm_max(PM_F(3.0f), PM_F(5.0f)),        4); /* 5.0000 */
pm_print(pm_clamp(PM_F(7.0f), PM_ONE, PM_F(4.0f)), 4); /* 4.0000 */

ascii_put_i16(pm_sign(PM_PI));    ascii_nl();  /*  1 */
ascii_put_i16(pm_sign(PM_ZERO));  ascii_nl();  /*  0 */
ascii_put_i16(pm_sign(-PM_ONE));  ascii_nl();  /* -1 */
```

---

### Floor / Ceil / Round {#floor-ceil-round}

```c
num_t pm_floor(num_t x);
num_t pm_ceil (num_t x);
num_t pm_round(num_t x);   /* half-up: pm_floor(x + 0.5) */
```

Fixed mode uses bit-masking for floor and ceil — correct for negative
two's-complement values without any branches.

```c
pm_print(pm_floor(PM_F( 3.7f)), 4);  /*  3.0000 */
pm_print(pm_floor(PM_F(-1.5f)), 4);  /* -2.0000 */
pm_print(pm_ceil (PM_F( 3.2f)), 4);  /*  4.0000 */
pm_print(pm_ceil (PM_F(-1.5f)), 4);  /* -1.0000 */
pm_print(pm_round(PM_F( 2.6f)), 4);  /*  3.0000 */
pm_print(pm_round(PM_F( 2.4f)), 4);  /*  2.0000 */
```

---

### Linear Interpolation {#linear-interpolation}

```c
num_t pm_lerp(num_t a, num_t b, num_t t);
/* a + t*(b-a),  t should be in [0, 1] */
```

In fixed mode: one 32-bit multiply. No branches.

```c
/* Fade from 0 to 255 at t=0.75 */
num_t result = pm_lerp(PM_ZERO, PM_F(255.0f), PM_F(0.75f));
pm_print(result, 0);  /* 191 */
ascii_nl();

/* Midpoint between two angles */
num_t mid = pm_lerp(PM_F(1.0f), PM_F(3.0f), PM_HALF);
pm_print(mid, 4);  /* 2.0000 */
ascii_nl();
```

---

### Angle Helpers {#angle-helpers}

```c
num_t pm_wrap_angle(num_t x);      /* wrap radians to [0, 2*pi) */
num_t pm_deg2rad(num_t degrees);   /* degrees -> radians */
num_t pm_rad2deg(num_t radians);   /* radians -> degrees */
```

`pm_wrap_angle` uses a subtraction loop — efficient when angles are
already near-range, as is typical in rotation and animation code.

**Fixed-point range note for `pm_rad2deg`:** Q8.8 holds integers up to
127, so angles above ~127° will overflow on output. `pm_deg2rad` has the
same limit on input. Use `PMATH_USE_FLOAT` for full 0–360° arithmetic.

```c
num_t a = pm_deg2rad(PM_F(45.0f));
pm_print(a, 4);   /* 0.7851 (Q8.8) / 0.7854 (float) */
ascii_nl();

num_t d = pm_rad2deg(PM_HALF_PI);
pm_print(d, 4);   /* 90.0000 */
ascii_nl();

num_t wrapped = pm_wrap_angle(PM_F(-0.5f));
pm_print(wrapped, 4);  /* ~5.7831 (= 2*pi - 0.5) */
ascii_nl();
```

---

### Square Root {#square-root}

```c
/* Omitted when PMATH_NO_SQRT is defined */
num_t pm_sqrt(num_t x);
```

**Fixed mode:** digit-by-digit binary method, no division. Upshifts
the raw input by `PMATH_FRAC_BITS` before the integer sqrt so the
result lands in the correct Q format. Works for any `PMATH_FRAC_BITS`.

**Float mode:** fast inverse sqrt via IEEE 754 bit trick
(`0x5F375A86` constant) with two Newton-Raphson iterations,
error < 1 ULP for 32-bit float. Requires C99 union type-punning.

```c
pm_print(pm_sqrt(PM_ONE),     4);  /* 1.0000 */  ascii_nl();
pm_print(pm_sqrt(PM_TWO),     4);  /* 1.4140 */  ascii_nl();
pm_print(pm_sqrt(PM_F(4.0f)), 4);  /* 2.0000 */  ascii_nl();
pm_print(pm_sqrt(PM_F(9.0f)), 4);  /* 3.0000 */  ascii_nl();
pm_print(pm_sqrt(PM_HALF),    4);  /* 0.7070 */  ascii_nl();
```

---

### Trigonometry {#trigonometry}

```c
/* Omitted when PMATH_NO_TRIG is defined */
num_t pm_sin(num_t radians);
num_t pm_cos(num_t radians);
```

All angles are in radians in the current `num_t` format.

**Fixed mode:** 65-entry `uint8_t` quarter-wave lookup table (65 bytes
ROM). Quadrant reduction folds the full circle down to `[0, π/2]` using
only subtraction and comparison — no division. Table index computed as
`(x * 41) >> 8`, which approximates `x * 64 / 402` without a divide.
Accuracy < 0.5 LSB across full range at Q8.8.

**Float mode:** Bhaskara I rational approximation for sin on `[0, π]`:

```
sin(x) ≈ 16x(π−x) / (5π² − 4x(π−x))
```

Two multiplies and one divide. Exact at 0, π/6, π/4, π/3, π/2 and
their mirrors. Maximum error ~0.17%.

```c
pm_print(pm_sin(PM_ZERO),    4);  /* 0.0000 */  ascii_nl();
pm_print(pm_sin(PM_HALF_PI), 4);  /* 1.0000 */  ascii_nl();
pm_print(pm_sin(PM_PI),      4);  /* 0.0000 */  ascii_nl();
pm_print(pm_cos(PM_ZERO),    4);  /* 1.0000 */  ascii_nl();
pm_print(pm_cos(PM_HALF_PI), 4);  /* 0.0000 */  ascii_nl();

/* Pythagorean identity: sin^2 + cos^2 = 1 */
num_t s = pm_sin(PM_F(1.2f));
num_t c = pm_cos(PM_F(1.2f));
num_t identity = (num_t)(pm_mul(s,s) + pm_mul(c,c));
pm_print(identity, 4);   /* ~0.9921 (Q8.8) / ~0.9994 (float) */
ascii_nl();
```

---

### Printing — pm_print {#pm_print}

```c
void pm_print(num_t v, uint8_t decimals);
```

The primary output function. Bridges `num_t` to `ascii_io` in a way
that is correct and minimal in both modes.

**Float mode:** delegates directly to `ascii_put_f32(v, decimals)`.
`ASCII_NO_FLOAT` must not be set (enforced by `#error` at compile time).

**Fixed mode:** digit-by-digit integer arithmetic only — no float
involved at any point. The fractional digits are extracted by
multiplying the raw fractional bits by 10 in a 32-bit intermediate
and reading the integer part of the result, one digit at a time.
`ASCII_NO_FLOAT` is completely safe to define.

```c
pm_print(PM_PI,         4);  /* 3.1416 (float) / 3.1406 (Q8.8) */
pm_print(PM_F(1.5f),    3);  /* 1.500  */
pm_print(PM_F(-1.5f),   3);  /* -1.500 */
pm_print(PM_ZERO,       2);  /* 0.00   */
pm_print(pm_itof(42),   0);  /* 42     */
```

Precision is bounded by the format. Q8.8 has ~2.4 significant decimal
digits; requesting more decimal places than the format supports will
print correct digits down to the LSB and zeros beyond.

---

## Using Both Libraries Together {#together}

### Include order

Always include `ascii_io.h` before `pmath.h`. Both headers share a
`PORTABLE_TYPES_DEFINED` sentinel so typedefs are only emitted once.

```c
#include "ascii_io.h"
#include "pmath.h"
```

### Valid Flag Combinations {#valid-combinations}

All combinations in this table build and run correctly.

| Configuration | Flags | Notes |
|---------------|-------|-------|
| Full build | *(none)* | Everything present, float printer available |
| Ideal 8-bit | `ASCII_NO_FLOAT` | Fixed math, `pm_print` uses integers, float printer gone |
| No trig | `ASCII_NO_FLOAT` `PMATH_NO_TRIG` | Saves 65-byte table + ~150 bytes reduction code |
| No sqrt | `ASCII_NO_FLOAT` `PMATH_NO_SQRT` | Saves ~50 bytes |
| No float type at all | `ASCII_NO_FLOAT` `PMATH_NO_FP_CONST` | 6809 / no-float toolchain. Use `PM_RAW()` instead of `PM_F()`. Q8.8 only. |
| Bare minimum | `ASCII_NO_FLOAT` `ASCII_NO_BCD` `ASCII_NO_INPUT` `PMATH_NO_TRIG` `PMATH_NO_SQRT` | 32 bytes pmath text; arithmetic + print only |
| Float on capable target | `PMATH_USE_FLOAT` | RP2040, ARM; float printer required |
| Higher precision | `PMATH_FRAC_BITS=12` | Q4.12 — works with any drop flag above |
| Wider range | `PMATH_FRAC_BITS=4` | Q12.4 — works with any drop flag above |

### A typical Z80/AVR main

```c
#define ASCII_NO_FLOAT      /* no soft-float library */
#define ASCII_NO_INPUT      /* output only device    */
#define PMATH_NO_TRIG       /* trig not needed here  */

#include "ascii_io.h"
#include "pmath.h"

/* Platform hook -- replace with your UART write */
void ascii_putc(char c) { /* ... */ }

int main(void)
{
    num_t x = PM_F(9.0f);
    num_t r = pm_sqrt(x);

    ascii_puts("sqrt(9) = ");
    pm_print(r, 3);
    ascii_nl();
    /* Prints: sqrt(9) = 3.000 */

    return 0;
}
```

### A typical RP2040 main

```c
#define PMATH_USE_FLOAT     /* RP2040 ROM float is cheap */

#include "ascii_io.h"
#include "pmath.h"

void ascii_putc(char c) { uart_putc(uart0, c); }

int main(void)
{
    num_t angle = PM_F(0.523f);   /* ~30 degrees */

    ascii_puts("sin(30 deg) = ");
    pm_print(pm_sin(angle), 5);
    ascii_nl();
    /* Prints: sin(30 deg) = 0.50000 */

    return 0;
}
```

### The Illegal Combination {#illegal-combination}

This combination is caught at compile time with a `#error`:

```c
#define PMATH_USE_FLOAT
#define ASCII_NO_FLOAT      /* ERROR -- pm_print needs ascii_put_f32 */
#include "ascii_io.h"
#include "pmath.h"
/* pmath.h:85: error: PMATH_USE_FLOAT requires ascii_put_f32() */
```

In fixed-point mode there is no such restriction — `ASCII_NO_FLOAT` is
always safe when `PMATH_USE_FLOAT` is not set.

---

*ascii_io standard: C89/C90 — pmath standard: C99*
