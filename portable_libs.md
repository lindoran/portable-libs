# Portable Embedded Libraries Reference

Three companion libraries for 8/16-bit targets with no stdlib dependency.

```c
#include "ascii_io.h"   /* always first */
#include "pmath.h"      /* second       */
#include "cbuf.h"       /* third        */
```

Each header pulls in `portable_types.h` automatically. You never need to
include it explicitly unless you want types available before including any
library header.

---

## Table of Contents

- [portable\_types.h — Integer Types](#portable_typesh--integer-types)
- [ascii\_io — ASCII Console I/O](#ascii_io--ascii-console-io)
  - [Platform Hooks](#platform-hooks)
  - [Compile-time Flags](#compile-time-flags)
  - [String Output](#string-output)
  - [Integer Output](#integer-output)
  - [Float Output](#float-output)
  - [BCD Output](#bcd-output)
  - [Input and Parse](#input-and-parse)
- [pmath — Portable Math](#pmath--portable-math)
  - [Compile-time Flags](#compile-time-flags-1)
  - [Fixed-point format options](#fixed-point-format-options)
  - [Number Format](#number-format)
  - [Constants](#constants)
  - [Literal Conversion](#literal-conversion)
  - [Multiply and Divide](#multiply-and-divide)
  - [Type Conversions](#type-conversions)
  - [Arithmetic](#arithmetic)
  - [Floor / Ceil / Round](#floor--ceil--round)
  - [Linear Interpolation](#linear-interpolation)
  - [Angle Helpers](#angle-helpers)
  - [Square Root](#square-root)
  - [Trigonometry](#trigonometry)
  - [Printing — pm\_print](#printing--pm_print)
- [cbuf — Character Display Buffer](#cbuf--character-display-buffer)
  - [Compile-time Flags](#compile-time-flags-2)
  - [Translation Table](#translation-table)
  - [Return Codes](#return-codes)
  - [Init](#init)
  - [Cursor](#cursor)
  - [Read](#read)
  - [Write](#write)
  - [Scroll](#scroll)
- [Using All Three Together](#using-all-three-together)
  - [Include Order](#include-order)
  - [Valid Flag Combinations](#valid-flag-combinations)
  - [Routing ascii\_io Through cbuf](#routing-ascii_io-through-cbuf)
  - [A Typical Z80/AVR Main](#a-typical-z80avr-main)
  - [A Typical RP2040 Main](#a-typical-rp2040-main)
  - [The Illegal Combination](#the-illegal-combination)

---

## portable_types.h — Integer Types

**File:** `portable_types.h`
**Standard:** C89/C90

Single source of truth for integer types across the whole library family.
Provides `uint8_t`, `int8_t`, `uint16_t`, `int16_t`, `uint32_t`, `int32_t`,
and `NULL`. Every other library includes it automatically via its own header.

### Flag

| Flag | Effect |
|------|--------|
| `PORTABLE_NO_STDINT` | Suppress `<stdint.h>`. Manual fallback typedefs are used instead. Only needed on toolchains that genuinely lack stdint.h. |

### Manual fallback typedefs

When `PORTABLE_NO_STDINT` is defined, the following assumptions are made.
Adjust the file if your platform differs:

```
uint8_t   unsigned char    -- always correct
uint16_t  unsigned short   -- correct on most 8/16-bit targets
uint32_t  unsigned long    -- correct on most 8/16-bit targets
                              (NOT correct on LP64 Linux -- but those have stdint.h)
```

### Usage

```c
/* Normal use -- stdint.h is available */
#include "portable_types.h"
uint16_t count = 0;

/* Ancient toolchain without stdint.h */
#define PORTABLE_NO_STDINT
#include "portable_types.h"
uint16_t count = 0;
```

---

## ascii_io — ASCII Console I/O

**File:** `ascii_io.h`, `ascii_io.c`
**Standard:** ANSI C89/C90
**Targets:** GCC, ia16-gcc, MinGW, SDCC, Z88DK, Arduino
**Dependency:** `portable_types.h` (included automatically)

Provides string, integer, float, and hex output plus line input and
parsing. All output goes through a single `ASCII_PUTC` hook; all input
goes through `ASCII_GETC`. No heap, no stdio, no stdlib.

---

### Platform Hooks

You must provide an output character function. Input is only needed
when `ASCII_NO_INPUT` is not defined.

**Option A — function definition (default):**

```c
/* platform.c */
#include "ascii_io.h"

void ascii_putc(char c) { UART_TX = c; }
char ascii_getc(void)   { return UART_RX; }
```

**Option B — inline macros (zero call overhead):**

Define the macros *before* including the header. The function declarations
are suppressed entirely.

```c
#define ASCII_PUTC(c)  uart_tx(c)
#define ASCII_GETC()   uart_rx()
#include "ascii_io.h"
```

**Routing through cbuf:**

```c
#define ASCII_PUTC(c)  cbuf_putc((uint8_t)(c))
#include "ascii_io.h"
#include "cbuf.h"
```

The cast to `uint8_t` is required. On targets where `char` is signed,
characters with the high bit set (inverse video, graphics, upper national
characters) sign-extend before the table lookup and produce wrong results
without it.

---

### Compile-time Flags

| Flag | Effect |
|------|--------|
| `ASCII_NO_FLOAT` | Omit `ascii_put_f32`. Safe in fixed-point pmath builds. Must NOT be set when `PMATH_USE_FLOAT` is active — pmath.h emits a `#error` if both are set. |
| `ASCII_NO_BCD` | Omit packed-BCD output functions. |
| `ASCII_NO_INPUT` | Omit all input, `ascii_gets`, and all parse/read functions. |
| `ASCII_CRLF` | `ascii_nl()` emits `\r\n` instead of `\n`. |
| `ASCII_GETS_ECHO` | `ascii_gets()` echoes each typed character back to the terminal. |
| `PORTABLE_NO_STDINT` | Suppress `<stdint.h>` in `portable_types.h`. Provide manual typedefs via that file. |

---

### String Output

```c
void ascii_puts(const char *s);            /* NUL-terminated, NULL-safe */
void ascii_putn(const char *s, uint8_t n); /* exactly n bytes */
void ascii_nl(void);                       /* newline (LF or CR+LF)     */
```

```c
ascii_puts("Hello, world");
ascii_nl();

ascii_putn("ABCDEF", 3);   /* prints "ABC" */
ascii_nl();
```

---

### Integer Output

```c
void ascii_put_u8 (uint8_t  v);    /*   0..255       */
void ascii_put_u16(uint16_t v);    /*   0..65535     */
void ascii_put_i8 (int8_t  v);     /* -128..127      */
void ascii_put_i16(int16_t v);     /* -32768..32767  */

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

INT_MIN is handled correctly — the implementation promotes through unsigned
arithmetic before negating to avoid undefined behaviour.

---

### Float Output

```c
/* Omitted when ASCII_NO_FLOAT is defined */
void ascii_put_f32(float v, uint8_t decimals);
```

`decimals` is silently capped at 7. Rounding is half-up, applied before
splitting whole and fractional parts so carries roll over correctly.
NaN and Inf are not handled.

```c
ascii_put_f32(3.14159f, 4);   ascii_nl();   /* 3.1416 */
ascii_put_f32(-0.5f,    2);   ascii_nl();   /* -0.50  */
ascii_put_f32(1.9995f,  3);   ascii_nl();   /* 2.000  */
```

On SDCC and Z88DK, software float is large. Define `ASCII_NO_FLOAT` and use
`pm_print()` from pmath in fixed-point mode — it uses only integer arithmetic.

---

### BCD Output

```c
/* Omitted when ASCII_NO_BCD is defined */
void ascii_put_bcd8 (uint8_t  v);
void ascii_put_bcd16(uint16_t v);
```

Each nibble is one decimal digit 0-9. Useful for RTC and hardware BCD registers.

```c
ascii_put_bcd8(0x42);     ascii_nl();   /* 42   */
ascii_put_bcd16(0x1999);  ascii_nl();   /* 1999 */
```

---

### Input and Parse

All input functions are omitted when `ASCII_NO_INPUT` is defined.

```c
uint8_t ascii_gets(char *buf, uint8_t maxlen);
```

Reads up to `maxlen-1` chars, NUL-terminates, returns character count.
Stops on CR or LF. Handles backspace and DEL. Define `ASCII_GETS_ECHO`
to echo characters as typed.

```c
char buf[16];
ascii_gets(buf, sizeof(buf));
ascii_puts("You typed: ");
ascii_puts(buf);
ascii_nl();
```

**Parse (string to number)** — returns 1 on success, 0 on bad format or
overflow. `*out` is unchanged on failure. Leading whitespace is not skipped.

```c
uint8_t ascii_parse_u8 (const char *s, uint8_t  *out);
uint8_t ascii_parse_u16(const char *s, uint16_t *out);
uint8_t ascii_parse_i8 (const char *s, int8_t   *out);
uint8_t ascii_parse_i16(const char *s, int16_t  *out);
uint8_t ascii_parse_x8 (const char *s, uint8_t  *out);   /* accepts 0x prefix */
uint8_t ascii_parse_x16(const char *s, uint16_t *out);
```

```c
uint16_t v;
if (ascii_parse_u16("1234", &v)) {
    ascii_put_u16(v);   /* 1234 */
}

int16_t i;
if (!ascii_parse_i16("99999", &i)) {
    ascii_puts("overflow");   /* 99999 > INT16_MAX */
}
```

**Stream read** — skips leading spaces and tabs, reads until a non-number
character (usually CR/LF), which is consumed.

```c
uint8_t ascii_read_u16(uint16_t *out);
uint8_t ascii_read_i16(int16_t  *out);
uint8_t ascii_read_x16(uint16_t *out);
```

---

## pmath — Portable Math

**File:** `pmath.h`, `pmath.c`
**Standard:** C99
**Targets:** Z80, AVR, 6809, RP2040, and other 8/16-bit systems
**Dependency:** `ascii_io.h` (for `pm_print`), `portable_types.h`

---

### Compile-time Flags

| Flag | Effect |
|------|--------|
| `PMATH_USE_FLOAT` | Use 32-bit IEEE 754 `float` as `num_t`. Default is Q8.8 fixed-point. |
| `PMATH_FRAC_BITS n` | Set fractional bit count (default 8). |
| `PMATH_NO_TRIG` | Omit `pm_sin` and `pm_cos`. Saves ~200 bytes ROM in fixed mode. |
| `PMATH_NO_SQRT` | Omit `pm_sqrt`. |
| `PMATH_NO_FP_CONST` | Replace all float literals in constant definitions with pre-computed Q8.8 integers. Use when the toolchain has no float type at all. Only valid in fixed-point mode. Only correct for `PMATH_FRAC_BITS=8`. `PM_F()` is undefined under this flag — use `PM_RAW()`. |
| `PORTABLE_NO_STDINT` | Suppress `<stdint.h>`. See `portable_types.h`. |

#### Fixed-point format options

| `PMATH_FRAC_BITS` | Format | Range | Resolution | Best for |
|---|---|---|---|---|
| 4 | Q12.4 | ±2047 | ~0.0625 | Large integer ranges |
| **8** (default) | **Q8.8** | **±127** | **~0.0039** | **General 8-bit use** |
| 12 | Q4.12 | ±7 | ~0.00024 | High-precision fractions |

---

### Number Format

`num_t` is the universal numeric type across pmath. All functions accept and
return `num_t`. The same source code compiles in both fixed and float modes.

```c
typedef int16_t fixed_t;   /* always available */

#ifdef PMATH_USE_FLOAT
  typedef float   num_t;
#else
  typedef fixed_t num_t;
#endif
```

---

### Constants

All constants are in `num_t` format and resolve at compile time.

| Constant | Value |
|---|---|
| `PM_ZERO` | 0 |
| `PM_HALF` | 0.5 |
| `PM_ONE` | 1.0 |
| `PM_TWO` | 2.0 |
| `PM_PI` | π ≈ 3.14159 |
| `PM_TWO_PI` | 2π ≈ 6.28318 |
| `PM_HALF_PI` | π/2 ≈ 1.57079 |
| `PM_SQRT2` | √2 ≈ 1.41421 |
| `PM_INV_SQRT2` | 1/√2 ≈ 0.70710 |

---

### Literal Conversion

**`PM_F(x)`** converts a compile-time float constant to `num_t`. Use for
initialisers, not inner loops on 8-bit targets.

```c
num_t speed     = PM_F(2.5f);
num_t threshold = PM_F(-0.75f);
```

**`PM_RAW(x)`** takes a pre-scaled integer and converts it to `num_t`.
Available in all modes, and is the only option when `PMATH_NO_FP_CONST`
is defined because `PM_F()` is intentionally undefined in that mode.

```c
/* PMATH_NO_FP_CONST build -- no float type on this toolchain */
num_t x = PM_RAW(896);    /* 3.5 * 256 = 896 -- Q8.8 for 3.5 */
num_t y = PM_RAW(64);     /* 0.25 * 256 = 64 -- Q8.8 for 0.25 */
```

Any remaining `PM_F()` calls give a compile error under `PMATH_NO_FP_CONST`,
which is the correct behaviour -- replace them with `PM_RAW()`.

For integer values, `pm_itof` is a shift with no float literals at all:

```c
num_t seven = pm_itof(7);
```

---

### Multiply and Divide

Use `pm_mul` and `pm_div` for portable code. In fixed mode they use a 32-bit
intermediate to keep the binary point correct. In float mode they expand to
plain `*` and `/`. Never use `*` directly on two `fixed_t` values.

```c
num_t a = PM_F(3.5f);
num_t b = PM_F(1.25f);

num_t product = pm_mul(a, b);   /* 4.375 */
num_t quotient = pm_div(a, b);  /* 2.800 (float) / 2.796 (Q8.8) */

pm_print(product,  4);  ascii_nl();
pm_print(quotient, 4);  ascii_nl();
```

Addition and subtraction work with plain `+` and `-` in both modes — the
binary point does not move for add/subtract in fixed-point.

---

### Type Conversions

```c
int16_t pm_ftoi(num_t x);      /* num_t -> int16_t (truncates in float, floors in fixed) */
num_t   pm_itof(int16_t x);    /* int16_t -> num_t                                        */

float   pm_to_float(num_t x);  /* num_t -> float  (debug bridge, avoid in production)    */
num_t   pm_from_float(float x);/* float -> num_t  (debug bridge)                          */
```

```c
num_t  x = PM_F(3.9f);
int16_t i = pm_ftoi(x);        /* 3 */
num_t  back = pm_itof(i);      /* 3.0 */
```

`pm_to_float` and `pm_from_float` involve a float operation in fixed mode.
Avoid them in production code when `ASCII_NO_FLOAT` is defined as they
may pull in a soft-float library on SDCC.

---

### Arithmetic

All inline, no function call overhead.

```c
num_t pm_abs  (num_t x);
num_t pm_min  (num_t a, num_t b);
num_t pm_max  (num_t a, num_t b);
int   pm_sign (num_t x);            /* returns -1, 0, or 1 */
num_t pm_clamp(num_t x, num_t lo, num_t hi);
```

```c
pm_print(pm_abs(PM_F(-2.5f)),                    4);  /* 2.5000 */
pm_print(pm_clamp(PM_F(7.0f), PM_ONE, PM_F(4.0f)), 4);  /* 4.0000 */
ascii_put_i16(pm_sign(-PM_ONE));  ascii_nl();             /* -1     */
```

---

### Floor / Ceil / Round

```c
num_t pm_floor(num_t x);
num_t pm_ceil (num_t x);
num_t pm_round(num_t x);   /* half-up: pm_floor(x + 0.5) */
```

Fixed mode uses bit-masking — correct for negative two's-complement values,
no branches.

```c
pm_print(pm_floor(PM_F( 3.7f)), 4);  /*  3.0000 */
pm_print(pm_floor(PM_F(-1.5f)), 4);  /* -2.0000 */
pm_print(pm_ceil (PM_F(-1.5f)), 4);  /* -1.0000 */
pm_print(pm_round(PM_F( 2.6f)), 4);  /*  3.0000 */
```

---

### Linear Interpolation

```c
num_t pm_lerp(num_t a, num_t b, num_t t);
/* a + t*(b-a),  t in [0, 1] */
```

```c
num_t result = pm_lerp(PM_ZERO, PM_F(255.0f), PM_F(0.75f));
pm_print(result, 0);  ascii_nl();   /* 191 */
```

---

### Angle Helpers

```c
num_t pm_wrap_angle(num_t x);      /* wrap radians to [0, 2*pi) */
num_t pm_deg2rad(num_t degrees);
num_t pm_rad2deg(num_t radians);
```

Fixed-point range note: Q8.8 holds integers up to 127, so `pm_rad2deg`
output above ~127 degrees overflows. Use `PMATH_USE_FLOAT` for full
0-360 degree arithmetic.

```c
num_t a = pm_deg2rad(PM_F(45.0f));
pm_print(a, 4);  ascii_nl();   /* 0.7851 (Q8.8) / 0.7854 (float) */

num_t d = pm_rad2deg(PM_HALF_PI);
pm_print(d, 4);  ascii_nl();   /* 90.0000 */
```

---

### Square Root

```c
/* Omitted when PMATH_NO_SQRT is defined */
num_t pm_sqrt(num_t x);
```

**Fixed mode:** digit-by-digit binary method, no division. Works for any
`PMATH_FRAC_BITS` value.

**Float mode:** fast inverse sqrt via IEEE 754 bit trick with two
Newton-Raphson iterations, error < 1 ULP. Requires C99 union type-punning.

```c
pm_print(pm_sqrt(PM_ONE),    4);  /* 1.0000 */  ascii_nl();
pm_print(pm_sqrt(PM_TWO),    4);  /* 1.4142 */  ascii_nl();
pm_print(pm_sqrt(PM_F(9.0f)),4);  /* 3.0000 */  ascii_nl();
```

---

### Trigonometry

```c
/* Omitted when PMATH_NO_TRIG is defined */
num_t pm_sin(num_t radians);
num_t pm_cos(num_t radians);
```

**Fixed mode:** 65-entry `uint8_t` quarter-wave table (65 bytes ROM).
Quadrant reduction uses only subtraction and comparison. Table index
computed as `(x * 41) >> 8`, approximating `x * 64 / 402` without division.
Accuracy < 0.5 LSB across full range at Q8.8.

**Float mode:** Bhaskara I rational approximation on [0, π]:

```
sin(x) ≈ 16x(π−x) / (5π² − 4x(π−x))
```

Two multiplies, one divide. Exact at 0, π/6, π/4, π/3, π/2 and mirrors.
Maximum error ~0.17%.

```c
pm_print(pm_sin(PM_ZERO),    4);  /* 0.0000 */  ascii_nl();
pm_print(pm_sin(PM_HALF_PI), 4);  /* 1.0000 */  ascii_nl();
pm_print(pm_cos(PM_PI),      4);  /* -1.0000 */ ascii_nl();

/* Pythagorean identity */
num_t s = pm_sin(PM_F(1.2f));
num_t c = pm_cos(PM_F(1.2f));
num_t id = (num_t)(pm_mul(s,s) + pm_mul(c,c));
pm_print(id, 4);  ascii_nl();   /* ~0.9921 (Q8.8) / ~0.9994 (float) */
```

---

### Printing — pm_print

```c
void pm_print(num_t v, uint8_t decimals);
```

**Float mode:** delegates to `ascii_put_f32(v, decimals)`. `ASCII_NO_FLOAT`
must not be set — enforced by `#error` at compile time.

**Fixed mode:** digit-by-digit integer arithmetic only. No float involved
at any point. `ASCII_NO_FLOAT` is completely safe.

```c
pm_print(PM_PI,         4);   /* 3.1416 (float) / 3.1406 (Q8.8) */
pm_print(PM_F(1.5f),    3);   /* 1.500  */
pm_print(PM_F(-1.5f),   3);   /* -1.500 */
pm_print(pm_itof(42),   0);   /* 42     */
```

---

## cbuf — Character Display Buffer

**File:** `cbuf.h`, `cbuf.c`
**Standard:** C89/C90
**Targets:** Z80, AVR, 6809, RP2040, memory-mapped display hardware
**Dependency:** `portable_types.h` (included automatically)

Manages a flat byte array as a character display. Each byte is one screen
position. The library tracks a soft cursor and writes translated character
codes into the buffer. Hardware rendering is entirely the application's
responsibility — this library only manages what is in the buffer.

It is a palette and canvas. Backspace, line editing, blinking cursors,
double buffering — those are brushes. Build them from the primitives here.

---

### Compile-time Flags

| Flag | Effect |
|------|--------|
| `CBUF_ASCII_NATIVE` | No translation table. Characters written directly. Use when the display ROM is ASCII-compatible. Drops all table code and ROM data. Cannot combine with `CBUF_TABLE_POINTER`. |
| `CBUF_NO_SCROLL` | Drop `cbuf_scroll_up` and the runtime scroll flag at compile time. `cbuf_putc` returns `CBUF_OVERFLOW` on out-of-bounds, always. |
| `CBUF_TABLE_POINTER` | Translation table is a runtime pointer. Enables `cbuf_set_table()` and `cbuf_copy_table()`. Cannot combine with `CBUF_ASCII_NATIVE`. |
| `CBUF_TABLE_SIZE 256` | Extend table to full byte range 0x00..0xFF (default 128). Costs 256 bytes ROM for the default table vs 128. Needed for inverse video, graphics characters, upper national characters (CoCo, VZ200). |
| `PORTABLE_NO_STDINT` | Suppress `<stdint.h>`. See `portable_types.h`. |

---

### Translation Table

`cbuf_default_table` is defined in `cbuf.c` as an identity map (ASCII
passthrough). Override it by defining your own array with the same name
in your application — the linker prefers your definition.

With `CBUF_TABLE_POINTER` you can switch tables at runtime and patch
the ROM default into RAM:

```c
static uint8_t my_table[CBUF_TABLE_SIZE];
cbuf_copy_table(my_table);          /* clone ROM default into RAM    */
my_table['A'] = COCO_SCREEN_A;     /* patch individual entries      */
cbuf_set_table(my_table);           /* repoint library to RAM table  */

/* Repoint back to ROM default later if needed */
cbuf_set_table(cbuf_default_table);
```

`cbuf_copy_table` is a 128 or 256 byte loop — essentially free on any target.

---

### Return Codes

| Code | Value | Meaning |
|------|-------|---------|
| `CBUF_OK` | 0 | Operation succeeded |
| `CBUF_SCROLLED` | 1 | Write succeeded but caused a scroll |
| `CBUF_OVERFLOW` | -1 | Out of bounds and scroll is off or dropped |

---

### Init

```c
void cbuf_init(uint8_t *ptr, uint8_t cols, uint8_t rows);
```

Bind the library to a display buffer. `ptr` must point to at least
`cols * rows` bytes. On memory-mapped targets pass the screen RAM address
directly. Cursor is reset to (0,0). Buffer contents are not cleared.

```c
static uint8_t screen[32 * 16];
cbuf_init(screen, 32, 16);
cbuf_clear(' ');

/* CoCo memory-mapped screen: */
cbuf_init((uint8_t *)0x0400, 32, 16);
```

---

### Cursor

```c
int8_t  cbuf_goto(uint8_t x, uint8_t y);
uint8_t cbuf_get_x(void);
uint8_t cbuf_get_y(void);
```

`cbuf_goto` moves the soft cursor. Out-of-bounds coordinates are clamped and
`CBUF_OVERFLOW` is returned — the cursor always ends up at a valid position.

```c
cbuf_goto(0, 0);                          /* top-left          */
cbuf_goto(cbuf_get_x() + 1, cbuf_get_y()); /* one step right  */
```

---

### Read

```c
uint8_t cbuf_getc(void);
uint8_t cbuf_get_raw(uint8_t x, uint8_t y);
uint8_t cbuf_read_eol(uint8_t fill, char *dst, uint8_t maxlen);
```

`cbuf_getc` reads the raw screen code at the current cursor position.
`cbuf_get_raw` reads at an arbitrary position. Both return 0 for out-of-bounds
positions. Neither moves the cursor.

`cbuf_read_eol` copies raw bytes from the cursor to the end of meaningful
content on the current row, stripping trailing `fill` characters. Always
NUL-terminates `dst`. Returns the number of bytes copied.

```c
/* Soft cursor blink -- save, replace, restore */
uint8_t saved = cbuf_getc();
cbuf_put_raw(cbuf_get_x(), cbuf_get_y(), 0xDB);  /* show block cursor */
/* ... on next timer tick: */
cbuf_put_raw(cbuf_get_x(), cbuf_get_y(), saved);  /* restore           */

/* Read user input from a row */
char buf[33];
cbuf_goto(0, 5);
cbuf_read_eol(' ', buf, sizeof(buf));
ascii_puts("Got: ");
ascii_puts(buf);
ascii_nl();
```

---

### Write

```c
int8_t  cbuf_putc(uint8_t c);
int8_t  cbuf_puts(const char *s);
void    cbuf_clear(uint8_t fill);
void    cbuf_clear_line(uint8_t y, uint8_t fill);
void    cbuf_clear_eol(uint8_t fill);
int8_t  cbuf_put_raw(uint8_t x, uint8_t y, uint8_t code);
```

`cbuf_putc` translates `c` through the active table and writes at the current
cursor position, then advances. At end of row it wraps to column 0 of the
next row. At end of the last row it scrolls (if scroll is on) or returns
`CBUF_OVERFLOW` (if scroll is off). Returns `CBUF_OK`, `CBUF_SCROLLED`,
or `CBUF_OVERFLOW`.

`cbuf_puts` calls `cbuf_putc` for each character. Stops at first overflow.

`cbuf_put_raw` writes a raw screen code with no translation, no cursor
movement, and no scroll. Use it for graphics characters, inverse video,
and cursor blinking.

`cbuf_clear` fills the whole buffer and resets cursor to (0,0).
`cbuf_clear_line` fills one row. `cbuf_clear_eol` fills from cursor to end
of current row. None of the clear operations move the cursor (except
`cbuf_clear` which resets it).

```c
cbuf_goto(0, 0);
cbuf_puts("Hello, world");

cbuf_goto(10, 5);
cbuf_put_raw(10, 5, 0xAE);   /* direct graphics char at (10,5) */

cbuf_clear_line(7, '-');     /* draw a separator line */
```

---

### Scroll

```c
/* Both omitted when CBUF_NO_SCROLL is defined */
void cbuf_set_scroll(uint8_t on);
void cbuf_scroll_up(uint8_t fill);
```

`cbuf_set_scroll(1)` enables auto-scroll on overflow (the default after
`cbuf_init`). `cbuf_set_scroll(0)` switches to hard-stop mode — `cbuf_putc`
returns `CBUF_OVERFLOW` instead of scrolling.

`cbuf_scroll_up` shifts rows 1..rows-1 up to rows 0..rows-2, fills the new
bottom row with `fill` (a raw screen code, not translated), and decrements
the cursor row by one. Uses a plain byte loop — no memmove, no string.h.

```c
cbuf_set_scroll(1);

/* Fill all rows then overflow -- watch it scroll */
uint8_t y;
for (y = 0; y < 16; y++) {
    cbuf_goto(0, y);
    cbuf_putc((uint8_t)('0' + (y % 10)));
    cbuf_puts(": content here");
}
/* Writing past the last row triggers scroll automatically */
cbuf_puts(" overflow!");

/* Or scroll explicitly */
cbuf_scroll_up(' ');
```

---

## Using All Three Together

### Include Order

```c
#include "ascii_io.h"   /* first  */
#include "pmath.h"      /* second */
#include "cbuf.h"       /* third  */
```

Each header includes `portable_types.h` internally. The first include
defines the types via the header's include guard — subsequent includes are
no-ops. You never need to worry about type redefinitions.

### Valid Flag Combinations

| Configuration | Flags | Notes |
|---|---|---|
| Full build | *(none)* | Everything present |
| Ideal 8-bit | `ASCII_NO_FLOAT` | Fixed math, integer print, float printer dropped |
| No trig | `ASCII_NO_FLOAT` `PMATH_NO_TRIG` | Saves 65-byte table + ~150 bytes |
| No sqrt | `ASCII_NO_FLOAT` `PMATH_NO_SQRT` | Saves ~50 bytes |
| No float type at all | `ASCII_NO_FLOAT` `PMATH_NO_FP_CONST` | 6809/no-float toolchain. Use `PM_RAW()` not `PM_F()`. Q8.8 only. |
| Bare minimum | `ASCII_NO_FLOAT` `ASCII_NO_BCD` `ASCII_NO_INPUT` `PMATH_NO_TRIG` `PMATH_NO_SQRT` | 32 bytes pmath text |
| Float on capable target | `PMATH_USE_FLOAT` | RP2040, ARM |
| ASCII-native display | `CBUF_ASCII_NATIVE` | Display ROM is ASCII, no table needed |
| No scroll | `CBUF_NO_SCROLL` | Drop scroll_up entirely |
| Full CoCo/VZ200 | `CBUF_TABLE_SIZE=256` `CBUF_TABLE_POINTER` | Inverse video, graphics, patchable table |
| Higher precision | `PMATH_FRAC_BITS=12` | Q4.12, any drop flag combination |
| No stdint.h | `PORTABLE_NO_STDINT` | Ancient toolchain, one flag covers all three libraries |

### Routing ascii_io Through cbuf

```c
#define ASCII_PUTC(c)  cbuf_putc((uint8_t)(c))
#include "ascii_io.h"
#include "pmath.h"
#include "cbuf.h"

static uint8_t screen[32 * 16];

void ascii_putc(char c) { (void)c; }  /* stub -- ASCII_PUTC macro overrides */

int main(void)
{
    cbuf_init(screen, 32, 16);
    cbuf_clear(' ');

    cbuf_goto(0, 0);
    ascii_puts("pi = ");
    pm_print(PM_PI, 4);        /* "pi = 3.1406" written into screen buffer */

    cbuf_goto(0, 1);
    ascii_puts("sqrt(2) = ");
    pm_print(pm_sqrt(PM_TWO), 4);

    /* Push screen[] to hardware here */
    return 0;
}
```

### A Typical Z80/AVR Main

```c
#define ASCII_NO_FLOAT
#define ASCII_NO_INPUT
#define PMATH_NO_TRIG

#include "ascii_io.h"
#include "pmath.h"

void ascii_putc(char c) { /* UART write */ }

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

### A Typical RP2040 Main

```c
#define PMATH_USE_FLOAT   /* RP2040 ROM float routines are essentially free */

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

### The Illegal Combination

This is caught at compile time with a `#error`:

```c
#define PMATH_USE_FLOAT
#define ASCII_NO_FLOAT   /* ERROR -- pm_print needs ascii_put_f32 in float mode */
#include "ascii_io.h"
#include "pmath.h"
/* pmath.h: error: PMATH_USE_FLOAT requires ascii_put_f32() */
```

In fixed-point mode there is no such restriction. `ASCII_NO_FLOAT` is always
safe when `PMATH_USE_FLOAT` is not set.

---

*ascii_io, cbuf, portable_types: C89/C90 — pmath: C99*
