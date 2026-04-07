# portable-libs

Three small C libraries for embedded and retro targets. No stdlib, no malloc,
no printf. Just a UART hook and you're in business.

They're designed to work together but each one stands alone. The focus
is 8-bit platforms — Z80, AVR, 6809 — though they build clean on RP2040
and anything else you throw at them.

---

## What's here

**`portable_types.h`** is the foundation. One header, one flag
(`PORTABLE_NO_STDINT`), covers all six integer types the family needs.
Every other library includes it automatically so you never have to think
about it unless your toolchain is genuinely ancient.

**`ascii_io`** handles all your console I/O. Strings, integers, hex, floats,
packed BCD, line input, parsing. Everything you'd normally lean on printf
for, without dragging in the C runtime. You give it one function that writes
a byte to your UART and it takes care of the rest.

**`pmath`** is a minimal math library built around a `num_t` type that
switches between Q8.8 fixed-point and 32-bit float at compile time. Same
source code, both targets. On an 8-bit machine you get fixed-point sin/cos
from a 65-byte lookup table, a no-division binary sqrt, and all the usual
arithmetic helpers. On something with an FPU you flip one flag and get
proper floats with no code changes.

**`cbuf`** manages a flat byte array as a character display. You hand it a
pointer, tell it the dimensions, and it gives you a soft cursor, translation
table, scrolling, and read/write primitives. Hardware rendering is entirely
your problem — the library just keeps track of what's where. Works directly
with a memory-mapped screen on a CoCo or VZ200 with no intermediary.

---

## Why bother

On a Z80 or AVR, `math.h` sin() is a full software float implementation —
typically 1.5–4KB of machine code just for that one function. The fixed-point
sin here is around 100–150 bytes of code plus the 65-byte table. That's a
real difference when you're working in a 32K ROM.

Same story with printf. It's enormous on small targets and usually pulls in
heap allocation. `ascii_io` covers every output format you actually need and
costs a fraction of the size.

The other thing is portability. SDCC, Z88DK, AVR-GCC, ia16-gcc, CMOC — they
all have their own quirks around math libraries and float support. These
libraries have none of those dependencies. Drop the `.c` files in your build,
define an `ascii_putc` hook, done.

---

## Quick start

```c
#define ASCII_NO_FLOAT    /* don't need the float printer on this target */
#define PMATH_NO_TRIG     /* not doing any trig in this project */

#include "ascii_io.h"
#include "pmath.h"

/* Your platform hook -- one function, that's it */
void ascii_putc(char c) { while (!(UCSRA & (1<<UDRE))); UDR = c; }

int main(void)
{
    num_t x = PM_F(2.0f);
    num_t r = pm_sqrt(x);

    ascii_puts("sqrt(2) = ");
    pm_print(r, 4);
    ascii_nl();
    /* sqrt(2) = 1.4140 */

    return 0;
}
```

Include order: `ascii_io.h` first, `pmath.h` second, `cbuf.h` third if you're
using it. Each header pulls in `portable_types.h` automatically.

---

## Fixed-point or float

By default `pmath` runs in Q8.8 fixed-point. That gives you a ±127 integer
range with about 0.004 resolution — fine for most sensor work, animation,
and geometry on small systems.

```c
/* Default: Q8.8 fixed-point, no float anywhere */
#include "pmath.h"
```

If you're on a target where float is cheap — RP2040 has float routines baked
into its ROM — flip one flag:

```c
#define PMATH_USE_FLOAT
#include "pmath.h"
```

The same `num_t` type, the same function calls, the same `PM_F()` literals.
Nothing else changes in your application code.

You can also tune the fixed-point format if Q8.8 isn't the right fit:

```c
#define PMATH_FRAC_BITS 12  /* Q4.12 -- tighter range, finer resolution */
#define PMATH_FRAC_BITS  4  /* Q12.4 -- wider range, coarser steps      */
```

---

## No float support at all

Some 6809 toolchains (and others) have no float type whatsoever — not just
expensive float, genuinely absent. `PMATH_NO_FP_CONST` handles this.

In fixed-point mode all the arithmetic is pure integer at runtime already.
The only place float tokens appear is in the constant definitions like `PM_PI`
and `PM_HALF_PI`, which use float literals to compute their raw Q8.8 values
at compile time. On a no-float toolchain the compiler chokes on those before
it even gets to your code.

`PMATH_NO_FP_CONST` replaces every one of those with a pre-computed integer
literal. After that there is not a single float token anywhere in the library:

```c
#define ASCII_NO_FLOAT
#define PMATH_NO_FP_CONST
#include "ascii_io.h"
#include "pmath.h"
```

