/* cbuf.c -- portable character display buffer library implementation
 * See cbuf.h for configuration, usage, and porting notes.
 *
 * Design notes:
 *   All state is in six static variables.  No heap, no stdlib.
 *   scroll_up uses a plain byte loop -- no memmove, no string.h.
 *   cbuf_default_table is the identity map (ASCII passthrough) by default.
 *   Override it by defining your own cbuf_default_table[] in your .c file.
 */

#include "cbuf.h"

/* =======================================================================
 * DEFAULT TRANSLATION TABLE
 *
 * Identity map -- output equals input.  Override this for your target's
 * character set by defining cbuf_default_table in your own .c file.
 *
 * For a CoCo target: map ASCII codes to CoCo screen codes.
 * For a VZ200 target: map ASCII codes to VZ200 screen codes.
 * The linker will prefer your definition over this one.
 *
 * With CBUF_TABLE_SIZE 256 the full byte range is covered.
 * With CBUF_TABLE_SIZE 128 only 0x00..0x7F are mapped.
 * ======================================================================= */

#ifndef CBUF_ASCII_NATIVE
const uint8_t cbuf_default_table[CBUF_TABLE_SIZE] = {
#if CBUF_TABLE_SIZE == 256
/*00-07*/  0,  1,  2,  3,  4,  5,  6,  7,
/*08-0F*/  8,  9, 10, 11, 12, 13, 14, 15,
/*10-17*/ 16, 17, 18, 19, 20, 21, 22, 23,
/*18-1F*/ 24, 25, 26, 27, 28, 29, 30, 31,
/*20-27*/ 32, 33, 34, 35, 36, 37, 38, 39,
/*28-2F*/ 40, 41, 42, 43, 44, 45, 46, 47,
/*30-37*/ 48, 49, 50, 51, 52, 53, 54, 55,
/*38-3F*/ 56, 57, 58, 59, 60, 61, 62, 63,
/*40-47*/ 64, 65, 66, 67, 68, 69, 70, 71,
/*48-4F*/ 72, 73, 74, 75, 76, 77, 78, 79,
/*50-57*/ 80, 81, 82, 83, 84, 85, 86, 87,
/*58-5F*/ 88, 89, 90, 91, 92, 93, 94, 95,
/*60-67*/ 96, 97, 98, 99,100,101,102,103,
/*68-6F*/104,105,106,107,108,109,110,111,
/*70-77*/112,113,114,115,116,117,118,119,
/*78-7F*/120,121,122,123,124,125,126,127,
/*80-87*/128,129,130,131,132,133,134,135,
/*88-8F*/136,137,138,139,140,141,142,143,
/*90-97*/144,145,146,147,148,149,150,151,
/*98-9F*/152,153,154,155,156,157,158,159,
/*A0-A7*/160,161,162,163,164,165,166,167,
/*A8-AF*/168,169,170,171,172,173,174,175,
/*B0-B7*/176,177,178,179,180,181,182,183,
/*B8-BF*/184,185,186,187,188,189,190,191,
/*C0-C7*/192,193,194,195,196,197,198,199,
/*C8-CF*/200,201,202,203,204,205,206,207,
/*D0-D7*/208,209,210,211,212,213,214,215,
/*D8-DF*/216,217,218,219,220,221,222,223,
/*E0-E7*/224,225,226,227,228,229,230,231,
/*E8-EF*/232,233,234,235,236,237,238,239,
/*F0-F7*/240,241,242,243,244,245,246,247,
/*F8-FF*/248,249,250,251,252,253,254,255
#else /* 128 */
/*00-07*/  0,  1,  2,  3,  4,  5,  6,  7,
/*08-0F*/  8,  9, 10, 11, 12, 13, 14, 15,
/*10-17*/ 16, 17, 18, 19, 20, 21, 22, 23,
/*18-1F*/ 24, 25, 26, 27, 28, 29, 30, 31,
/*20-27*/ 32, 33, 34, 35, 36, 37, 38, 39,
/*28-2F*/ 40, 41, 42, 43, 44, 45, 46, 47,
/*30-37*/ 48, 49, 50, 51, 52, 53, 54, 55,
/*38-3F*/ 56, 57, 58, 59, 60, 61, 62, 63,
/*40-47*/ 64, 65, 66, 67, 68, 69, 70, 71,
/*48-4F*/ 72, 73, 74, 75, 76, 77, 78, 79,
/*50-57*/ 80, 81, 82, 83, 84, 85, 86, 87,
/*58-5F*/ 88, 89, 90, 91, 92, 93, 94, 95,
/*60-67*/ 96, 97, 98, 99,100,101,102,103,
/*68-6F*/104,105,106,107,108,109,110,111,
/*70-77*/112,113,114,115,116,117,118,119,
/*78-7F*/120,121,122,123,124,125,126,127
#endif
};
#endif /* CBUF_ASCII_NATIVE */

