/*
 * ascii_io.c  --  Portable ASCII console I/O, no stdlib
 * ANSI C89 / C90.  See ascii_io.h for usage and porting notes.
 */

#include "ascii_io.h"

/* =======================================================================
 * INTERNAL HELPERS
 * ======================================================================= */

/*
 * Emit one hex nibble (0-15) as an uppercase ASCII digit.
 */
static void put_hexnib(uint8_t n)
{
    n &= 0x0Fu;
    ASCII_PUTC(n < 10u ? (char)('0' + n) : (char)('A' + n - 10u));
}

/*
 * Decimal digit extraction into a small stack buffer, least-significant
 * digit first.  Returns the number of digits written (always >= 1).
 * Buffers are sized for the maximum digit count of each type.
 *
 * We avoid the (--i) trick with uint8_t because some compilers warn
 * about unsigned wrap; emit_buf() uses an explicit > 0 guard instead.
 */

static uint8_t fill_u8(uint8_t v, char buf[3])
{
    uint8_t n = 0u;
    if (v == 0u) { buf[0] = '0'; return 1u; }
    while (v != 0u) {
        buf[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    return n;
}

static uint8_t fill_u16(uint16_t v, char buf[5])
{
    uint8_t n = 0u;
    if (v == 0u) { buf[0] = '0'; return 1u; }
    while (v != 0u) {
        buf[n++] = (char)('0' + (uint8_t)(v % 10u));
        v /= 10u;
    }
    return n;
}

/*
 * Emit a digit buffer in reverse (most-significant digit first).
 */
static void emit_buf(char *buf, uint8_t n)
{
    while (n > 0u) {
        n--;
        ASCII_PUTC(buf[n]);
    }
}

/* =======================================================================
 * STRING OUTPUT
 * ======================================================================= */

void ascii_puts(ASCII_CONST char *s)
{
    if (s == NULL) return;
    while (*s != '\0') { ASCII_PUTC(*s); s++; }
}

void ascii_putn(ASCII_CONST char *s, uint8_t n)
{
    if (s == NULL) return;
    while (n > 0u && *s != '\0') { ASCII_PUTC(*s); s++; n--; }
}

void ascii_nl(void)
{
#ifdef ASCII_CRLF
    ASCII_PUTC('\r');
#endif
    ASCII_PUTC('\n');
}

/* =======================================================================
 * HEX OUTPUT
 * ======================================================================= */

void ascii_put_x8(uint8_t v)
{
    put_hexnib(v >> 4u);
    put_hexnib(v);
}

void ascii_put_x16(uint16_t v)
{
    ascii_put_x8((uint8_t)(v >> 8u));
    ascii_put_x8((uint8_t)(v));
}

void ascii_put_x8p(uint8_t v)  { ASCII_PUTC('0'); ASCII_PUTC('x'); ascii_put_x8(v);  }
void ascii_put_x16p(uint16_t v){ ASCII_PUTC('0'); ASCII_PUTC('x'); ascii_put_x16(v); }

/* =======================================================================
 * UNSIGNED DECIMAL OUTPUT
 * ======================================================================= */

void ascii_put_u8(uint8_t v)
{
    char buf[3];
    emit_buf(buf, fill_u8(v, buf));
}

void ascii_put_u16(uint16_t v)
{
    char buf[5];
    emit_buf(buf, fill_u16(v, buf));
}

/* =======================================================================
 * SIGNED DECIMAL OUTPUT
 *
 * INT_MIN edge case:
 *   Negating INT8_MIN (-128) or INT16_MIN (-32768) as the same signed type
 *   is undefined behaviour in C.  We promote one rank wider before negating
 *   so the arithmetic stays in the unsigned domain where wrap is defined.
 *   int8_t  -> promoted via (int16_t) before cast to uint8_t
 *   int16_t -> promoted via unary minus on the value already being negative;
 *              since -INT16_MIN (32768) fits in uint16_t we cast directly.
 * ======================================================================= */

void ascii_put_i8(int8_t v)
{
    if (v < 0) {
        ASCII_PUTC('-');
        ascii_put_u8((uint8_t)(-(int16_t)v));
    } else {
        ascii_put_u8((uint8_t)v);
    }
}

void ascii_put_i16(int16_t v)
{
    if (v < 0) {
        ASCII_PUTC('-');
        /* -INT16_MIN = 32768, which fits in uint16_t cleanly. */
        ascii_put_u16((uint16_t)(0u - (uint16_t)v));
    } else {
        ascii_put_u16((uint16_t)v);
    }
}

/* =======================================================================
 * FLOAT OUTPUT
 *
 * We stay entirely in float + uint8_t arithmetic; no wider integer type
 * is needed or used.  This keeps the float path self-contained for
 * compilers (SDCC) where long / int32_t operations are costly.
 *
 * Strategy:
 *   1. Pre-apply rounding (add 0.5 * 10^-decimals) so a carry in the
 *      fractional part correctly rolls into the whole part.
 *   2. Find the magnitude of the whole part by repeated *10.
 *   3. Emit whole-part digits by successive division + remainder.
 *   4. Emit fractional digits by successive *10 + floor.
 *
 * Precision cap: float has ~7 significant decimal digits.  Requesting
 * more than 7-len(whole) fractional digits produces noise.  The `decimals`
 * parameter is silently capped at 7 for safety.
 * ======================================================================= */

#ifndef ASCII_NO_FLOAT

void ascii_put_f32(float v, uint8_t decimals)
{
    float    divisor;
    float    rounding;
    uint8_t  dig;
    uint8_t  d;

    /* Handle sign */
    if (v < 0.0f) { ASCII_PUTC('-'); v = -v; }

    /* Cap fractional digits at float precision */
    if (decimals > 7u) decimals = 7u;

    /* Pre-round: add 0.5 ulp at the last requested decimal place.
     * This ensures e.g. 1.9995 printed to 3dp shows "2.000" not "1.999". */
    rounding = 0.5f;
    for (d = 0u; d < decimals; d++) rounding /= 10.0f;
    v += rounding;

    /* Find the magnitude of the whole part.
     * After rounding, v >= 1.0 is guaranteed if the result has digits. */
    divisor = 1.0f;
    while (divisor * 10.0f <= v) divisor *= 10.0f;

    /* Emit whole-part digits */
    while (divisor >= 1.0f) {
        dig = (uint8_t)(v / divisor);
        if (dig > 9u) dig = 9u;        /* guard float rounding fuzz */
        ASCII_PUTC((char)('0' + dig));
        v -= (float)dig * divisor;
        divisor /= 10.0f;
    }

    if (decimals == 0u) return;
    ASCII_PUTC('.');

    /* Emit fractional digits by shifting one decimal place at a time */
    for (d = 0u; d < decimals; d++) {
        v *= 10.0f;
        dig = (uint8_t)v;
        if (dig > 9u) dig = 9u;
        ASCII_PUTC((char)('0' + dig));
        v -= (float)dig;
    }
}

#endif /* ASCII_NO_FLOAT */

/* =======================================================================
 * PACKED BCD OUTPUT
 * ======================================================================= */

#ifndef ASCII_NO_BCD

void ascii_put_bcd8(uint8_t v)
{
    ASCII_PUTC((char)('0' + ((v >> 4u) & 0x0Fu)));
    ASCII_PUTC((char)('0' + (v & 0x0Fu)));
}

void ascii_put_bcd16(uint16_t v)
{
    ascii_put_bcd8((uint8_t)(v >> 8u));
    ascii_put_bcd8((uint8_t)(v));
}

#endif /* ASCII_NO_BCD */

/* =======================================================================
 * STRING INPUT
 * ======================================================================= */

#ifndef ASCII_NO_INPUT

uint8_t ascii_gets(char *buf, uint8_t maxlen)
{
    uint8_t n = 0u;
    char c;

    if (buf == NULL || maxlen == 0u) return 0u;

    while (n < (uint8_t)(maxlen - 1u)) {
        c = ASCII_GETC();
        if (c == '\r' || c == '\n') break;
        if (c == '\b' || c == (char)127) {  /* backspace or DEL */
            if (n > 0u) {
                n--;
#ifdef ASCII_GETS_ECHO
                ASCII_PUTC('\b'); ASCII_PUTC(' '); ASCII_PUTC('\b');
#endif
            }
            continue;
        }
        buf[n++] = c;
#ifdef ASCII_GETS_ECHO
        ASCII_PUTC(c);
#endif
    }
    buf[n] = '\0';
    return n;
}

/* =======================================================================
 * PARSE -- internal hex nibble helper
 * Returns 0xFF on a character that is not a valid hex digit.
 * ======================================================================= */

static uint8_t hexval(char c)
{
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    return 0xFFu;
}

/* =======================================================================
 * PARSE -- unsigned decimal
 *
 * Overflow guard without needing a wider type:
 *   uint8_t  max = 255.   threshold = 255/10 = 25.  above 25, *10 overflows.
 *                          at 25, only digits 0-5 are safe (25*10+5=255).
 *   uint16_t max = 65535.  threshold = 6553.         at 6553 only 0-5 safe.
 * ======================================================================= */

uint8_t ascii_parse_u8(ASCII_CONST char *s, uint8_t *out)
{
    uint8_t acc = 0u;
    uint8_t any = 0u;
    uint8_t d;

    if (s == NULL || out == NULL) return 0u;

    while (*s != '\0') {
        if (*s < '0' || *s > '9') break;
        d = (uint8_t)(*s - '0');
        if (acc > 25u) return 0u;               /* *10 would exceed 255 */
        if (acc == 25u && d > 5u) return 0u;    /* 25*10+6 = 256 */
        acc = (uint8_t)(acc * 10u + d);
        any = 1u;
        s++;
    }
    if (!any) return 0u;
    *out = acc;
    return 1u;
}

uint8_t ascii_parse_u16(ASCII_CONST char *s, uint16_t *out)
{
    uint16_t acc = 0u;
    uint8_t  any = 0u;
    uint8_t  d;

    if (s == NULL || out == NULL) return 0u;

    while (*s != '\0') {
        if (*s < '0' || *s > '9') break;
        d = (uint8_t)(*s - '0');
        if (acc > 6553u) return 0u;
        if (acc == 6553u && d > 5u) return 0u;  /* 6553*10+6 = 65536 */
        acc = (uint16_t)(acc * 10u + d);
        any = 1u;
        s++;
    }
    if (!any) return 0u;
    *out = acc;
    return 1u;
}

/* =======================================================================
 * PARSE -- signed decimal
 * ======================================================================= */

uint8_t ascii_parse_i8(ASCII_CONST char *s, int8_t *out)
{
    uint8_t neg = 0u;
    uint8_t uval;

    if (s == NULL || out == NULL) return 0u;
    if (*s == '-') { neg = 1u; s++; }
    else if (*s == '+') { s++; }

    if (!ascii_parse_u8(s, &uval)) return 0u;

    if (neg) {
        if (uval > 128u) return 0u;
        *out = (uval == 128u) ? (int8_t)(-128) : (int8_t)(-(int8_t)uval);
    } else {
        if (uval > 127u) return 0u;
        *out = (int8_t)uval;
    }
    return 1u;
}

uint8_t ascii_parse_i16(ASCII_CONST char *s, int16_t *out)
{
    uint8_t  neg = 0u;
    uint16_t uval;

    if (s == NULL || out == NULL) return 0u;
    if (*s == '-') { neg = 1u; s++; }
    else if (*s == '+') { s++; }

    if (!ascii_parse_u16(s, &uval)) return 0u;

    if (neg) {
        if (uval > 32768u) return 0u;
        /* 0u - uval: defined unsigned arithmetic, then reinterpret */
        *out = (int16_t)(0u - uval);
    } else {
        if (uval > 32767u) return 0u;
        *out = (int16_t)uval;
    }
    return 1u;
}

/* =======================================================================
 * PARSE -- hex
 * ======================================================================= */

uint8_t ascii_parse_x8(ASCII_CONST char *s, uint8_t *out)
{
    uint8_t acc = 0u;
    uint8_t any = 0u;
    uint8_t nib;

    if (s == NULL || out == NULL) return 0u;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;

    while (*s != '\0') {
        nib = hexval(*s);
        if (nib == 0xFFu) break;
        if (acc > 0x0Fu) return 0u;            /* shifting in nib overflows */
        acc = (uint8_t)((acc << 4u) | nib);
        any = 1u;
        s++;
    }
    if (!any) return 0u;
    *out = acc;
    return 1u;
}

uint8_t ascii_parse_x16(ASCII_CONST char *s, uint16_t *out)
{
    uint16_t acc = 0u;
    uint8_t  any = 0u;
    uint8_t  nib;

    if (s == NULL || out == NULL) return 0u;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;

    while (*s != '\0') {
        nib = hexval(*s);
        if (nib == 0xFFu) break;
        if (acc > 0x0FFFu) return 0u;
        acc = (uint16_t)((acc << 4u) | nib);
        any = 1u;
        s++;
    }
    if (!any) return 0u;
    *out = acc;
    return 1u;
}

/* =======================================================================
 * STREAM READ  (reads from ASCII_GETC hook)
 *
 * Skips leading spaces and tabs.  Reads characters into a small stack
 * buffer until a non-number character is seen.  That terminating char
 * is consumed -- on a line terminal this is normally CR or LF, which
 * the next ascii_gets() would discard anyway.
 * ======================================================================= */

uint8_t ascii_read_u16(uint16_t *out)
{
    char    buf[6];             /* 5 digits + NUL */
    uint8_t n = 0u;
    char    c;

    do { c = ASCII_GETC(); } while (c == ' ' || c == '\t');

    while (n < 5u && c >= '0' && c <= '9') {
        buf[n++] = c;
        c = ASCII_GETC();
    }
    buf[n] = '\0';
    return ascii_parse_u16(buf, out);
}

uint8_t ascii_read_i16(int16_t *out)
{
    char    buf[7];             /* optional sign + 5 digits + NUL */
    uint8_t n = 0u;
    char    c;

    do { c = ASCII_GETC(); } while (c == ' ' || c == '\t');

    if (c == '-' || c == '+') { buf[n++] = c; c = ASCII_GETC(); }

    while (n < 6u && c >= '0' && c <= '9') {
        buf[n++] = c;
        c = ASCII_GETC();
    }
    buf[n] = '\0';
    return ascii_parse_i16(buf, out);
}

uint8_t ascii_read_x16(uint16_t *out)
{
    char    buf[7];             /* optional "0x" + 4 hex digits + NUL */
    uint8_t n = 0u;
    char    c;
    uint8_t nib;

    do { c = ASCII_GETC(); } while (c == ' ' || c == '\t');

    /* Absorb optional 0x prefix into buf so ascii_parse_x16 sees it */
    if (c == '0') {
        buf[n++] = c; c = ASCII_GETC();
        if (c == 'x' || c == 'X') { buf[n++] = c; c = ASCII_GETC(); }
    }

    nib = hexval(c);
    while (n < 6u && nib != 0xFFu) {
        buf[n++] = c;
        c = ASCII_GETC();
        nib = hexval(c);
    }
    buf[n] = '\0';
    return ascii_parse_x16(buf, out);
}

#endif /* ASCII_NO_INPUT */
