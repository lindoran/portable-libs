# portable-libs

Two small C libraries for embedded and retro targets. No stdlib, no malloc,
no printf. Just a UART hook and you're in business.

They're designed to work together but either one stands alone. The focus
is 8-bit platforms — Z80, AVR, 6809 — though they build clean on RP2040
and anything else you throw at them.

---

## What's here

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
libraries have none of those dependencies. Drop the two `.c` files in your
build, define an `ascii_putc` hook, done.

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

Always include `ascii_io.h` before `pmath.h`. They share a type sentinel
and the order matters.

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
num_t x = PM_RAW(896);    /* 3.5 * 256 = 896 */

/* Addition and subtraction work with plain + and - */
num_t y = PM_RAW(320);    /* 1.25 * 256 = 320 */
num_t sum = x + y;        /* 4.75 -- no macro needed */

/* Multiply and divide must go through pm_mul / pm_div */
num_t prod = pm_mul(x, y); /* 4.375 */
num_t quot = pm_div(x, y); /* 2.8   */

pm_print(sum,  3);  ascii_nl();   /* 4.750 */
pm_print(prod, 3);  ascii_nl();   /* 4.375 */
pm_print(quot, 3);  ascii_nl();   /* 2.796 */
```

The key point: addition and subtraction on fixed-point are just integer
`+` and `-` — the binary point doesn't move. Multiply and divide need
the 32-bit shift that `pm_mul`/`pm_div` provide, otherwise the binary
point ends up in the wrong place.

---

## Stripping it down

Both libraries are designed to be trimmed. Every feature has a drop flag.
On a really constrained target you can get pmath down to 32 bytes of text
and still have arithmetic and `pm_print`.

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

---

## What's in the box

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
- `PM_F(x)` — float literal to `num_t` (fixed/float modes)
- `PM_RAW(x)` — pre-scaled integer to `num_t` (all modes including no-float)
- `pm_mul`, `pm_div` — correct fixed-point arithmetic with 32-bit intermediates
- `+` and `-` on `num_t` work directly — no macro needed in fixed mode
- `pm_abs`, `pm_min`, `pm_max`, `pm_sign`, `pm_clamp`
- `pm_floor`, `pm_ceil`, `pm_round`
- `pm_lerp` — linear interpolation, one 32-bit multiply in fixed mode
- `pm_wrap_angle`, `pm_deg2rad`, `pm_rad2deg`
- `pm_sqrt` — no-division binary method (fixed) / rsqrt bit trick (float)
- `pm_sin`, `pm_cos` — 65-byte table (fixed) / Bhaskara I approximation (float)
- `pm_print(v, decimals)` — bridges to ascii_io, integer-only path in fixed mode
- `pm_ftoi`, `pm_itof`, `pm_to_float`, `pm_from_float`

---

## Building

No build system required. Just add the two `.c` files to your project.

```
ascii_io.c  ascii_io.h
pmath.c     pmath.h
```

```sh
# GCC desktop (quick sanity check)
gcc -std=c99 -Wall main.c pmath.c ascii_io.c -o myapp

# Float mode
gcc -std=c99 -Wall -DPMATH_USE_FLOAT main.c pmath.c ascii_io.c -o myapp

# AVR
avr-gcc -std=c99 -mmcu=atmega328p -Os \
    -DASCII_NO_FLOAT -DASCII_NO_INPUT \
    main.c pmath.c ascii_io.c -o myapp.elf

# SDCC Z80
sdcc --std-c99 -mz80 \
    -DASCII_NO_FLOAT -DASCII_NO_INPUT \
    main.c pmath.c ascii_io.c

# CMOC 6809 -- no float subsystem at all
cmoc -DASCII_NO_FLOAT -DPMATH_NO_FP_CONST \
    main.c pmath.c ascii_io.c
```

---

## Standards and compatibility

`ascii_io` is ANSI C89/C90. `pmath` requires C99 for `int32_t` and the
union type-pun in the float sqrt. If you're on a pre-C99 toolchain with no
float support at all, define `PMATH_NO_SQRT`, `PMATH_NO_FP_CONST`, and
`ASCII_NO_FLOAT` — the fixed-point arithmetic core and `pm_print` will
compile cleanly.

See the [full reference](portable_libs.md) for the complete API, all
flag combinations, and implementation notes.

---

## License



Copyright 2026 David Collins

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