Because `PM_F()` can no longer do the float-to-fixed conversion, it is
intentionally left undefined under this flag. Use `PM_RAW()` instead and
supply the pre-scaled Q8.8 integer directly:

```c
/* Instead of PM_F(3.5f): */
num_t x = PM_RAW(896);     /* 3.5 * 256 = 896 */
num_t y = PM_RAW(320);     /* 1.25 * 256 = 320 */

/* Addition and subtraction work with plain + and - */
num_t sum = x + y;         /* 4.75 -- no macro needed */

/* Multiply and divide must go through pm_mul / pm_div */
num_t prod = pm_mul(x, y); /* 4.375 */
num_t quot = pm_div(x, y); /* 2.8   */

pm_print(sum,  3);  ascii_nl();   /* 4.750 */
pm_print(prod, 3);  ascii_nl();   /* 4.375 */
pm_print(quot, 3);  ascii_nl();   /* 2.796 */
```

Addition and subtraction on fixed-point are just integer `+` and `-` — the
binary point doesn't move. Multiply and divide need the 32-bit shift that
`pm_mul`/`pm_div` provide, otherwise the binary point ends up in the wrong
place.

---

## Using cbuf with ascii_io

Point `ASCII_PUTC` at `cbuf_putc` and every ascii_io output function renders
directly into the screen buffer. The cast to `uint8_t` matters — without it,
characters with the high bit set sign-extend on targets where `char` is signed,
which corrupts the table lookup for inverse video and graphics characters.

```c
#define ASCII_PUTC(c)  cbuf_putc((uint8_t)(c))
#include "ascii_io.h"
#include "pmath.h"
#include "cbuf.h"

static uint8_t screen[32 * 16];  /* 32 cols, 16 rows */

void ascii_putc_unused(char c) { (void)c; }  /* stub -- macro overrides it */

int main(void)
{
    cbuf_init(screen, 32, 16);
    cbuf_clear(' ');

    cbuf_goto(0, 0);
    ascii_puts("pi = ");
    pm_print(PM_PI, 4);          /* renders: "pi = 3.1406" into buffer */

    cbuf_goto(0, 1);
    ascii_puts("sqrt(2) = ");
    pm_print(pm_sqrt(PM_TWO), 4); /* renders: "sqrt(2) = 1.4140" */

    /* Now copy screen[] to hardware, blit it, whatever you need */
    return 0;
}
```

On a CoCo or VZ200 with memory-mapped video, `screen` just IS the display RAM:

```c
static uint8_t *screen = (uint8_t *)0x0400;  /* CoCo screen RAM */
cbuf_init(screen, 32, 16);
```

---

## Stripping it down

Every feature has a drop flag. On a really constrained target you can get
pmath down to 32 bytes of text and still have arithmetic and `pm_print`.

```c
#define ASCII_NO_FLOAT      /* no soft-float library on this toolchain */
#define ASCII_NO_BCD        /* not using BCD */
#define ASCII_NO_INPUT      /* output-only device */
#define PMATH_NO_TRIG       /* no sin/cos needed */
#define PMATH_NO_SQRT       /* no sqrt needed */
```

The one rule: don't define `ASCII_NO_FLOAT` when `PMATH_USE_FLOAT` is set.
`pm_print` in float mode needs `ascii_put_f32`. The header enforces this
with a `#error` at compile time so you'll know immediately if you get the
combination wrong.

In fixed-point mode `pm_print` uses pure integer arithmetic and
`ASCII_NO_FLOAT` is completely safe. That's the typical 8-bit setup.

cbuf also has drop flags:

```c
#define CBUF_ASCII_NATIVE    /* no translation table -- display ROM is ASCII */
#define CBUF_NO_SCROLL       /* drop scroll_up entirely */
```

---

## What's in the box

### portable_types.h

One flag: `PORTABLE_NO_STDINT` — suppresses `<stdint.h>` and uses manual
typedefs instead. Only needed on toolchains that genuinely lack stdint.h.
Provides `uint8_t`, `int8_t`, `uint16_t`, `int16_t`, `uint32_t`, `int32_t`,
and `NULL`. Every library in the family includes it automatically.

### ascii_io

- String output: `ascii_puts`, `ascii_putn`, `ascii_nl`
- Unsigned decimal: `ascii_put_u8`, `ascii_put_u16`
- Signed decimal: `ascii_put_i8`, `ascii_put_i16`
- Hex output: `ascii_put_x8`, `ascii_put_x16`, with and without `0x` prefix
- Float output: `ascii_put_f32(v, decimals)` — half-up rounding, 7 digit cap
- Packed BCD: `ascii_put_bcd8`, `ascii_put_bcd16`
- Line input: `ascii_gets` with backspace handling and optional echo
- Parse from string: u8, u16, i8, i16, hex — with overflow checking
- Stream read: reads a number directly from the GETC hook

