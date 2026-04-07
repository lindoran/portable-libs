/* cbuf.h -- portable character display buffer library
 *
 * Standard : C89/C90
 * Targets  : Z80, AVR, 6809, RP2040, and other 8/16-bit systems
 *
 * Companion to ascii_io.h and pmath.h.  Manages a flat byte array as a
 * character display.  Each byte is one screen position.  The library tracks
 * a soft cursor and writes translated character codes into the buffer.
 * Hardware rendering is entirely the application's responsibility.
 *
 * The library is a palette and canvas -- it provides primitives.
 * Backspace, line editing, blinking cursors, double buffering -- those
 * are brushes.  Build them yourself from what's here.
 *
 * -----------------------------------------------------------------------
 * INCLUDE ORDER
 * -----------------------------------------------------------------------
 *  #include "ascii_io.h"   -- first: defines ASCII_PUTC hook
 *  #include "pmath.h"      -- second (if used)
 *  #include "cbuf.h"       -- third
 *
 *  To route all ascii_io output through cbuf, define before including:
 *    #define ASCII_PUTC(c)  cbuf_putc((uint8_t)(c))
 *  The cast to uint8_t is required for correct high-byte handling on
 *  targets where char is signed (all characters >= 0x80 would otherwise
 *  sign-extend before the table lookup).
 *
 * -----------------------------------------------------------------------
 * FEATURE DROP FLAGS  (define before including to shrink the binary)
 * -----------------------------------------------------------------------
 *  CBUF_ASCII_NATIVE  -- no translation table at all.  Characters are
 *                        written directly to the buffer as-is.  Use on
 *                        targets where the display ROM is ASCII-compatible.
 *                        Drops all table code and data.
 *                        Cannot be combined with CBUF_TABLE_POINTER.
 *
 *  CBUF_NO_SCROLL     -- omit scroll_up at compile time.  cbuf_putc()
 *                        returns CBUF_OVERFLOW on out-of-bounds, always.
 *                        Saves the scroll loop and the runtime flag byte.
 *
 *  CBUF_TABLE_POINTER -- the translation table is a runtime pointer.
 *                        Call cbuf_set_table() after cbuf_init() to point
 *                        at any table.  Enables cbuf_copy_table() to clone
 *                        the ROM default into RAM for patching.
 *                        Cannot be combined with CBUF_ASCII_NATIVE.
 *
 *  Integer types are provided by portable_types.h.  To suppress
 *  <stdint.h> on toolchains that lack it, define PORTABLE_NO_STDINT
 *  before including any library header.
 *
 * -----------------------------------------------------------------------
 * TABLE SIZE
 * -----------------------------------------------------------------------
 *  CBUF_TABLE_SIZE 128  -- (default) table covers 0x00..0x7F.  Input
 *                          bytes are masked to 0x7F before lookup.
 *
 *  CBUF_TABLE_SIZE 256  -- full byte range 0x00..0xFF.  Input passed
 *                          through as uint8_t directly.  Costs 256 bytes
 *                          ROM for the default table vs 128.  Useful on
 *                          targets where the upper 128 positions are
 *                          graphics, inverse video, or national characters
 *                          (e.g. CoCo, VZ200).
 *
 *  To set: #define CBUF_TABLE_SIZE 256
 *
 * -----------------------------------------------------------------------
 * SCREEN CODES
 * -----------------------------------------------------------------------
 *  The default ROM table maps ASCII input to ASCII output (identity).
 *  Override cbuf_default_table[] in your own .c file for your target's
 *  character set, or use CBUF_TABLE_POINTER to supply one at runtime.
 *
 *  With CBUF_TABLE_SIZE 256 and CBUF_TABLE_POINTER:
 *    static uint8_t my_table[256];
 *    cbuf_copy_table(my_table);         -- clone ROM default into RAM
 *    my_table['A'] = COCO_SCREEN_A;    -- patch individual entries
 *    cbuf_set_table(my_table);          -- repoint library to RAM table
 *
 * -----------------------------------------------------------------------
 * SCROLL BEHAVIOUR
 * -----------------------------------------------------------------------
 *  When scroll is ON (runtime, default after cbuf_init):
 *    cbuf_putc() at end of last row shifts all rows up one, clears the
 *    bottom row with space (0x20), and returns CBUF_SCROLLED.
 *
 *  When scroll is OFF (runtime):
 *    cbuf_putc() at end of buffer returns CBUF_OVERFLOW.
 *    cbuf_goto() to an out-of-bounds position clamps and returns
 *    CBUF_OVERFLOW.
 *
 *  Scroll can be toggled at any time with cbuf_set_scroll().
 *  Dropped entirely at compile time with CBUF_NO_SCROLL.
 *
 * -----------------------------------------------------------------------
 * RETURN CODES
 * -----------------------------------------------------------------------
 *  CBUF_OK        ( 0)  operation succeeded
 *  CBUF_SCROLLED  ( 1)  write succeeded but caused a scroll
 *  CBUF_OVERFLOW  (-1)  out of bounds and scroll is off (or dropped)
 *
 * -----------------------------------------------------------------------
 * K&R PORTING NOTE
 * -----------------------------------------------------------------------
 *  Written in C89.  No string.h -- scroll_up uses a plain byte loop.
 *  No float types used anywhere.  Integer types from portable_types.h.
 *  To suppress stdint.h define PORTABLE_NO_STDINT and provide typedefs
 *  manually in portable_types.h.
 *
 * -----------------------------------------------------------------------
 * QUICK START
 * -----------------------------------------------------------------------
 *  #define ASCII_PUTC(c)  cbuf_putc((uint8_t)(c))
 *  #include "ascii_io.h"
 *  #include "cbuf.h"
 *
 *  static uint8_t screen[32 * 16];
 *
 *  cbuf_init(screen, 32, 16);
 *  cbuf_clear(' ');
 *  cbuf_set_scroll(1);
 *
 *  cbuf_goto(0, 0);
 *  ascii_puts("Hello, world");   -- renders to display buffer
 */