/* =======================================================================
 * INTERNAL STATE
 * ======================================================================= */

static uint8_t  *cb_ptr;     /* base of display buffer              */
static uint8_t   cb_cols;    /* columns per row                     */
static uint8_t   cb_rows;    /* number of rows                      */
static uint8_t   cb_x;       /* cursor column (0-based)             */
static uint8_t   cb_y;       /* cursor row    (0-based)             */

#ifndef CBUF_NO_SCROLL
static uint8_t   cb_scroll;  /* 0 = hard stop, 1 = auto scroll      */
#endif

#ifdef CBUF_TABLE_POINTER
static const uint8_t *cb_table;
#endif

/* =======================================================================
 * INTERNAL HELPERS
 * ======================================================================= */

/* Translate one character through the active table */
static uint8_t cb_translate(uint8_t c)
{
#ifdef CBUF_ASCII_NATIVE
    return c;
#elif defined(CBUF_TABLE_POINTER)
    return cb_table[c & (uint8_t)(CBUF_TABLE_SIZE - 1)];
#else
    return cbuf_default_table[c & (uint8_t)(CBUF_TABLE_SIZE - 1)];
#endif
}

/* Raw offset of (x, y) into the buffer */
static uint16_t cb_offset(uint8_t x, uint8_t y)
{
    return (uint16_t)y * (uint16_t)cb_cols + (uint16_t)x;
}

/* =======================================================================
 * INIT
 * ======================================================================= */

void cbuf_init(uint8_t *ptr, uint8_t cols, uint8_t rows)
{
    cb_ptr  = ptr;
    cb_cols = cols;
    cb_rows = rows;
    cb_x    = 0;
    cb_y    = 0;
#ifndef CBUF_NO_SCROLL
    cb_scroll = 1;
#endif
#ifdef CBUF_TABLE_POINTER
    cb_table = cbuf_default_table;
#endif
}

/* =======================================================================
 * TRANSLATION TABLE MANAGEMENT
 * ======================================================================= */

#ifdef CBUF_TABLE_POINTER

void cbuf_set_table(const uint8_t *table)
{
    cb_table = table;
}

void cbuf_copy_table(uint8_t *dst)
{
    /* Plain byte loop -- no string.h, no memcpy */
#ifndef CBUF_ASCII_NATIVE
    uint16_t i;
    for (i = 0; i < (uint16_t)CBUF_TABLE_SIZE; i++) {
        dst[i] = cbuf_default_table[i];
    }
#else
    /* Nothing to copy -- ASCII native has no table */
    (void)dst;
#endif
}

#endif /* CBUF_TABLE_POINTER */

/* =======================================================================
 * SCROLL CONTROL
 * ======================================================================= */

#ifndef CBUF_NO_SCROLL

void cbuf_set_scroll(uint8_t on)
{
    cb_scroll = on ? 1u : 0u;
}

void cbuf_scroll_up(uint8_t fill)
{
    /* Shift rows 1..rows-1 up to rows 0..rows-2.
     * Plain byte loop -- portable, no memmove needed.               */
    uint16_t total = (uint16_t)(cb_rows - 1u) * (uint16_t)cb_cols;
    uint16_t i;
    for (i = 0; i < total; i++) {
        cb_ptr[i] = cb_ptr[i + cb_cols];
    }

    /* Clear the new bottom row */
    {
        uint16_t base = (uint16_t)(cb_rows - 1u) * (uint16_t)cb_cols;
        for (i = 0; i < (uint16_t)cb_cols; i++) {
            cb_ptr[base + i] = fill;
        }
    }

    /* Pull cursor up one row, clamped */
    if (cb_y > 0) cb_y--;
}

#endif /* CBUF_NO_SCROLL */

/* =======================================================================
 * CURSOR
 * ======================================================================= */

int8_t cbuf_goto(uint8_t x, uint8_t y)
{
    int8_t  rc  = CBUF_OK;

    if (x >= cb_cols) { x = cb_cols - 1u; rc = CBUF_OVERFLOW; }
    if (y >= cb_rows) { y = cb_rows - 1u; rc = CBUF_OVERFLOW; }

    cb_x = x;
    cb_y = y;
    return rc;
}