### pmath

- `num_t` type — `int16_t` in fixed mode, `float` in float mode
- `PM_F(x)` — float literal to `num_t` (fixed and float modes)
- `PM_RAW(x)` — pre-scaled integer to `num_t` (all modes including no-float)
- `pm_mul`, `pm_div` — correct fixed-point arithmetic with 32-bit intermediates
- `+` and `-` on `num_t` work directly in fixed mode — no macro needed
- `pm_abs`, `pm_min`, `pm_max`, `pm_sign`, `pm_clamp`
- `pm_floor`, `pm_ceil`, `pm_round`
- `pm_lerp` — linear interpolation, one 32-bit multiply in fixed mode
- `pm_wrap_angle`, `pm_deg2rad`, `pm_rad2deg`
- `pm_sqrt` — no-division binary method (fixed) / rsqrt bit trick (float)
- `pm_sin`, `pm_cos` — 65-byte table (fixed) / Bhaskara I approximation (float)
- `pm_print(v, decimals)` — bridges to ascii_io, integer-only path in fixed mode
- `pm_ftoi`, `pm_itof`, `pm_to_float`, `pm_from_float`

### cbuf

- `cbuf_init(ptr, cols, rows)` — bind library to a display buffer
- `cbuf_clear(fill)` — fill entire buffer, reset cursor to 0,0
- `cbuf_goto(x, y)` — move soft cursor, returns error on out-of-bounds
- `cbuf_get_x()`, `cbuf_get_y()` — read current cursor position
- `cbuf_putc(c)` — translate and write at cursor, advance, scroll or error
- `cbuf_puts(s)` — write string via cbuf_putc
- `cbuf_getc()` — read raw screen code at cursor (no cursor move)
- `cbuf_get_raw(x, y)`, `cbuf_put_raw(x, y, code)` — direct buffer access, no translation
- `cbuf_clear_line(y, fill)` — clear one row
- `cbuf_clear_eol(fill)` — clear from cursor to end of row
- `cbuf_read_eol(fill, dst, maxlen)` — copy meaningful content from cursor to end of row
- `cbuf_scroll_up(fill)` — shift all rows up, clear bottom
- `cbuf_set_scroll(on)` — enable/disable auto-scroll at runtime
- `cbuf_set_table(ptr)` — point to a different translation table at runtime
- `cbuf_copy_table(dst)` — clone ROM default table into RAM for patching

---

## Building

No build system required. Add the `.c` files to your project.

```
portable_types.h
ascii_io.c   ascii_io.h
pmath.c      pmath.h
cbuf.c       cbuf.h
```

```sh
# GCC desktop (quick sanity check)
gcc -std=c99 -Wall main.c pmath.c ascii_io.c cbuf.c -o myapp

# Float mode
gcc -std=c99 -Wall -DPMATH_USE_FLOAT main.c pmath.c ascii_io.c cbuf.c -o myapp

# AVR
avr-gcc -std=c99 -mmcu=atmega328p -Os \
    -DASCII_NO_FLOAT -DASCII_NO_INPUT \
    main.c pmath.c ascii_io.c cbuf.c -o myapp.elf

# SDCC Z80
sdcc --std-c99 -mz80 \
    -DASCII_NO_FLOAT -DASCII_NO_INPUT \
    main.c pmath.c ascii_io.c cbuf.c

# CMOC 6809 -- no float subsystem at all
cmoc -DASCII_NO_FLOAT -DPMATH_NO_FP_CONST \
    main.c pmath.c ascii_io.c cbuf.c

# Old toolchain without stdint.h
gcc -std=c99 -DPORTABLE_NO_STDINT main.c pmath.c ascii_io.c cbuf.c -o myapp
```

---

## Standards and compatibility

`portable_types.h`, `ascii_io`, and `cbuf` are ANSI C89/C90. `pmath` requires
C99 for `int32_t`/`uint32_t` and the union type-pun in the float sqrt. If
you're on a pre-C99 toolchain with no float support at all, define
`PMATH_NO_SQRT`, `PMATH_NO_FP_CONST`, and `ASCII_NO_FLOAT` — the fixed-point
arithmetic core and `pm_print` will compile cleanly.

If your toolchain lacks `<stdint.h>` entirely, define `PORTABLE_NO_STDINT`.
The fallback typedefs in `portable_types.h` cover everything the family needs.

See the [full reference](portable_libs.md) for the complete API, all
flag combinations, and implementation notes.

---

## License

MIT License

Copyright (c) 2025 David

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