#ifndef CBUF_H
#define CBUF_H

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * INTEGER TYPES
 * ----------------------------------------------------------------------- */
#include "portable_types.h"

/* -----------------------------------------------------------------------
 * TABLE SIZE
 * ----------------------------------------------------------------------- */

#ifndef CBUF_TABLE_SIZE
#  define CBUF_TABLE_SIZE  128
#endif

#if (CBUF_TABLE_SIZE != 128) && (CBUF_TABLE_SIZE != 256)
#  error "CBUF_TABLE_SIZE must be 128 or 256"
#endif

/* -----------------------------------------------------------------------
 * CONFLICT GUARD
 * ----------------------------------------------------------------------- */

#if defined(CBUF_ASCII_NATIVE) && defined(CBUF_TABLE_POINTER)
#  error "CBUF_ASCII_NATIVE and CBUF_TABLE_POINTER are mutually exclusive."
#endif

/* -----------------------------------------------------------------------
 * RETURN CODES
 * ----------------------------------------------------------------------- */

#define CBUF_OK        ( 0)
#define CBUF_SCROLLED  ( 1)
#define CBUF_OVERFLOW  (-1)

/* =======================================================================
 * TRANSLATION TABLE
 *
 * cbuf_default_table is defined in cbuf.c as an identity map.
 * Override it by defining your own array with the same name in your
 * application's .c file -- the linker will prefer your definition.
 * Or use CBUF_TABLE_POINTER for runtime switching without relinking.
 * ======================================================================= */

#ifndef CBUF_ASCII_NATIVE
extern const uint8_t cbuf_default_table[CBUF_TABLE_SIZE];
#endif

/* =======================================================================
 * INIT
 * ======================================================================= */

/*
 * cbuf_init -- bind the library to a display buffer.
 *
 *   ptr  : pointer to the first byte of the display buffer.
 *          Must be at least cols*rows bytes.
 *          On memory-mapped targets this is the screen RAM address.
 *   cols : number of character columns (e.g. 32, 40, 80)
 *   rows : number of character rows    (e.g. 16, 24, 25)
 *
 * Cursor is reset to (0, 0).  Scroll defaults to ON.
 * Buffer contents are not cleared -- call cbuf_clear() explicitly.
 */
void cbuf_init(uint8_t *ptr, uint8_t cols, uint8_t rows);

/* =======================================================================
 * TRANSLATION TABLE MANAGEMENT
 * ======================================================================= */

#ifdef CBUF_TABLE_POINTER

/*
 * cbuf_set_table -- repoint the runtime translation table.
 *   table : pointer to a CBUF_TABLE_SIZE byte array (ROM or RAM).
 *           The pointer is stored; the array is not copied.
 */
void cbuf_set_table(const uint8_t *table);

/*
 * cbuf_copy_table -- copy the ROM default table into a RAM buffer.
 *   dst : caller-supplied buffer of at least CBUF_TABLE_SIZE bytes.
 *
 * Pattern for patching:
 *   static uint8_t my_table[CBUF_TABLE_SIZE];
 *   cbuf_copy_table(my_table);         -- clone ROM default
 *   my_table['a'] = MY_SCREEN_CODE;   -- patch entries
 *   cbuf_set_table(my_table);          -- repoint to RAM
 */
void cbuf_copy_table(uint8_t *dst);

#endif /* CBUF_TABLE_POINTER */

/* =======================================================================
 * SCROLL CONTROL
 * ======================================================================= */

#ifndef CBUF_NO_SCROLL