uint8_t cbuf_get_x(void) { return cb_x; }
uint8_t cbuf_get_y(void) { return cb_y; }

/* =======================================================================
 * READ
 * ======================================================================= */

uint8_t cbuf_getc(void)
{
    return cb_ptr[cb_offset(cb_x, cb_y)];
}

uint8_t cbuf_read_eol(uint8_t fill, char *dst, uint8_t maxlen)
{
    /* Find the last non-fill byte on the current row */
    uint8_t  end = cb_cols;
    uint8_t  i;
    uint8_t  n = 0;

    if (dst == 0 || maxlen == 0u) return 0u;

    /* Scan row right-to-left to find meaningful content end */
    while (end > cb_x) {
        if (cb_ptr[cb_offset(end - 1u, cb_y)] != fill) break;
        end--;
    }

    /* Copy from cursor to end, up to maxlen-1 */
    for (i = cb_x; i < end && n < (uint8_t)(maxlen - 1u); i++) {
        dst[n++] = (char)cb_ptr[cb_offset(i, cb_y)];
    }
    dst[n] = '\0';
    return n;
}

/* =======================================================================
 * WRITE
 * ======================================================================= */

int8_t cbuf_put_raw(uint8_t x, uint8_t y, uint8_t code)
{
    if (x >= cb_cols || y >= cb_rows) return CBUF_OVERFLOW;
    cb_ptr[cb_offset(x, y)] = code;
    return CBUF_OK;
}

uint8_t cbuf_get_raw(uint8_t x, uint8_t y)
{
    if (x >= cb_cols || y >= cb_rows) return 0;
    return cb_ptr[cb_offset(x, y)];
}

int8_t cbuf_putc(uint8_t c)
{
    int8_t rc = CBUF_OK;

    /* If cursor is already out of bounds from a previous operation,
     * attempt recovery before writing.                              */
    if (cb_x >= cb_cols || cb_y >= cb_rows) {
#ifndef CBUF_NO_SCROLL
        if (cb_scroll) {
            cbuf_scroll_up(' ');
            cb_x = 0;
            rc = CBUF_SCROLLED;
        } else
#endif
        return CBUF_OVERFLOW;
    }

    /* Write translated character at current (valid) position */
    cb_ptr[cb_offset(cb_x, cb_y)] = cb_translate(c);

    /* Advance cursor */
    cb_x++;
    if (cb_x >= cb_cols) {
        cb_x = 0;
        cb_y++;
        if (cb_y >= cb_rows) {
#ifndef CBUF_NO_SCROLL
            if (cb_scroll) {
                cbuf_scroll_up(' ');
                /* cb_y adjusted inside scroll_up */
                rc = CBUF_SCROLLED;
            } else {
                /* Hard stop: cursor stays at last valid position.
                 * The character was written -- the ADVANCE overflows. */
                cb_y = cb_rows - 1u;
                cb_x = cb_cols - 1u;
                rc = CBUF_OVERFLOW;
            }
#else
            /* CBUF_NO_SCROLL: same hard stop behaviour */
            cb_y = cb_rows - 1u;
            cb_x = cb_cols - 1u;
            rc = CBUF_OVERFLOW;
#endif
        }
    }

    return rc;
}

int8_t cbuf_puts(const char *s)
{
    int8_t rc = CBUF_OK;
    int8_t r;

    if (s == 0) return CBUF_OK;

    while (*s != '\0') {
        r = cbuf_putc((uint8_t)*s);
        if (r == CBUF_OVERFLOW) return CBUF_OVERFLOW;
        if (r == CBUF_SCROLLED) rc = CBUF_SCROLLED;
        s++;
    }
    return rc;
}

void cbuf_clear(uint8_t fill)
{
    uint16_t total = (uint16_t)cb_cols * (uint16_t)cb_rows;
    uint16_t i;
    for (i = 0; i < total; i++) {
        cb_ptr[i] = fill;
    }
    cb_x = 0;
    cb_y = 0;
}

void cbuf_clear_line(uint8_t y, uint8_t fill)
{
    uint16_t base;
    uint8_t  i;
    if (y >= cb_rows) return;
    base = cb_offset(0, y);
    for (i = 0; i < cb_cols; i++) {
        cb_ptr[base + i] = fill;
    }
}

void cbuf_clear_eol(uint8_t fill)
{
    uint8_t i;
    for (i = cb_x; i < cb_cols; i++) {
        cb_ptr[cb_offset(i, cb_y)] = fill;
    }
}