/*
 * cbuf_set_scroll -- enable or disable auto-scroll at runtime.
 *   on : 1 = scroll on overflow, 0 = return CBUF_OVERFLOW on overflow.
 *   Default after cbuf_init is scroll ON.
 */
void cbuf_set_scroll(uint8_t on);

/*
 * cbuf_scroll_up -- scroll the entire display up one row.
 *   fill : raw screen code to fill the new bottom row (not translated).
 * Cursor row is decremented by one, clamped to 0.
 */
void cbuf_scroll_up(uint8_t fill);

#endif /* CBUF_NO_SCROLL */

/* =======================================================================
 * CURSOR
 * ======================================================================= */

/*
 * cbuf_goto -- move the soft cursor to (x, y).
 *   x : column  (0-based, 0 = leftmost)
 *   y : row     (0-based, 0 = top)
 * Returns CBUF_OK if in bounds, CBUF_OVERFLOW if clamped.
 * Cursor always ends up at a valid position regardless of return value.
 */
int8_t cbuf_goto(uint8_t x, uint8_t y);

uint8_t cbuf_get_x(void);   /* current cursor column */
uint8_t cbuf_get_y(void);   /* current cursor row    */

/* =======================================================================
 * READ
 * ======================================================================= */

/*
 * cbuf_getc -- read the raw screen byte at the current cursor position.
 *   Returns the byte as stored (raw screen code, not ASCII).
 *   Does not move the cursor.
 */
uint8_t cbuf_getc(void);

/*
 * cbuf_read_eol -- copy raw screen bytes from cursor to end of meaningful
 *   content on the current row into dst.
 *   Trailing fill characters are stripped.
 *   fill   : the fill character used to pad the row (raw screen code).
 *   dst    : caller buffer, must be at least cols bytes.
 *   maxlen : maximum bytes to copy including NUL terminator.
 *   Returns number of bytes copied (not counting NUL).
 *   dst is always NUL-terminated.  Does not move the cursor.
 */
uint8_t cbuf_read_eol(uint8_t fill, char *dst, uint8_t maxlen);

/* =======================================================================
 * WRITE
 * ======================================================================= */

/*
 * cbuf_putc -- translate c and write to cursor position, then advance.
 *
 *   Translation:
 *     CBUF_ASCII_NATIVE   : c written directly, no table.
 *     default/ROM table   : c masked to (CBUF_TABLE_SIZE-1), looked up.
 *     CBUF_TABLE_POINTER  : same lookup via runtime pointer.
 *
 *   Advance and overflow:
 *     Column increments.  At end of row, wraps to column 0 of next row.
 *     At end of last row:
 *       scroll ON  : calls cbuf_scroll_up(' '), returns CBUF_SCROLLED.
 *       scroll OFF : does not write beyond, returns CBUF_OVERFLOW.
 *       CBUF_NO_SCROLL : always CBUF_OVERFLOW at boundary.
 *
 *   Returns CBUF_OK, CBUF_SCROLLED, or CBUF_OVERFLOW.
 */
int8_t cbuf_putc(uint8_t c);

/*
 * cbuf_puts -- write a NUL-terminated string via cbuf_putc.
 *   Returns CBUF_OK, CBUF_SCROLLED, or CBUF_OVERFLOW.
 *   Stops at first CBUF_OVERFLOW -- does not continue after rejection.
 */
int8_t cbuf_puts(const char *s);

/*
 * cbuf_clear -- fill the entire buffer with fill.
 *   fill : raw screen code (not translated).
 *   Cursor is reset to (0, 0).
 */
void cbuf_clear(uint8_t fill);

/*
 * cbuf_clear_line -- fill one row with fill.
 *   y    : row to clear (0-based).  Does nothing if out of bounds.
 *   fill : raw screen code.  Does not move the cursor.
 */
void cbuf_clear_line(uint8_t y, uint8_t fill);

/*
 * cbuf_clear_eol -- fill from cursor to end of current row with fill.
 *   fill : raw screen code.  Does not move the cursor.
 */
void cbuf_clear_eol(uint8_t fill);

/*
 * cbuf_put_raw -- write a raw screen code at (x, y).
 *   No translation.  No cursor movement.  No scroll.
 *   Useful for graphics characters, inverse video, cursor blink.
 *   Returns CBUF_OK or CBUF_OVERFLOW if out of bounds.
 */
int8_t cbuf_put_raw(uint8_t x, uint8_t y, uint8_t code);

/*
 * cbuf_get_raw -- read the raw screen code at (x, y).
 *   No cursor movement.  Returns 0 if out of bounds.
 */
uint8_t cbuf_get_raw(uint8_t x, uint8_t y);

#ifdef __cplusplus
}
#endif

#endif /* CBUF_H */
